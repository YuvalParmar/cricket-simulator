#pragma once

#include "CoreMinimal.h"
#include "PhoneBatNetworkProtocol.generated.h"

UENUM(BlueprintType)
enum class EPhoneBatPacketType : uint8
{
    // Connection packets
    Handshake       UMETA(DisplayName = "Handshake"),
    HandshakeAck    UMETA(DisplayName = "Handshake Ack"),
    PairingRequest  UMETA(DisplayName = "Pairing Request"),
    PairingResponse UMETA(DisplayName = "Pairing Response"),
    Disconnect      UMETA(DisplayName = "Disconnect"),
    Heartbeat       UMETA(DisplayName = "Heartbeat"),

    // Calibration packets
    CalibrationStart    UMETA(DisplayName = "Calibration Start"),
    CalibrationData     UMETA(DisplayName = "Calibration Data"),
    CalibrationComplete UMETA(DisplayName = "Calibration Complete"),

    // Sensor data packets
    SensorFrame     UMETA(DisplayName = "Sensor Frame"),
    SensorBatch     UMETA(DisplayName = "Sensor Batch"),

    // Swing analysis packets
    SwingPhaseUpdate    UMETA(DisplayName = "Swing Phase Update"),
    SwingContactPredict UMETA(DisplayName = "Swing Contact Predict"),
    SwingResult         UMETA(DisplayName = "Swing Result"),

    // Clock sync packets
    ClockSyncRequest  UMETA(DisplayName = "Clock Sync Request"),
    ClockSyncResponse UMETA(DisplayName = "Clock Sync Response"),

    // Configuration packets
    ConfigUpdate      UMETA(DisplayName = "Config Update"),
    SettingsRequest   UMETA(DisplayName = "Settings Request"),
    SettingsResponse  UMETA(DisplayName = "Settings Response")
};

UENUM(BlueprintType)
enum class EPhoneBatSwingPhase : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Backswing       UMETA(DisplayName = "Backswing"),
    Transition      UMETA(DisplayName = "Transition"),
    Downswing       UMETA(DisplayName = "Downswing"),
    Contact         UMETA(DisplayName = "Contact"),
    FollowThrough   UMETA(DisplayName = "Follow Through"),
    Recovery        UMETA(DisplayName = "Recovery")
};

UENUM(BlueprintType)
enum class EPhoneBatConnectionState : uint8
{
    Disconnected    UMETA(DisplayName = "Disconnected"),
    Discovering     UMETA(DisplayName = "Discovering"),
    Pairing         UMETA(DisplayName = "Pairing"),
    Connected       UMETA(DisplayName = "Connected"),
    Calibrating     UMETA(DisplayName = "Calibrating"),
    Active          UMETA(DisplayName = "Active"),
    Reconnecting    UMETA(DisplayName = "Reconnecting"),
    Error           UMETA(DisplayName = "Error")
};

USTRUCT(BlueprintType)
struct FPhoneBatPacketHeader
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 MagicNumber = 0x43425048; // "CBPH" - Cricket Bat Protocol Header

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 Version = 1;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    EPhoneBatPacketType PacketType = EPhoneBatPacketType::Handshake;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 SequenceNumber = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 PayloadSize = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 Timestamp = 0; // Microseconds since epoch (phone clock)

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ServerTimestamp = 0; // Microseconds since epoch (server clock)
};

USTRUCT(BlueprintType)
struct FPhoneBatHandshakePayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString DeviceId;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString DeviceName;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString AppVersion;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString Platform; // "iOS" or "Android"

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bSupportsAR = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bSupportsBluetooth = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    TArray<FString> SupportedFeatures;
};

USTRUCT(BlueprintType)
struct FPhoneBatPairingPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString PairingCode; // 6-digit code

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString DeviceId;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bIsLeftHanded = false;
};

USTRUCT(BlueprintType)
struct FPhoneBatSensorFramePayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 Timestamp = 0; // Microseconds

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector Accelerometer; // m/s^2

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector Gyroscope; // rad/s

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector Magnetometer; // microtesla

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FQuat Orientation; // World space quaternion

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector Gravity; // Extracted gravity vector

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector LinearAcceleration; // Acceleration without gravity

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float Confidence = 1.0f; // Sensor fusion confidence 0-1

    // ========================================================================
    // Latency Instrumentation Timestamps (T0-T1 from phone side)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    // T1: Phone sensor fusion complete time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    // T2: Phone serialization/send time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSendTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatSwingPhasePayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 Timestamp = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    EPhoneBatSwingPhase CurrentPhase = EPhoneBatSwingPhase::Idle;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float PhaseProgress = 0.0f; // 0-1 within current phase

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float BatSpeed = 0.0f; // m/s at current frame

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float MaxBatSpeed = 0.0f; // Peak speed in current swing

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector BatVelocity; // Current velocity vector

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector BatAngularVelocity; // Current angular velocity

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float Confidence = 1.0f;

    // ========================================================================
    // Latency Instrumentation Timestamps
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    // T1: Phone sensor fusion complete time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    // T2: Phone serialization/send time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSendTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatContactPredictPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 Timestamp = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 PredictedContactTime = 0; // Microseconds (phone clock)

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector PredictedContactPoint; // World space

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector PredictedBatVelocity; // At contact

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FQuat PredictedBatOrientation; // At contact

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float Confidence = 1.0f;

    // ========================================================================
    // Latency Instrumentation Timestamps
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    // T1: Phone sensor fusion complete time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    // T2: Phone serialization/send time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSendTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatSwingResultPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ContactTimestamp = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString ShotType; // "Drive", "Cut", "Pull", etc.

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float TimingQuality = 0.0f; // 0-1 (Perfect=1.0)

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float PowerRating = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float ControlRating = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector ContactPoint; // World space

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector BatVelocityAtContact;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FQuat BatOrientationAtContact;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float WristRotationAtContact = 0.0f; // Degrees

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    TArray<FPhoneBatSensorFramePayload> SwingFrames; // Full swing history
};

USTRUCT(BlueprintType)
struct FPhoneBatCalibrationPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 Step = 0; // Calibration step index

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString Instruction; // User-facing instruction

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FTransform PhoneToBatTransform; // Grip transform

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector BatLength = FVector(0.0f, 0.0f, 96.5f); // Standard bat length in cm

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FVector BatCenterOfMass = FVector(0.0f, 0.0f, 60.0f); // From handle

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bComplete = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float QualityScore = 0.0f; // 0-1
};

USTRUCT(BlueprintType)
struct FPhoneBatClockSyncPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ClientSendTime = 0; // T1: Client sends request

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ServerReceiveTime = 0; // T2: Server receives

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ServerSendTime = 0; // T3: Server sends response

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int64 ClientReceiveTime = 0; // T4: Client receives (filled on receipt)

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 RoundTripTime = 0; // Microseconds

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 ClockOffset = 0; // Microseconds (server - client)

    // ========================================================================
    // Latency Instrumentation Timestamps (for cross-reference)
    // ========================================================================
    // T0: Phone sensor read time (microseconds since epoch)
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    // T1: Phone sensor fusion complete time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    // T2: Phone serialization/send time
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol|Latency")
    int64 PhoneSendTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FPhoneBatConfigPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float SensorRate = 100.0f; // Hz

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float SwingDetectionSensitivity = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bEnableHaptics = true;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    bool bEnableARVisualization = true;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    int32 JitterBufferSize = 4; // Frames

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    float MaxLatencyCompensation = 100.0f; // ms

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FString PlayerProfileId;
};

// Complete packet structure for serialization
USTRUCT(BlueprintType)
struct FPhoneBatPacket
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    FPhoneBatPacketHeader Header;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Protocol")
    TArray<uint8> PayloadData; // Serialized payload

    // Helper functions for specific payload types
    bool SerializePayload(const FPhoneBatHandshakePayload& Payload);
    bool SerializePayload(const FPhoneBatPairingPayload& Payload);
    bool SerializePayload(const FPhoneBatSensorFramePayload& Payload);
    bool SerializePayload(const FPhoneBatSwingPhasePayload& Payload);
    bool SerializePayload(const FPhoneBatContactPredictPayload& Payload);
    bool SerializePayload(const FPhoneBatSwingResultPayload& Payload);
    bool SerializePayload(const FPhoneBatCalibrationPayload& Payload);
    bool SerializePayload(const FPhoneBatClockSyncPayload& Payload);
    bool SerializePayload(const FPhoneBatConfigPayload& Payload);

    bool DeserializePayload(FPhoneBatHandshakePayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatPairingPayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatSensorFramePayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatSwingPhasePayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatContactPredictPayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatSwingResultPayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatCalibrationPayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatClockSyncPayload& OutPayload) const;
    bool DeserializePayload(FPhoneBatConfigPayload& OutPayload) const;
};

// Constants
namespace PhoneBatProtocol
{
    constexpr uint32 MAGIC_NUMBER = 0x43425048; // "CBPH"
    constexpr uint16 CURRENT_VERSION = 1;
    constexpr int32 DEFAULT_PORT = 7777;
    constexpr int32 MAX_PACKET_SIZE = 1400; // MTU safe
    constexpr int32 HEADER_SIZE = 24; // bytes
    constexpr float HEARTBEAT_INTERVAL = 2.0f; // seconds
    constexpr float CONNECTION_TIMEOUT = 10.0f; // seconds
    constexpr int32 MAX_JITTER_BUFFER_FRAMES = 8;
    constexpr float MAX_LATENCY_COMPENSATION_MS = 100.0f;
}