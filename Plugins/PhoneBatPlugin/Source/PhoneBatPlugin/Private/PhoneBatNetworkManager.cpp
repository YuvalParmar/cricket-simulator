#include "PhoneBatNetworkManager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Async/Async.h"
#include "HAL/RunnableThread.h"
#include "Misc/DateTime.h"

#define PHONEBAT_MAX_PACKET_SIZE 1400

// Static instance
TSharedPtr<PhoneBat::UPhoneBatNetworkManager> GPhoneBatNetworkManagerInstance;

UPhoneBatNetworkManager& UPhoneBatNetworkManager::Get()
{
    if (!GPhoneBatNetworkManagerInstance.IsValid())
    {
        GPhoneBatNetworkManagerInstance = MakeShareable(new UPhoneBatNetworkManager());
        GPhoneBatNetworkManagerInstance->AddToRoot();
    }
    return *GPhoneBatNetworkManagerInstance;
}

UPhoneBatNetworkManager::UPhoneBatNetworkManager()
{
    ConnectionStats = FPhoneBatConnectionStats();
    bIsListening = false;
    ListenPort = 7777;
    bStopReceiveThread = true;
    LatencyHistory.Empty();
    AverageLatencyMs = 0.0f;
    ClockOffsetUs = 0;
    PacketSequence = 0;
    LastPacketTimestampUs = 0;
}

void UPhoneBatNetworkManager::Initialize()
{
    if (!bIsListening)
    {
        StartListening(ListenPort);
    }
    UE_LOG(LogTemp, Log, TEXT("PhoneBatNetworkManager initialized"));
}

void UPhoneBatNetworkManager::Shutdown()
{
    StopListening();
    UE_LOG(LogTemp, Log, TEXT("PhoneBatNetworkManager shutdown"));
}

bool UPhoneBatNetworkManager::StartListening(int32 Port)
{
    if (bIsListening) return true;

    FIPv4Address LocalAddress(0, 0, 0, 0);
    FIPv4Endpoint LocalEndpoint(LocalAddress, Port);

    FSocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("PhoneBat: Socket subsystem not available"));
        return false;
    }

    ListenSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("PhoneBatUDP"), false);
    if (!ListenSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PhoneBat: Failed to create UDP socket"));
        return false;
    }

    if (!ListenSocket->Bind(*LocalEndpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("PhoneBat: Failed to bind UDP socket to port %d"), Port);
        ListenSocket.Reset();
        return false;
    }

    ListenSocket->SetNonBlocking(true);
    ListenSocket->SetReceiveBufferSize(65536, 65536);

    ListenPort = Port;
    bStopReceiveThread = false;
    bIsListening = true;

    ConnectionStats.bIsConnected = true;
    ConnectionStats.Port = Port;
    ConnectionStats.IPAddress = TEXT("0.0.0.0");
    OnConnectionStatusChanged.Broadcast(true);

    // Start receive thread with high priority
    FRunnableThread* Thread = FRunnableThread::Create(
        [this]() {
            TArray<uint8> Buffer;
            Buffer.SetNumUninitialized(PHONEBAT_MAX_PACKET_SIZE + 128);

            FSocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

            while (!bStopReceiveThread)
            {
                if (ListenSocket.IsValid())
                {
                    FIPv4Endpoint Sender;
                    int32 BytesRead = 0;
                    if (ListenSocket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *Sender.ToInternetAddr()))
                    {
                        if (BytesRead > 0 && BytesRead >= 24)
                        {
                            FPhoneBatSensorFramePayload Payload;
                            ProcessBinaryPacket(Buffer.GetData(), BytesRead, Sender);
                        }
                    }
                }
                FPlatformProcess::Sleep(0.001f);
            }
        },
        TEXT("PhoneBatReceiveThread"),
        128 * 1024,
        TPri_TimeCritical
    );

    ReceiveThread = Thread;
    UE_LOG(LogTemp, Log, TEXT("PhoneBat: Listening on UDP port %d"), Port);
    return true;
}

void UPhoneBatNetworkManager::StopListening()
{
    bStopReceiveThread = true;

    if (ReceiveThread)
    {
        ReceiveThread->WaitForCompletion();
        delete ReceiveThread;
        ReceiveThread = nullptr;
    }

    if (ListenSocket.IsValid())
    {
        ListenSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket.Get());
        ListenSocket.Reset();
    }

    bIsListening = false;
    ConnectionStats.bIsConnected = false;
    OnConnectionStatusChanged.Broadcast(false);
}

void UPhoneBatNetworkManager::SendClockSyncRequest()
{
    // In a real implementation, this would send a clock sync request
    int64 ClientSendTime = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL +
                           FDateTime::UtcNow().GetMillisecond() * 1000LL;

    // Simulate response after 1-2ms
    AsyncTask(ENamedThreads::AnyThread, [ClientSendTime]() {
        FPlatformProcess::Sleep(0.001f + FMath::RandRange(0, 1) / 1000.0f);

        int64 ServerReceiveTime = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL;
        int64 ServerSendTime = ServerReceiveTime + FMath::RandRange(1, 2) * 1000LL;
        int64 ClientReceiveTime = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL +
                                  FDateTime::UtcNow().GetMillisecond() * 1000LL;

        int32 RTT = static_cast<int32>(ClientReceiveTime - ClientSendTime);
        int32 Offset = (ServerReceiveTime + (ServerSendTime - ServerReceiveTime) / 2) - ClientReceiveTime;

        AsyncTask(ENamedThreads::GameThread, [Offset, RTT]() {
            Get().ClockOffsetUs = Offset;
            Get().OnClockSyncUpdate.Broadcast(Offset, RTT);
        });
    });
}

void UPhoneBatNetworkManager::InjectSensorFrame(const FPhoneBatSensorFramePayload& Payload)
{
    OnSensorFrameReceived.Broadcast(Payload);
}

void UPhoneBatNetworkManager::InjectSwingPhase(const FPhoneBatSwingPhasePayload& Payload)
{
    OnSwingPhaseReceived.Broadcast(Payload);
}

void UPhoneBatNetworkManager::ProcessBinaryPacket(const uint8* Data, int32 Size, const FIPv4Endpoint& Sender)
{
    // Check for magic number
    const uint32 Magic = 0x43425048; // "CBPH"
    if (Size < 24 || *reinterpret_cast<const uint32*>(Data) != Magic)
    {
        return;
    }

    // Parse header
    int32 Version = *reinterpret_cast<const uint16*>(Data + 4);
    int32 PacketType = *reinterpret_cast<const uint8*>(Data + 6);
    uint32 SeqNum = *reinterpret_cast<const uint32*>(Data + 8);
    uint32 PayloadSize = *reinterpret_cast<const uint32*>(Data + 12);
    int64 Timestamp = *reinterpret_cast<const int64*>(Data + 16);

    if (Version != 1 || PayloadSize > PHONEBAT_MAX_PACKET_SIZE || Size < 24 + PayloadSize)
    {
        return;
    }

    // Process based on packet type
    switch (PacketType)
    {
        case 20: // SensorFrame
        {
            FPhoneBatSensorFramePayload Payload;
            DeserializeSensorFrame(Data + 24, PayloadSize, Payload);

            UpdateLatency(Timestamp);
            Payload.PacketSequence = SeqNum;

            AsyncTask(ENamedThreads::GameThread, [Payload]() {
                Get().OnSensorFrameReceived.Broadcast(Payload);
            });
            break;
        }
        case 30: // SwingPhaseUpdate
        {
            FPhoneBatSwingPhasePayload Payload;
            DeserializeSwingPhase(Data + 24, PayloadSize, Payload);

            UpdateLatency(Timestamp);
            Payload.Timestamp = Timestamp;

            AsyncTask(ENamedThreads::GameThread, [Payload]() {
                Get().OnSwingPhaseReceived.Broadcast(Payload);
            });
            break;
        }
        case 31: // ContactPredict
        {
            FPhoneBatContactPredictPayload Payload;
            DeserializeContactPredict(Data + 24, PayloadSize, Payload);

            UpdateLatency(Timestamp);
            Payload.Timestamp = Timestamp;

            AsyncTask(ENamedThreads::GameThread, [Payload]() {
                Get().OnContactPredictionReceived.Broadcast(Payload);
            });
            break;
        }
        case 32: // SwingResult
        {
            FPhoneBatSwingResultPayload Payload;
            DeserializeSwingResult(Data + 24, PayloadSize, Payload);

            UpdateLatency(Timestamp);

            AsyncTask(ENamedThreads::GameThread, [Payload]() {
                Get().OnSwingResultReceived.Broadcast(Payload);
            });
            break;
        }
    }

    // Update connection stats
    ConnectionStats.LastPacketTimeUs = Timestamp;
    ConnectionStats.IPAddress = Sender.Address.ToString();
    PacketHistory.Add(Timestamp);
    if (PacketHistory.Num() > 60) PacketHistory.RemoveAt(0);
}

void UPhoneBatNetworkManager::UpdateLatency(int64 RemoteTimestamp)
{
    int64 LocalTimestamp = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL +
                           FDateTime::UtcNow().GetMillisecond() * 1000LL;

    float LatencyMs = (LocalTimestamp - RemoteTimestamp - ClockOffsetUs) / 1000.0f;

    LatencyHistory.Add(LatencyMs);
    if (LatencyHistory.Num() > 100) LatencyHistory.RemoveAt(0);

    float Sum = 0;
    for (float L : LatencyHistory) Sum += L;
    AverageLatencyMs = LatencyHistory.Num() > 0 ? Sum / LatencyHistory.Num() : 0;

    ConnectionStats.LatencyMS = AverageLatencyMs;
    ConnectionStats.PacketLossPercent = 0.0f;
    ConnectionStats.PacketsPerSecond = PacketHistory.Num() > 1 ?
        1000000.0f / (PacketHistory.Last() - PacketHistory[0]) * PacketHistory.Num() : 0;
    ConnectionStats.TotalPacketsReceived++;
}

void UPhoneBatNetworkManager::DeserializeSensorFrame(const uint8* Data, int32 Size, FPhoneBatSensorFramePayload& OutPayload)
{
    const uint8* Ptr = Data;

    OutPayload.Timestamp = *reinterpret_cast<const int64*>(Ptr); Ptr += 8;
    OutPayload.Accelerometer = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Gyroscope = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Magnetometer = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Orientation = FQuat(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Gravity = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.LinearAcceleration = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Confidence = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
}

void UPhoneBatNetworkManager::DeserializeSwingPhase(const uint8* Data, int32 Size, FPhoneBatSwingPhasePayload& OutPayload)
{
    const uint8* Ptr = Data;

    OutPayload.Timestamp = *reinterpret_cast<const int64*>(Ptr); Ptr += 8;
    OutPayload.CurrentPhase = static_cast<EPhoneBatSwingPhase>(*Ptr++);
    OutPayload.PhaseProgress = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.BatSpeed = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.MaxBatSpeed = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.BatVelocity = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.BatAngularVelocity = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Confidence = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
}

void UPhoneBatNetworkManager::DeserializeContactPredict(const uint8* Data, int32 Size, FPhoneBatContactPredictPayload& OutPayload)
{
    const uint8* Ptr = Data;

    OutPayload.Timestamp = *reinterpret_cast<const int64*>(Ptr); Ptr += 8;
    OutPayload.PredictedContactTime = *reinterpret_cast<const int64*>(Ptr); Ptr += 8;
    OutPayload.PredictedContactPoint = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.PredictedBatVelocity = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.PredictedBatOrientation = FQuat(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.Confidence = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
}

void UPhoneBatNetworkManager::DeserializeSwingResult(const uint8* Data, int32 Size, FPhoneBatSwingResultPayload& OutPayload)
{
    const uint8* Ptr = Data;

    OutPayload.ContactTimestamp = *reinterpret_cast<const int64*>(Ptr); Ptr += 8;

    // Read FString (32-bit length prefix + UTF8 data)
    int32 NameLen = *reinterpret_cast<const int32*>(Ptr); Ptr += 4;
    OutPayload.ShotType = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Ptr)));
    Ptr += NameLen;

    OutPayload.TimingQuality = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.PowerRating = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.ControlRating = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
    OutPayload.ContactPoint = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.BatVelocityAtContact = FVector(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.BatOrientationAtContact = FQuat(
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4,
        *reinterpret_cast<const float*>(Ptr); Ptr += 4
    );
    OutPayload.WristRotationAtContact = *reinterpret_cast<const float*>(Ptr); Ptr += 4;
}