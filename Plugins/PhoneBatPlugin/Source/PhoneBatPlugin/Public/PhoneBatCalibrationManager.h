#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkManager.h"
#include "PhoneBatCalibrationManager.generated.h"

USTRUCT(BlueprintType)
struct FPhoneBatCalibrationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FTransform PhoneToBatTransform;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FVector GripOffset;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    FRotator GripRotation;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float SweetSpotDistance = 0.18f;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float BatLength = 0.965f;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    float QualityScore = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Calibration")
    int64 CalibrationTimestamp = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationReceived, const FPhoneBatCalibrationResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCalibrationStep, int32, Step, const FString&);

UCLASS(BlueprintType)
class PHONEBATPLUGIN_API UPhoneBatCalibrationManager : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatCalibrationManager();

    // Singleton access
    static UPhoneBatCalibrationManager& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Process calibration packet
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Calibration")
    void ProcessCalibrationPacket(const FPhoneBatCalibrationPayload& Payload);

    // Get calibration result
    UFUNCTION(BlueprintPure, Category = "PhoneBat|Calibration")
    const FPhoneBatCalibrationResult& GetResult() const { return CurrentResult; }

    // Check if calibration is complete
    UFUNCTION(BlueprintPure, Category = "PhoneBat|Calibration")
    bool IsCalibrationComplete() const { return CurrentResult.bIsValid; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnCalibrationReceived OnCalibrationReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnCalibrationStep OnCalibrationStepChanged;

protected:
    UPROPERTY()
    FPhoneBatCalibrationResult CurrentResult;

    void ComputeTransformFromSamples(const TArray<FQuat>& Quaternions, const TArray<FVector>& Positions);
};