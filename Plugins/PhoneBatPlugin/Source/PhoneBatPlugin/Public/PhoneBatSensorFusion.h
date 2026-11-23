#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkManager.h"
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensorFrameProcessed, const FSensorFusionState&, State);

UCLASS(BlueprintType)
class PHONEBATPLUGIN_API UPhoneBatSensorFusion : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatSensorFusion();

    // Singleton access
    static UPhoneBatSensorFusion& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Process sensor frame
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

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSensorFrameProcessed OnSensorFrameProcessed;

protected:
    UPROPERTY()
    FSensorFusionState CurrentState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    ESensorFusionAlgorithm Algorithm = ESensorFusionAlgorithm::MadgwickFilter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float Beta = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float TwoKp = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float TwoKi = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|SensorFusion")
    float ComplementaryAlpha = 0.98f;

    FVector MahonyIntegralError = FVector::ZeroVector;
    float LastTimestampUs = 0;

    FSensorFusionState UpdateComplementaryFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);
    FSensorFusionState UpdateMadgwickFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);
    FSensorFusionState UpdateMahonyFilter(const FPhoneBatSensorFramePayload& Payload, float DeltaTime);

    void NormalizeVector(FVector& Vec) const;
    float InvSqrt(float x) const;
};