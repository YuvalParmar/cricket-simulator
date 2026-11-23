#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "PhoneBatNetworkProtocol.h"
#include "PhoneBatBinaryProtocol.h"
#include "PhoneBatReceiverComponent.generated.h"

UENUM(BlueprintType)
enum class EPhoneBatConnectionType : uint8
{
    UDP             UMETA(DisplayName = "UDP (WiFi)"),
    BluetoothLE     UMETA(DisplayName = "Bluetooth LE"),
    Auto            UMETA(DisplayName = "Auto Detect")
};

USTRUCT(BlueprintType)
struct FPhoneBatDataPacket
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    int64 Timestamp = 0; // Microseconds since epoch

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    FTransform BatTransform; // Phone transform in world space (after calibration)

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    FVector LinearVelocity; // m/s

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    FVector AngularVelocity; // rad/s

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    FVector LinearAcceleration; // m/s^2

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    EPhoneBatSwingPhase SwingPhase = EPhoneBatSwingPhase::Idle;

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float SwingProgress = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float PredictedContactTime = 0.0f; // Seconds from now

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float ContactConfidence = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float BatSpeed = 0.0f; // m/s at contact

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float WristRotationAtContact = 0.0f; // degrees

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    int32 ShotTypeClassification = 0; // Classified shot type

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float ClassificationConfidence = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    int32 PacketSequence = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float BatteryLevel = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data")
    float Temperature = 25.0f; // Celsius

    // ========================================================================
    // Latency Instrumentation Timestamps (for end-to-end measurement)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    // T1: Phone sensor fusion complete time
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    // T2: Phone serialization/send time
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PhoneSendTimeUs = 0;

    // T3: PC receive time (set by receiver)
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PcReceiveTimeUs = 0;

    // T4: PC jitter buffer exit time
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PcJitterBufferExitTimeUs = 0;

    // T5: PC prediction complete time
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 PcPredictionCompleteTimeUs = 0;

    // T6: Game thread tick time (when bat state updated)
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 GameThreadUpdateTimeUs = 0;

    // T7: Contact detection time
    UPROPERTY(BlueprintReadWrite, Category = "Phone Bat Data|Latency")
    int64 ContactDetectionTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatCalibrationData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FTransform PhoneToBatTransform; // Transform from phone to bat coordinate space

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector GripOffset; // Offset from phone center to grip point

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FRotator GripRotation; // Rotation of grip relative to phone

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float BatLength = 0.965f; // Standard bat length in meters

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float SweetSpotDistance = 0.18f; // Distance from handle to sweet spot

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    int64 CalibrationTimestamp = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatConnectionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString DeviceName;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString DeviceID;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    FString IPAddress;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    int32 Port = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    EPhoneBatConnectionType ConnectionType = EPhoneBatConnectionType::UDP;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    bool bIsConnected = false;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    float LatencyMS = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    float PacketLossPercent = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Connection")
    int64 LastPacketTime = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatDataReceived, const FPhoneBatDataPacket&, Packet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatCalibrationComplete, const FPhoneBatCalibrationData&, Calibration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatConnectionChanged, bool, bConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatDeviceDiscovered, const FPhoneBatConnectionInfo&, DeviceInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatSwingPhaseChanged, EPhoneBatSwingPhase, Phase);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UPhoneBatReceiverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPhoneBatReceiverComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Connection management
    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    bool StartListening(int32 Port = 7777, EPhoneBatConnectionType Type = EPhoneBatConnectionType::UDP);

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    void StopListening();

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    bool ConnectToDevice(const FPhoneBatConnectionInfo& DeviceInfo);

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    void StartDeviceDiscovery(float TimeoutSeconds = 5.0f);

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Connection")
    void StopDeviceDiscovery();

    // Calibration
    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Calibration")
    void StartCalibration();

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Calibration")
    void ApplyCalibration(const FPhoneBatCalibrationData& CalibrationData);

    UFUNCTION(BlueprintCallable, Category = "Phone Bat|Calibration")
    void ResetCalibration();

    // Data access
    UFUNCTION(BlueprintPure, Category = "Phone Bat|Data")
    const FPhoneBatDataPacket& GetLatestPacket() const { return LatestPacket; }

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Data")
    const FPhoneBatCalibrationData& GetCalibrationData() const { return CalibrationData; }

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Data")
    const FPhoneBatConnectionInfo& GetConnectionInfo() const { return ConnectionInfo; }

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Data")
    bool IsConnected() const { return ConnectionInfo.bIsConnected; }

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Data")
    float GetLatencyMS() const { return ConnectionInfo.LatencyMS; }

    // Predicted bat state (with latency compensation)
    UFUNCTION(BlueprintPure, Category = "Phone Bat|Prediction")
    FTransform GetPredictedBatTransform(float TimeAhead) const;

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Prediction")
    FVector GetPredictedBatVelocity(float TimeAhead) const;

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Prediction")
    float GetTimeToContact() const { return LatestPacket.PredictedContactTime; }

    UFUNCTION(BlueprintPure, Category = "Phone Bat|Prediction")
    float GetContactConfidence() const { return LatestPacket.ContactConfidence; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Phone Bat|Events")
    FOnPhoneBatDataReceived OnPhoneBatDataReceived;

    UPROPERTY(BlueprintAssignable, Category = "Phone Bat|Events")
    FOnPhoneBatCalibrationComplete OnPhoneBatCalibrationComplete;

    UPROPERTY(BlueprintAssignable, Category = "Phone Bat|Events")
    FOnPhoneBatConnectionChanged OnPhoneBatConnectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Phone Bat|Events")
    FOnPhoneBatDeviceDiscovered OnPhoneBatDeviceDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Phone Bat|Events")
    FOnPhoneBatSwingPhaseChanged OnPhoneBatSwingPhaseChanged;

protected:
    // Network
    TSharedPtr<FSocket> UDPSocket;
    TSharedPtr<FSocket> BroadcastSocket;
    FIPv4Endpoint LocalEndpoint;
    FIPv4Endpoint RemoteEndpoint;

    // Thread for receiving
    FRunnableThread* ReceiveThread;
    class FPhoneBatReceiveRunnable;

    class FPhoneBatReceiveRunnable* ReceiveRunnable;

    // Data buffers
    UPROPERTY()
    FPhoneBatDataPacket LatestPacket;

    UPROPERTY()
    FPhoneBatDataPacket PreviousPacket;

    UPROPERTY()
    FPhoneBatCalibrationData CalibrationData;

    UPROPERTY()
    FPhoneBatConnectionInfo ConnectionInfo;

    // Packet history for interpolation/prediction
    TArray<FPhoneBatDataPacket> PacketHistory;
    static const int32 MAX_HISTORY_SIZE = 30; // 0.5 second at 60Hz (reduced memory)

    // Jitter buffer - adaptive size based on network conditions
    TArray<FPhoneBatDataPacket> JitterBuffer;
    static const int32 MIN_JITTER_BUFFER_SIZE = 1; // Frames = ~16.7ms at 60Hz (minimum)
    static const int32 MAX_JITTER_BUFFER_SIZE = 4; // Maximum under high jitter

    // Adaptive jitter buffer state
    int32 CurrentJitterBufferSize = 1;
    float MeasuredJitterMs = 0.0f;

    // Latency compensation
    float ClockOffset = 0.0f; // Remote time - local time
    float AverageLatency = 0.0f;
    TArray<float> LatencySamples;
    static const int32 LATENCY_SAMPLE_COUNT = 100;

    // Calibration state
    bool bIsCalibrating = false;
    TArray<FTransform> CalibrationSamples;
    static const int32 CALIBRATION_SAMPLE_COUNT = 60; // 1 second at 60Hz

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    int32 ListenPort = 7777;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    int32 MaxPacketSize = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    float JitterBufferTimeMS = 16.67f; // 1 frame at 60Hz (minimum)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    float MaxLatencyMS = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    float PacketTimeoutMS = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    bool bEnablePrediction = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat|Config")
    float PredictionTimeSteps = 3; // Frames to predict ahead

    // Internal functions
    void ProcessPacket(const uint8* Data, int32 Size, const FIPv4Endpoint& Sender);
    FPhoneBatDataPacket DeserializePacket(const uint8* Data, int32 Size);
    FPhoneBatDataPacket DeserializeBinaryPacket(const uint8* Data, int32 Size);

    // Static timestamp for binary serializer
    static uint64 GetCurrentTimestampUSStatic();
    void UpdateConnectionStats();
    void UpdateLatencyEstimation(uint64 RemoteTimestamp);
    void UpdateJitterBuffer(const FPhoneBatDataPacket& Packet);
    FPhoneBatDataPacket GetInterpolatedPacket() const;
    FPhoneBatDataPacket PredictPacket(float TimeAhead) const;
    void ProcessCalibrationData(const FPhoneBatDataPacket& Packet);
    void FinalizeCalibration();
    void OnDeviceDiscovered(const FPhoneBatConnectionInfo& DeviceInfo);
    void HandleSwingPhaseChange(EPhoneBatSwingPhase NewPhase);
    uint64 GetCurrentTimestampUS() const;
    float GetCurrentTimestampSeconds() const;
};