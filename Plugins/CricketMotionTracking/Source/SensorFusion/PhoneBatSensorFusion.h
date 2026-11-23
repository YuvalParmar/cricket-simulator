#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkProtocol.h"
#include "PhoneBatSensorFusion.generated.h"

UENUM(BlueprintType)
enum class ESensorFusionAlgorithm : uint8
{
    ComplementaryFilter UMETA(DisplayName = "Complementary Filter"),
    MadgwickFilter      UMETA(DisplayName = "Madgwick Filter"),
    MahonyFilter        UMETA(DisplayName = "Mahony Filter"),
    KalmanFilter        UMETA(DisplayName = "Kalman Filter")
};

USTRUCT(BlueprintType)
struct FSensorFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    int64 TimestampUs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector Accelerometer; // m/s^2

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector Gyroscope; // rad/s

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector Magnetometer; // microtesla

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FQuat Orientation; // World space quaternion

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector Gravity; // Extracted gravity vector

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector LinearAcceleration; // Acceleration without gravity

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float Confidence = 1.0f;

    // Latency timestamps
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    int64 PhoneSensorReadTimeUs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    int64 PhoneFusionCompleteTimeUs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    int64 PhoneSendTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FSensorFusionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FQuat Orientation = FQuat::Identity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector LinearAcceleration = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector Gravity = FVector(0.0f, 0.0f, -9.81f);

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    bool bIsInitialized = false;
};

UCLASS(BlueprintType)
class CRICKETMOTIONTRACKING_API UPhoneBatSensorFusion : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatSensorFusion();

    // Singleton access
    static UPhoneBatSensorFusion& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Process incoming sensor frame from phone
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|SensorFusion")
    FSensorFusionState ProcessSensorFrame(const FPhoneBatSensorFramePayload& Payload);

    // Get current fused state
    UFUNCTION(BlueprintPure, Category = "PhoneBat|SensorFusion")
    const FSensorFusionState& GetCurrentState() const { return CurrentState; }

    // Configuration
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|SensorFusion")
    void SetAlgorithm(ESensorFusionAlgorithm Algorithm);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat|SensorFusion")
    void SetGain(float Gain);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat|SensorFusion")
    void SetMagneticGain(float Gain);

    // Reset filter
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|SensorFusion")
    void Reset();

protected:
    // Current state
    UPROPERTY()
    FSensorFusionState CurrentState;

    // Algorithm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    ESensorFusionAlgorithm Algorithm = ESensorFusionAlgorithm::MadgwickFilter;

    // Filter parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float Beta = 0.1f; // Madgwick filter gain

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float TwoKp = 2.0f; // Mahony filter proportional gain

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float TwoKi = 0.0f; // Mahony filter integral gain

    // Complementary filter
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float ComplementaryAlpha = 0.98f;

    // Internal state for filters
    FVector MahonyIntegralError = FVector::ZeroVector;
    float LastTimestampUs = 0;

    // Filter implementations
    FSensorFusionState UpdateComplementaryFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);
    FSensorFusionState UpdateMadgwickFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);
    FSensorFusionState UpdateMahonyFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);

    // Helper functions
    FQuat ComputeOrientationFromAccelMag(const FVector& Accel, const FVector& Mag) const;
    void NormalizeVector(FVector& Vec) const;
    float InvSqrt(float x) const;
};