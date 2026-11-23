#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkManager.h"
#include "PhoneBatMotionInterpreter.generated.h"

UENUM(BlueprintType)
enum class ESwingPhase : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Backswing       UMETA(DisplayName = "Backswing"),
    Transition      UMETA(DisplayName = "Transition"),
    Downswing       UMETA(DisplayName = "Downswing"),
    Contact         UMETA(DisplayName = "Contact"),
    FollowThrough   UMETA(DisplayName = "Follow Through"),
    Recovery        UMETA(DisplayName = "Recovery")
};

USTRUCT(BlueprintType)
struct FSwingMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float MaxBatSpeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float AvgBatSpeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float BackswingDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float DownswingDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float ContactTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float BatAngleAtContact = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float WristRotationAtContact = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float SwingPlaneAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float Tempo = 0.0f;
};

USTRUCT(BlueprintType)
struct FContactPrediction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float TimeToContact = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector PredictedContactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector PredictedBatVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FQuat PredictedBatOrientation = FQuat::Identity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float Confidence = 0.0f;
};

USTRUCT(BlueprintType)
struct FShotClassification
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FString ShotType;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float TimingQuality = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float PowerRating = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float ControlRating = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float Confidence = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingPhaseChanged, ESwingPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContactPredicted, const FContactPrediction&, Prediction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingCompleted, const FSwingMetrics&, Metrics);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotClassified, const FShotClassification&, Classification);

UCLASS(BlueprintType)
class PHONEBATPLUGIN_API UPhoneBatMotionInterpreter : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatMotionInterpreter();

    // Singleton access
    static UPhoneBatMotionInterpreter& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Process sensor frame
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Motion")
    void ProcessSensorFrame(const FPhoneBatSensorFramePayload& Payload);

    // Process swing phase update
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Motion")
    void ProcessSwingPhase(const FPhoneBatSwingPhasePayload& Payload);

    // Process contact prediction
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Motion")
    void ProcessContactPrediction(const FPhoneBatContactPredictPayload& Payload);

    // Process swing result
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Motion")
    void ProcessSwingResult(const FPhoneBatSwingResultPayload& Result);

    // Getters
    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    ESwingPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    float GetPhaseProgress() const { return PhaseProgress; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    float GetBatSpeed() const { return CurrentBatSpeed; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    const FSwingMetrics& GetCurrentSwingMetrics() const { return CurrentSwingMetrics; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    const FContactPrediction& GetContactPrediction() const { return ContactPrediction; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Motion")
    const FShotClassification& GetLastShotClassification() const { return LastShotClassification; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSwingPhaseChanged OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnContactPredicted OnContactPredicted;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSwingCompleted OnSwingCompleted;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnShotClassified OnShotClassified;

protected:
    ESwingPhase CurrentPhase = ESwingPhase::Idle;
    float PhaseProgress = 0.0f;
    float CurrentBatSpeed = 0.0f;
    float PeakBatSpeed = 0.0f;
    FSwingMetrics CurrentSwingMetrics;
    FContactPrediction ContactPrediction;
    FShotClassification LastShotClassification;

    TArray<float> SpeedHistory;
    TArray<float> AccelerationHistory;
    TArray<FQuat> OrientationHistory;
    TArray<int64> TimestampHistory;
    TArray<FVector> AngularVelocityHistory;

    int64 SwingStartTimeUs = 0;
    int64 BackswingEndTimeUs = 0;
    int64 ContactTimeUs = 0;

    float SwingDetectionThreshold = 3.0f;
    float BackswingToDownswingThreshold = 0.8f;
    float ContactAccelerationThreshold = 40.0f;
    float MinDownswingSpeed = 1.0f;
    int32 HistoryWindowSize = 50;

    void DetectPhase(const FPhoneBatSensorFramePayload& Payload);
    void UpdatePhaseProgress(const FPhoneBatSensorFramePayload& Payload);
    void HandlePhaseTransition(ESwingPhase OldPhase, ESwingPhase NewPhase);
    void OnBackswingStart();
    void OnTransition();
    void OnDownswingStart();
    void OnContact();
    void OnFollowThrough();
    void OnRecovery();
    void PredictContact(const FPhoneBatSensorFramePayload& Payload);
    void PredictContactFromHistory();
    FShotClassification ClassifyShot();
    float EstimateBatSpeed(const FPhoneBatSensorFramePayload& Payload);
    float CalculateBatAngle() const;
    float CalculateWristRotation() const;
    float CalculateSwingPlane() const;
    float CalculateTempo() const;
};