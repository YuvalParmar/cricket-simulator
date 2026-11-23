#pragma once

#include "CoreMinimal.h"
#include "PhoneBatNetworkProtocol.h"
#include "PhoneBatCalibration.generated.h"

class UPhoneBatInput;

UENUM(BlueprintType)
enum class ECalibrationStep : uint8
{
    Step0_PlacePhoneFlat    UMETA(DisplayName = "Place Phone Flat on Table"),
    Step1_HoldBatGrip       UMETA(DisplayName = "Hold Phone in Bat Grip"),
    Step2_SwingSlowly       UMETA(DisplayName = "Perform Slow Practice Swing"),
    Step3_VerifyAlignment   UMETA(DisplayName = "Verify Bat Alignment"),
    Complete                UMETA(DisplayName = "Calibration Complete")
};

UENUM(BlueprintType)
enum class ECalibrationStatus : uint8
{
    NotStarted      UMETA(DisplayName = "Not Started"),
    InProgress      UMETA(DisplayName = "In Progress"),
    WaitingForPhone UMETA(DisplayName = "Waiting for Phone"),
    Completed       UMETA(DisplayName = "Completed"),
    Failed          UMETA(DisplayName = "Failed"),
    Cancelled       UMETA(DisplayName = "Cancelled")
};

USTRUCT(BlueprintType)
struct FCalibrationStepData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    ECalibrationStep Step = ECalibrationStep::Step0_PlacePhoneFlat;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString Instruction;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString DetailInstruction;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float EstimatedDurationSeconds = 10.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bRequiresUserAction = true;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bAutoAdvance = false;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    TArray<FString> ValidationCriteria;
};

USTRUCT(BlueprintType)
struct FCalibrationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FTransform PhoneToBatTransform;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector BatLength = FVector(0.0f, 0.0f, 96.5f);

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector BatCenterOfMass = FVector(0.0f, 0.0f, 60.0f);

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float QualityScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString ErrorMessage;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    TArray<FVector> ValidationSamples; // Collected validation poses

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float AverageOrientationError = 0.0f; // Degrees

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float AveragePositionError = 0.0f; // cm
};

USTRUCT(BlueprintType)
struct FCalibrationProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString ProfileId;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bIsLeftHanded = false;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FTransform PhoneToBatTransform;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector BatDimensions; // X=width, Y=depth, Z=length

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector BatCenterOfMass;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float GripOffsetAlongHandle = 0.0f; // cm from bottom

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float QualityScore = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FDateTime CalibrationDate;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FString DeviceId;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    int32 AppVersion = 1;
};

UCLASS(BlueprintType, Blueprintable)
class CRICKETSIMULATOR_API UPhoneBatCalibration : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatCalibration();

    // Initialize calibration system
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void Initialize(UPhoneBatInput* InPhoneBatInput);

    // Start calibration sequence
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void StartCalibration(const FCalibrationProfile& Profile);

    // Advance to next step
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void AdvanceStep();

    // Complete current step with data
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void CompleteCurrentStep(const FTransform& MeasuredTransform);

    // Cancel calibration
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void CancelCalibration();

    // Save/load calibration profile
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    bool SaveProfile(const FCalibrationProfile& Profile, const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    bool LoadProfile(FCalibrationProfile& OutProfile, const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    TArray<FString> GetSavedProfileNames() const;

    // Validation
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void StartValidation();

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    void RecordValidationSample(const FTransform& ExpectedTransform, const FTransform& ActualTransform);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Calibration")
    FCalibrationResult FinishValidation();

    // Getters
    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    ECalibrationStatus GetStatus() const { return Status; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    ECalibrationStep GetCurrentStep() const { return CurrentStep; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    FCalibrationStepData GetCurrentStepData() const;

    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    float GetOverallProgress() const;

    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    const FCalibrationProfile& GetCurrentProfile() const { return CurrentProfile; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat Calibration")
    const FCalibrationResult& GetLastResult() const { return LastResult; }

    // Events
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationStepChanged, ECalibrationStep, NewStep);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationStatusChanged, ECalibrationStatus, NewStatus);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationProgress, float, Progress);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationComplete, const FCalibrationResult&, Result);

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Calibration Events")
    FOnCalibrationStepChanged OnStepChanged;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Calibration Events")
    FOnCalibrationStatusChanged OnStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Calibration Events")
    FOnCalibrationProgress OnProgress;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Calibration Events")
    FOnCalibrationComplete OnComplete;

protected:
    // References
    UPROPERTY()
    TObjectPtr<UPhoneBatInput> PhoneBatInput;

    // State
    UPROPERTY()
    ECalibrationStatus Status = ECalibrationStatus::NotStarted;

    UPROPERTY()
    ECalibrationStep CurrentStep = ECalibrationStep::Step0_PlacePhoneFlat;

    UPROPERTY()
    FCalibrationProfile CurrentProfile;

    UPROPERTY()
    FCalibrationResult LastResult;

    UPROPERTY()
    TArray<FCalibrationStepData> CalibrationSteps;

    // Validation state
    UPROPERTY()
    bool bValidating = false;

    UPROPERTY()
    TArray<FVector> ValidationOrientationErrors;

    UPROPERTY()
    TArray<float> ValidationPositionErrors;

    // Internal functions
    void SetupCalibrationSteps();
    FCalibrationStepData CreateStepData(ECalibrationStep Step) const;
    void SendStepToPhone(ECalibrationStep Step);
    void ProcessPhoneResponse(const FPhoneBatCalibrationPayload& Payload);
    FCalibrationResult CalculateResult();
    float CalculateOrientationError(const FQuat& Expected, const FQuat& Actual) const;
    float CalculatePositionError(const FVector& Expected, const FVector& Actual) const;
    void UpdateProgress();
};