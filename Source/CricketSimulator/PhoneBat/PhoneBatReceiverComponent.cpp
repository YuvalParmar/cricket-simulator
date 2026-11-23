#include "PhoneBatReceiverComponent.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/RunnableThread.h"
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
    JitterBuffer.Reserve(JITTER_BUFFER_SIZE);

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
            PreviousPacket = LatestPacket;
            LatestPacket = InterpolatedPacket;

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

        // Create receive thread
        ReceiveRunnable = new FPhoneBatReceiveRunnable(this);
        ReceiveThread = FRunnableThread::Create(ReceiveRunnable, TEXT("PhoneBatReceiveThread"), 128 * 1024, TPri_Normal);

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
    if (Size < sizeof(FPhoneBatDataPacket))
    {
        return;
    }

    FPhoneBatDataPacket Packet = DeserializePacket(Data, Size);

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

    // Add to history
    PacketHistory.Add(Packet);
    if (PacketHistory.Num() > MAX_HISTORY_SIZE)
    {
        PacketHistory.RemoveAt(0);
    }

    // Handle calibration
    if (bIsCalibrating)
    {
        ProcessCalibrationData(Packet);
    }
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
    if (JitterBuffer.Num() > JITTER_BUFFER_SIZE)
    {
        JitterBuffer.RemoveAt(0);
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

        while (!bStop)
        {
            if (Component->UDPSocket.IsValid())
            {
                FIPv4Endpoint Sender;
                int32 BytesRead = 0;
                bool bHasData = Component->UDPSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *Sender.ToInternetAddr());

                if (bHasData && BytesRead > 0)
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

            FPlatformProcess::Sleep(0.001f); // 1ms sleep
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