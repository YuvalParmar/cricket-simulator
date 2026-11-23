#include "PhoneBatReceiverComponent.h"
#include "PhoneBatBinaryProtocol.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Misc/DateTime.h"

UPhoneBatReceiverComponent::UPhoneBatReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Every frame

    LatestPacket = FPhoneBatDataPacket();
    PreviousPacket = FPhoneBatDataPacket();
    CalibrationData = FPhoneBatCalibrationData();
    ConnectionInfo = FPhoneBatConnectionInfo();

    PacketHistory.SetNumZeroed(0);
    PacketHistory.Reserve(MAX_HISTORY_SIZE);

    JitterBuffer.SetNumZeroed(0);
    JitterBuffer.Reserve(MIN_JITTER_BUFFER_SIZE);
    CurrentJitterBufferSize = MIN_JITTER_BUFFER_SIZE;

    LatencySamples.SetNumZeroed(0);
    LatencySamples.Reserve(LATENCY_SAMPLE_COUNT);

    CalibrationSamples.SetNumZeroed(0);
    CalibrationSamples.Reserve(CALIBRATION_SAMPLE_COUNT);
}

void UPhoneBatReceiverComponent::BeginPlay()
{
    Super::BeginPlay();

    // Start listening by default
    StartListening(ListenPort, EPhoneBatConnectionType::UDP);
}

void UPhoneBatReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopListening();
    StopDeviceDiscovery();

    Super::EndPlay(EndPlayReason);
}

void UPhoneBatReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!ConnectionInfo.bIsConnected)
    {
        return;
    }

    // Check for packet timeout
    float CurrentTime = GetCurrentTimestampSeconds();
    float LastPacketAge = CurrentTime - (LatestPacket.Timestamp / 1000000.0f);

    if (LastPacketAge * 1000.0f > PacketTimeoutMS)
    {
        UE_LOG(LogTemp, Warning, TEXT("Phone Bat packet timeout: %.1fms"), LastPacketAge * 1000.0f);
        ConnectionInfo.bIsConnected = false;
        OnPhoneBatConnectionChanged.Broadcast(false);
    }

    // Update connection stats
    UpdateConnectionStats();

    // Process jitter buffer
    if (JitterBuffer.Num() > 0)
    {
        FPhoneBatDataPacket InterpolatedPacket = GetInterpolatedPacket();
        if (InterpolatedPacket.PacketSequence > LatestPacket.PacketSequence)
        {
            // T5: PC prediction complete time (after interpolation/prediction)
            InterpolatedPacket.PcPredictionCompleteTimeUs = GetCurrentTimestampUSStatic();

            PreviousPacket = LatestPacket;
            LatestPacket = InterpolatedPacket;

            // T6: Game thread tick time (when bat state updated)
            LatestPacket.GameThreadUpdateTimeUs = GetCurrentTimestampUSStatic();

            // Add to history
            PacketHistory.Add(LatestPacket);
            if (PacketHistory.Num() > MAX_HISTORY_SIZE)
            {
                PacketHistory.RemoveAt(0);
            }

            // Broadcast data received event
            OnPhoneBatDataReceived.Broadcast(LatestPacket);

            // Check for swing phase change
            if (LatestPacket.SwingPhase != PreviousPacket.SwingPhase)
            {
                HandleSwingPhaseChange(LatestPacket.SwingPhase);
            }
        }
    }

    // Handle calibration
    if (bIsCalibrating)
    {
        // Calibration samples are collected in ProcessPacket
        if (CalibrationSamples.Num() >= CALIBRATION_SAMPLE_COUNT)
        {
            FinalizeCalibration();
        }
    }
}

bool UPhoneBatReceiverComponent::StartListening(int32 Port, EPhoneBatConnectionType Type)
{
    if (Type == EPhoneBatConnectionType::UDP || Type == EPhoneBatConnectionType::Auto)
    {
        // Create UDP socket
        FIPv4Address LocalAddress(0, 0, 0, 0); // Listen on all interfaces
        LocalEndpoint = FIPv4Endpoint(LocalAddress, Port);

        FSocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (!SocketSubsystem)
        {
            UE_LOG(LogTemp, Error, TEXT("PhoneBat: Socket subsystem not available"));
            return false;
        }

        UDPSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PhoneBatUDP"), false);
        if (!UDPSocket.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("PhoneBat: Failed to create UDP socket"));
            return false;
        }

        // Bind socket
        if (!UDPSocket->Bind(*LocalEndpoint.ToInternetAddr()))
        {
            UE_LOG(LogTemp, Error, TEXT("PhoneBat: Failed to bind UDP socket to port %d"), Port);
            UDPSocket.Reset();
            return false;
        }

        // Set non-blocking
        UDPSocket->SetNonBlocking(true);

        // Set receive buffer size
        int32 BufferSize = 65536;
        UDPSocket->SetReceiveBufferSize(BufferSize, BufferSize);

        // Create receive thread with TimeCritical priority for minimal latency
        ReceiveRunnable = new FPhoneBatReceiveRunnable(this);
        ReceiveThread = FRunnableThread::Create(ReceiveRunnable, TEXT("PhoneBatReceiveThread"), 128 * 1024, TPri_TimeCritical);

        ConnectionInfo.Port = Port;
        ConnectionInfo.ConnectionType = EPhoneBatConnectionType::UDP;
        ConnectionInfo.bIsConnected = false; // Will be set true when first packet received

        UE_LOG(LogTemp, Log, TEXT("PhoneBat: Listening on UDP port %d"), Port);
        return true;
    }

    return false;
}

void UPhoneBatReceiverComponent::StopListening()
{
    // Stop receive thread
    if (ReceiveRunnable)
    {
        ReceiveRunnable->Stop();
        ReceiveRunnable = nullptr;
    }

    if (ReceiveThread)
    {
        ReceiveThread->WaitForCompletion();
        delete ReceiveThread;
        ReceiveThread = nullptr;
    }

    // Close socket
    if (UDPSocket.IsValid())
    {
        UDPSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(UDPSocket.Get());
        UDPSocket.Reset();
    }

    ConnectionInfo.bIsConnected = false;
    OnPhoneBatConnectionChanged.Broadcast(false);
}

bool UPhoneBatReceiverComponent::ConnectToDevice(const FPhoneBatConnectionInfo& DeviceInfo)
{
    // For UDP, we just set the remote endpoint
    if (UDPSocket.IsValid())
    {
        FIPv4Address RemoteAddr;
        if (FIPv4Address::Parse(DeviceInfo.IPAddress, RemoteAddr))
        {
            RemoteEndpoint = FIPv4Endpoint(RemoteAddr, DeviceInfo.Port);
            ConnectionInfo = DeviceInfo;
            ConnectionInfo.bIsConnected = true;
            OnPhoneBatConnectionChanged.Broadcast(true);
            return true;
        }
    }

    return false;
}

void UPhoneBatReceiverComponent::Disconnect()
{
    StopListening();
    ConnectionInfo = FPhoneBatConnectionInfo();
}

void UPhoneBatReceiverComponent::StartDeviceDiscovery(float TimeoutSeconds)
{
    // Broadcast discovery packet
    if (!BroadcastSocket.IsValid())
    {
        FSocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            BroadcastSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PhoneBatBroadcast"), false);
            if (BroadcastSocket.IsValid())
            {
                BroadcastSocket->SetNonBlocking(true);
                BroadcastSocket->SetBroadcast(true);
            }
        }
    }

    if (BroadcastSocket.IsValid())
    {
        // Send discovery packet
        FPhoneBatDataPacket DiscoveryPacket;
        DiscoveryPacket.Timestamp = GetCurrentTimestampUS();
        DiscoveryPacket.PacketSequence = 0xFFFFFFFF; // Special sequence for discovery

        TArray<uint8> Data;
        SerializePacket(DiscoveryPacket, Data);

        FIPv4Address BroadcastAddr(255, 255, 255, 255);
        FIPv4Endpoint BroadcastEndpoint(BroadcastAddr, ListenPort);

        int32 BytesSent = 0;
        BroadcastSocket->SendTo(Data.GetData(), Data.Num(), BytesSent, *BroadcastEndpoint.ToInternetAddr());

        UE_LOG(LogTemp, Log, TEXT("PhoneBat: Discovery packet sent"));

        // Start timeout timer
        FTimerHandle DiscoveryTimer;
        GetWorld()->GetTimerManager().SetTimer(DiscoveryTimer, [this]()
        {
            StopDeviceDiscovery();
        }, TimeoutSeconds, false);
    }
}

void UPhoneBatReceiverComponent::StopDeviceDiscovery()
{
    if (BroadcastSocket.IsValid())
    {
        BroadcastSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(BroadcastSocket.Get());
        BroadcastSocket.Reset();
    }
}

void UPhoneBatReceiverComponent::StartCalibration()
{
    bIsCalibrating = true;
    CalibrationSamples.Empty();
    UE_LOG(LogTemp, Log, TEXT("PhoneBat: Calibration started"));
}

void UPhoneBatReceiverComponent::ApplyCalibration(const FPhoneBatCalibrationData& CalibrationDataIn)
{
    CalibrationData = CalibrationDataIn;
    CalibrationData.bIsValid = true;
    CalibrationData.CalibrationTimestamp = GetCurrentTimestampUS();
    bIsCalibrating = false;
    OnPhoneBatCalibrationComplete.Broadcast(CalibrationData);
    UE_LOG(LogTemp, Log, TEXT("PhoneBat: Calibration applied"));
}

void UPhoneBatReceiverComponent::ResetCalibration()
{
    CalibrationData = FPhoneBatCalibrationData();
    bIsCalibrating = false;
}

FTransform UPhoneBatReceiverComponent::GetPredictedBatTransform(float TimeAhead) const
{
    if (!bEnablePrediction || PacketHistory.Num() < 2)
    {
        return LatestPacket.BatTransform;
    }

    return PredictPacket(TimeAhead).BatTransform;
}

FVector UPhoneBatReceiverComponent::GetPredictedBatVelocity(float TimeAhead) const
{
    if (!bEnablePrediction || PacketHistory.Num() < 2)
    {
        return LatestPacket.LinearVelocity;
    }

    return PredictPacket(TimeAhead).LinearVelocity;
}

void UPhoneBatReceiverComponent::ProcessPacket(const uint8* Data, int32 Size, const FIPv4Endpoint& Sender)
{
    if (Size < 24) // Minimum binary header size
    {
        return;
    }

    // T3: PC receive time (microseconds since epoch)
    int64 PcReceiveTimeUs = GetCurrentTimestampUSStatic();

    // Try binary format first (faster), fallback to JSON
    FPhoneBatDataPacket Packet = DeserializeBinaryPacket(Data, Size);

    // T3: Record PC receive time
    Packet.PcReceiveTimeUs = PcReceiveTimeUs;

    // Validate packet
    if (Packet.PacketSequence == 0xFFFFFFFF)
    {
        // Discovery response
        FPhoneBatConnectionInfo DeviceInfo;
        DeviceInfo.IPAddress = Sender.Address.ToString();
        DeviceInfo.Port = Sender.Port;
        DeviceInfo.ConnectionType = EPhoneBatConnectionType::UDP;
        DeviceInfo.bIsConnected = true;
        DeviceInfo.LatencyMS = 0.0f; // Will be measured
        OnDeviceDiscovered(DeviceInfo);
        return;
    }

    // Update latency estimation
    UpdateLatencyEstimation(Packet.Timestamp);

    // Update connection info
    if (!ConnectionInfo.bIsConnected)
    {
        ConnectionInfo.IPAddress = Sender.Address.ToString();
        ConnectionInfo.Port = Sender.Port;
        ConnectionInfo.bIsConnected = true;
        OnPhoneBatConnectionChanged.Broadcast(true);
    }

    // Add to jitter buffer
    UpdateJitterBuffer(Packet);
}

FPhoneBatDataPacket UPhoneBatReceiverComponent::DeserializePacket(const uint8* Data, int32 Size)
{
    FPhoneBatDataPacket Packet;
    FMemoryReader Reader(Data, Size);
    Reader << Packet.Timestamp;
    Reader << Packet.BatTransform;
    Reader << Packet.LinearVelocity;
    Reader << Packet.AngularVelocity;
    Reader << Packet.LinearAcceleration;
    Reader << Packet.SwingPhase;
    Reader << Packet.SwingProgress;
    Reader << Packet.PredictedContactTime;
    Reader << Packet.ContactConfidence;
    Reader << Packet.BatSpeed;
    Reader << Packet.WristRotationAtContact;
    Reader << Packet.ShotTypeClassification;
    Reader << Packet.ClassificationConfidence;
    Reader << Packet.PacketSequence;
    Reader << Packet.BatteryLevel;
    Reader << Packet.Temperature;
    return Packet;
}

FPhoneBatDataPacket UPhoneBatReceiverComponent::DeserializeBinaryPacket(const uint8* Data, int32 Size)
{
    FPhoneBatDataPacket Packet;

    // Try to detect if it's binary format (has magic number)
    if (Size >= static_cast<int32>(sizeof(FPhoneBatBinaryHeader)))
    {
        const FPhoneBatBinaryHeader* Header = reinterpret_cast<const FPhoneBatBinaryHeader*>(Data);
        if (Header->MagicNumber == PhoneBatBinaryProtocol::MAGIC_NUMBER &&
            Header->Version == PhoneBatBinaryProtocol::CURRENT_VERSION)
        {
            // Binary format - parse header + payload
            PhoneBatBinaryProtocol::EPacketType PacketType;
            uint32 SequenceNumber;
            uint64 Timestamp, ServerTimestamp;
            TArray<uint8> PayloadData;

            if (FPhoneBatBinarySerializer::DeserializePacket(Data, Size, PacketType, SequenceNumber, Timestamp, ServerTimestamp, PayloadData))
            {
                // Parse based on packet type
                switch (PacketType)
                {
                    case PhoneBatBinaryProtocol::EPacketType::SensorFrame:
                    {
                        FPhoneBatSensorFramePayload SensorFrame;
                        if (FPhoneBatBinarySerializer::DeserializeSensorFrame(PayloadData.GetData(), PayloadData.Num(), SensorFrame))
                        {
                            // Convert to data packet
                            Packet.Timestamp = SensorFrame.Timestamp;
                            Packet.PacketSequence = SequenceNumber;
                            Packet.BatTransform = FTransform(SensorFrame.Orientation, SensorFrame.Gravity * 100.0f);
                            Packet.LinearVelocity = FVector::ZeroVector; // Not in sensor frame
                            Packet.AngularVelocity = SensorFrame.Gyroscope;
                            Packet.LinearAcceleration = SensorFrame.LinearAcceleration * 100.0f; // Convert to cm/s^2
                            Packet.SwingPhase = EPhoneBatSwingPhase::Idle; // Not in sensor frame
                            Packet.SwingProgress = 0.0f;
                            Packet.PredictedContactTime = 0.0f;
                            Packet.ContactConfidence = SensorFrame.Confidence;
                            Packet.BatSpeed = 0.0f;
                            Packet.WristRotationAtContact = 0.0f;
                            Packet.ShotTypeClassification = 0;
                            Packet.ClassificationConfidence = 0.0f;
                            Packet.BatteryLevel = 1.0f;
                            Packet.Temperature = 25.0f;

                            // Copy latency timestamps from phone side
                            Packet.PhoneSensorReadTimeUs = SensorFrame.PhoneSensorReadTimeUs;
                            Packet.PhoneFusionCompleteTimeUs = SensorFrame.PhoneFusionCompleteTimeUs;
                            Packet.PhoneSendTimeUs = SensorFrame.PhoneSendTimeUs;

                            return Packet;
                        }
                        break;
                    }
                    case PhoneBatBinaryProtocol::EPacketType::SwingPhaseUpdate:
                    {
                        FPhoneBatSwingPhasePayload SwingPhase;
                        if (FPhoneBatBinarySerializer::DeserializeSwingPhase(PayloadData.GetData(), PayloadData.Num(), SwingPhase))
                        {
                            Packet.Timestamp = SwingPhase.Timestamp;
                            Packet.PacketSequence = SequenceNumber;
                            Packet.SwingPhase = SwingPhase.CurrentPhase;
                            Packet.SwingProgress = SwingPhase.PhaseProgress;
                            Packet.BatSpeed = SwingPhase.BatSpeed * 100.0f; // m/s to cm/s
                            Packet.BatVelocity = SwingPhase.BatVelocity * 100.0f;
                            Packet.BatAngularVelocity = SwingPhase.BatAngularVelocity;
                            Packet.ContactConfidence = SwingPhase.Confidence;

                            // Copy latency timestamps from phone side
                            Packet.PhoneSensorReadTimeUs = SwingPhase.PhoneSensorReadTimeUs;
                            Packet.PhoneFusionCompleteTimeUs = SwingPhase.PhoneFusionCompleteTimeUs;
                            Packet.PhoneSendTimeUs = SwingPhase.PhoneSendTimeUs;

                            return Packet;
                        }
                        break;
                    }
                    case PhoneBatBinaryProtocol::EPacketType::SwingContactPredict:
                    {
                        FPhoneBatContactPredictPayload ContactPredict;
                        if (FPhoneBatBinarySerializer::DeserializeContactPredict(PayloadData.GetData(), PayloadData.Num(), ContactPredict))
                        {
                            Packet.Timestamp = ContactPredict.Timestamp;
                            Packet.PacketSequence = SequenceNumber;
                            Packet.PredictedContactTime = static_cast<float>((ContactPredict.PredictedContactTime - Timestamp) / 1000000.0f); // Convert to seconds
                            Packet.PredictedContactPoint = ContactPredict.PredictedContactPoint * 100.0f; // m to cm
                            Packet.ContactConfidence = ContactPredict.Confidence;

                            // Copy latency timestamps from phone side
                            Packet.PhoneSensorReadTimeUs = ContactPredict.PhoneSensorReadTimeUs;
                            Packet.PhoneFusionCompleteTimeUs = ContactPredict.PhoneFusionCompleteTimeUs;
                            Packet.PhoneSendTimeUs = ContactPredict.PhoneSendTimeUs;

                            return Packet;
                        }
                        break;
                    }
                    case PhoneBatBinaryProtocol::EPacketType::ClockSyncResponse:
                    {
                        FPhoneBatClockSyncPayload ClockSync;
                        if (FPhoneBatBinarySerializer::DeserializeClockSync(PayloadData.GetData(), PayloadData.Num(), ClockSync))
                        {
                            // Handle clock sync in ProcessPacket
                            return Packet; // Empty packet, clock sync handled separately
                        }
                        break;
                    }
                }
            }
        }
    }

    // Fallback to JSON format
    return DeserializePacket(Data, Size);
}

uint64 UPhoneBatReceiverComponent::GetCurrentTimestampUSStatic()
{
    return FDateTime::UtcNow().ToUnixTimestamp() * 1000000ULL +
           FDateTime::UtcNow().GetMillisecond() * 1000ULL;
}

void UPhoneBatReceiverComponent::SerializePacket(const FPhoneBatDataPacket& Packet, TArray<uint8>& OutData)
{
    FMemoryWriter Writer(OutData);
    Writer << Packet.Timestamp;
    Writer << Packet.BatTransform;
    Writer << Packet.LinearVelocity;
    Writer << Packet.AngularVelocity;
    Writer << Packet.LinearAcceleration;
    Writer << Packet.SwingPhase;
    Writer << Packet.SwingProgress;
    Writer << Packet.PredictedContactTime;
    Writer << Packet.ContactConfidence;
    Writer << Packet.BatSpeed;
    Writer << Packet.WristRotationAtContact;
    Writer << Packet.ShotTypeClassification;
    Writer << Packet.ClassificationConfidence;
    Writer << Packet.PacketSequence;
    Writer << Packet.BatteryLevel;
    Writer << Packet.Temperature;
}

void UPhoneBatReceiverComponent::UpdateConnectionStats()
{
    if (PacketHistory.Num() < 2)
    {
        return;
    }

    // Calculate packet loss
    int32 ExpectedPackets = PacketHistory.Last().PacketSequence - PacketHistory[0].PacketSequence + 1;
    int32 ReceivedPackets = PacketHistory.Num();
    ConnectionInfo.PacketLossPercent = ExpectedPackets > 0 ?
        (1.0f - (float)ReceivedPackets / (float)ExpectedPackets) * 100.0f : 0.0f;

    // Update latency
    ConnectionInfo.LatencyMS = AverageLatency;
    ConnectionInfo.LastPacketTime = PacketHistory.Last().Timestamp;
}

void UPhoneBatReceiverComponent::UpdateLatencyEstimation(uint64 RemoteTimestamp)
{
    uint64 LocalTimestamp = GetCurrentTimestampUS();
    float LatencyUS = (float)(LocalTimestamp - RemoteTimestamp - ClockOffset);

    LatencySamples.Add(LatencyUS / 1000.0f); // Convert to ms
    if (LatencySamples.Num() > LATENCY_SAMPLE_COUNT)
    {
        LatencySamples.RemoveAt(0);
    }

    // Calculate average latency
    float Sum = 0.0f;
    for (float Sample : LatencySamples)
    {
        Sum += Sample;
    }
    AverageLatency = LatencySamples.Num() > 0 ? Sum / LatencySamples.Num() : 0.0f;

    // Update clock offset (using minimum latency as reference)
    float MinLatency = LatencySamples[0];
    for (float Sample : LatencySamples)
    {
        MinLatency = FMath::Min(MinLatency, Sample);
    }
    ClockOffset = (float)(LocalTimestamp - RemoteTimestamp) - MinLatency * 1000.0f;
}

void UPhoneBatReceiverComponent::UpdateJitterBuffer(const FPhoneBatDataPacket& Packet)
{
    JitterBuffer.Add(Packet);
    if (JitterBuffer.Num() > CurrentJitterBufferSize)
    {
        JitterBuffer.RemoveAt(0);
    }

    // Adaptive jitter buffer: measure network jitter and adjust buffer size
    if (PacketHistory.Num() >= 2)
    {
        const FPhoneBatDataPacket& LastPacket = PacketHistory.Last();
        float InterPacketTime = (Packet.Timestamp - LastPacket.Timestamp) / 1000.0f; // ms
        float ExpectedInterval = 10.0f; // 100Hz = 10ms
        float Jitter = FMath::Abs(InterPacketTime - ExpectedInterval);

        // Exponential moving average of jitter
        MeasuredJitterMs = MeasuredJitterMs * 0.9f + Jitter * 0.1f;

        // Adjust buffer size based on measured jitter
        // Target: buffer covers 2x measured jitter
        int32 OptimalSize = FMath::Clamp(
            FMath::CeilToInt(MeasuredJitterMs * 2.0f / 16.67f), // Convert to frames at 60Hz
            MIN_JITTER_BUFFER_SIZE,
            MAX_JITTER_BUFFER_SIZE
        );
        CurrentJitterBufferSize = OptimalSize;
    }
}

FPhoneBatDataPacket UPhoneBatReceiverComponent::GetInterpolatedPacket() const
{
    if (JitterBuffer.Num() == 0)
    {
        return LatestPacket;
    }

    if (JitterBuffer.Num() == 1)
    {
        return JitterBuffer[0];
    }

    // Simple linear interpolation between last two packets
    const FPhoneBatDataPacket& Prev = JitterBuffer[JitterBuffer.Num() - 2];
    const FPhoneBatDataPacket& Curr = JitterBuffer[JitterBuffer.Num() - 1];

    float Alpha = 0.5f; // Could be based on timing

    FPhoneBatDataPacket Interpolated;
    Interpolated.Timestamp = FMath::Lerp((float)Prev.Timestamp, (float)Curr.Timestamp, Alpha);
    Interpolated.BatTransform = FTransform::Slerp(Prev.BatTransform, Curr.BatTransform, Alpha);
    Interpolated.LinearVelocity = FMath::Lerp(Prev.LinearVelocity, Curr.LinearVelocity, Alpha);
    Interpolated.AngularVelocity = FMath::Lerp(Prev.AngularVelocity, Curr.AngularVelocity, Alpha);
    Interpolated.LinearAcceleration = FMath::Lerp(Prev.LinearAcceleration, Curr.LinearAcceleration, Alpha);
    Interpolated.SwingPhase = Curr.SwingPhase; // Use current phase
    Interpolated.SwingProgress = FMath::Lerp(Prev.SwingProgress, Curr.SwingProgress, Alpha);
    Interpolated.PredictedContactTime = Curr.PredictedContactTime;
    Interpolated.ContactConfidence = Curr.ContactConfidence;
    Interpolated.BatSpeed = Curr.BatSpeed;
    Interpolated.WristRotationAtContact = Curr.WristRotationAtContact;
    Interpolated.ShotTypeClassification = Curr.ShotTypeClassification;
    Interpolated.ClassificationConfidence = Curr.ClassificationConfidence;
    Interpolated.PacketSequence = Curr.PacketSequence;
    Interpolated.BatteryLevel = Curr.BatteryLevel;
    Interpolated.Temperature = Curr.Temperature;

    // Copy latency timestamps from current packet
    Interpolated.PhoneSensorReadTimeUs = Curr.PhoneSensorReadTimeUs;
    Interpolated.PhoneFusionCompleteTimeUs = Curr.PhoneFusionCompleteTimeUs;
    Interpolated.PhoneSendTimeUs = Curr.PhoneSendTimeUs;
    Interpolated.PcReceiveTimeUs = Curr.PcReceiveTimeUs;

    // T4: PC jitter buffer exit time
    Interpolated.PcJitterBufferExitTimeUs = GetCurrentTimestampUSStatic();

    return Interpolated;
}

FPhoneBatDataPacket UPhoneBatReceiverComponent::PredictPacket(float TimeAhead) const
{
    if (PacketHistory.Num() < 2)
    {
        return LatestPacket;
    }

    const FPhoneBatDataPacket& Current = PacketHistory.Last();
    const FPhoneBatDataPacket& Previous = PacketHistory[PacketHistory.Num() - 2];

    float dt = (Current.Timestamp - Previous.Timestamp) / 1000000.0f; // seconds
    if (dt <= 0.0f) dt = 1.0f / 60.0f;

    // Simple linear prediction
    FPhoneBatDataPacket Predicted = Current;
    Predicted.Timestamp = Current.Timestamp + (uint64)(TimeAhead * 1000000.0f);

    // Predict transform using velocity
    FVector PredictedPos = Current.BatTransform.GetLocation() + Current.LinearVelocity * TimeAhead;
    FQuat PredictedRot = Current.BatTransform.GetRotation() * FQuat(Current.AngularVelocity * TimeAhead);
    Predicted.BatTransform = FTransform(PredictedRot, PredictedPos, Current.BatTransform.GetScale3D());

    // Predict velocity with acceleration
    Predicted.LinearVelocity = Current.LinearVelocity + Current.LinearAcceleration * TimeAhead;

    // Predict swing progress
    if (Current.SwingPhase == EPhoneBatSwingPhase::Downswing)
    {
        Predicted.SwingProgress = FMath::Clamp(Current.SwingProgress + TimeAhead * 2.0f, 0.0f, 1.0f);
    }

    Predicted.PredictedContactTime = FMath::Max(0.0f, Current.PredictedContactTime - TimeAhead);

    return Predicted;
}

void UPhoneBatReceiverComponent::ProcessCalibrationData(const FPhoneBatDataPacket& Packet)
{
    CalibrationSamples.Add(Packet.BatTransform);
}

void UPhoneBatReceiverComponent::FinalizeCalibration()
{
    if (CalibrationSamples.Num() == 0)
    {
        return;
    }

    // Calculate average transform
    FVector AvgPos = FVector::ZeroVector;
    FQuat AvgRot = FQuat::Identity;

    for (const FTransform& Sample : CalibrationSamples)
    {
        AvgPos += Sample.GetLocation();
        AvgRot = FQuat::Slerp(AvgRot, Sample.GetRotation(), 1.0f / CalibrationSamples.Num());
    }

    AvgPos /= CalibrationSamples.Num();

    // Phone-to-bat transform assumes phone is held at grip, bat extends along -Y axis
    // This is a simplified calibration - real implementation would be more sophisticated
    FTransform PhoneToBat = FTransform(AvgRot, AvgPos);

    CalibrationData.PhoneToBatTransform = PhoneToBat;
    CalibrationData.GripOffset = FVector(0.0f, 0.15f, 0.0f); // 15cm from phone center to grip
    CalibrationData.GripRotation = FRotator(0.0f, -90.0f, 0.0f); // Bat extends along -Y
    CalibrationData.BatLength = 0.965f;
    CalibrationData.SweetSpotDistance = 0.18f;
    CalibrationData.bIsValid = true;
    CalibrationData.CalibrationTimestamp = GetCurrentTimestampUS();

    bIsCalibrating = false;
    OnPhoneBatCalibrationComplete.Broadcast(CalibrationData);

    CalibrationSamples.Empty();

    UE_LOG(LogTemp, Log, TEXT("PhoneBat: Calibration complete"));
}

void UPhoneBatReceiverComponent::OnDeviceDiscovered(const FPhoneBatConnectionInfo& DeviceInfo)
{
    OnPhoneBatDeviceDiscovered.Broadcast(DeviceInfo);
}

void UPhoneBatReceiverComponent::HandleSwingPhaseChange(EPhoneBatSwingPhase NewPhase)
{
    OnPhoneBatSwingPhaseChanged.Broadcast(NewPhase);
}

uint64 UPhoneBatReceiverComponent::GetCurrentTimestampUS() const
{
    return FDateTime::UtcNow().ToUnixTimestamp() * 1000000ULL +
           FDateTime::UtcNow().GetMillisecond() * 1000ULL;
}

float UPhoneBatReceiverComponent::GetCurrentTimestampSeconds() const
{
    return FDateTime::UtcNow().ToUnixTimestamp() + FDateTime::UtcNow().GetMillisecond() / 1000.0f;
}

// Receive runnable for background thread
class FPhoneBatReceiveRunnable : public FRunnable
{
public:
    FPhoneBatReceiveRunnable(UPhoneBatReceiverComponent* InComponent)
        : Component(InComponent), bStop(false)
    {
    }

    virtual ~FPhoneBatReceiveRunnable()
    {
        Stop();
    }

    virtual bool Init() override
    {
        return true;
    }

    virtual uint32 Run() override
    {
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(65536);

        // Event-driven receive using select()
        FSocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

        while (!bStop)
        {
            if (Component->UDPSocket.IsValid())
            {
                // Wait for data with 1ms timeout using select()
                bool bHasData = false;
                if (SocketSubsystem)
                {
                    TArray<FSocket*> ReadSockets;
                    ReadSockets.Add(Component->UDPSocket.Get());
                    TArray<FSocket*> WriteSockets;
                    TArray<FSocket*> ExceptSockets;

                    // Wait up to 1ms for data
                    bHasData = SocketSubsystem->WaitForSocket(ReadSockets, WriteSockets, ExceptSockets, 1000);
                }

                if (bHasData)
                {
                    FIPv4Endpoint Sender;
                    int32 BytesRead = 0;
                    bool bReceived = Component->UDPSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *Sender.ToInternetAddr());

                    if (bReceived && BytesRead > 0)
                    {
                        // Process on game thread
                        AsyncTask(ENamedThreads::GameThread, [Component = Component, Data = Buffer, Size = BytesRead, Sender]()
                        {
                            if (Component && Component->IsRegistered())
                            {
                                Component->ProcessPacket(Data.GetData(), Size, Sender);
                            }
                        });
                    }
                }
            }
            else
            {
                // Socket not ready yet, brief sleep
                FPlatformProcess::Sleep(0.001f);
            }
        }

        return 0;
    }

    virtual void Stop() override
    {
        bStop = true;
    }

    virtual void Exit() override
    {
    }

private:
    UPhoneBatReceiverComponent* Component;
    std::atomic<bool> bStop;
};