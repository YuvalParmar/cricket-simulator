#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IBatInputInterface.h"
#include "KeyboardMouseBatInput.generated.h"

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API UKeyboardMouseBatInput : public UObject, public IBatInputInterface
{
    GENERATED_BODY()

public:
    UKeyboardMouseBatInput();

    // IBatInputInterface implementation
    virtual EInputDeviceType GetDeviceType() const override;
    virtual FString GetDeviceName() const override;
    virtual bool IsConnected() const override;

    virtual void GetBattingInputState(FBattingInputState& OutState) const override;
    virtual void SetBattingInputState(const FBattingInputState& InState) override;

    virtual void GetBowlingInputState(FBowlingInputState& OutState) const override;
    virtual void SetBowlingInputState(const FBowlingInputState& InState) override;

    virtual void GetFieldingInputState(FFieldingInputState& OutState) const override;
    virtual void SetFieldingInputState(const FFieldingInputState& InState) override;

    virtual void TriggerHapticFeedback(FGameplayTag FeedbackType, float Intensity = 1.0f) override;
    virtual void TriggerAdaptiveTriggerResistance(FGameplayTag TriggerType, float Resistance = 1.0f) override;

    virtual void StartCalibration() override;
    virtual bool IsCalibrated() const override;

    virtual UInputMappingContext* GetBattingMappingContext() const override;
    virtual UInputMappingContext* GetBowlingMappingContext() const override;
    virtual UInputMappingContext* GetFieldingMappingContext() const override;

    // Input event handlers (called from PlayerController)
    void OnShotTypeSelected(int32 ShotTypeIndex);
    void OnFootworkInput(const FVector2D& Direction, float Magnitude);
    void OnBatFaceAngleInput(float Angle);
    void OnWristRotationInput(float Rotation);
    void OnTimingTriggerPressed();
    void OnTimingTriggerReleased();
    void OnTimingTriggerPressure(float Pressure);
    void OnPowerModifierPressed(bool bAggressive);
    void OnRunUpMarkerPlaced(const FVector& Location);
    void OnRunUpRhythmTap();
    void OnDeliveryTypeSelected(int32 DeliveryTypeIndex);
    void OnAimInput(const FVector2D& AimOffset);
    void OnReleasePointInput(float Height, float Angle);
    void OnSeamOrientationInput(const FVector& Orientation);
    void OnSpinAxisInput(const FVector& Axis, float RPM);
    void OnEffortInput(float Effort);
    void OnBowlButtonPressed();
    void OnBowlButtonReleased();
    void OnBowlButtonPressure(float Pressure);
    void OnMovementInput(const FVector2D& Direction);
    void OnDiveInput(const FVector2D& Direction);
    void OnThrowInput(const FVector& TargetLocation, float Power);
    void OnSwitchFielderInput(int32 Direction);

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> BattingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> BowlingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> FieldingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float MouseSensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float KeyboardRepeatDelay = 0.2f;

protected:
    // Current input states
    FBattingInputState CurrentBattingState;
    FBowlingInputState CurrentBowlingState;
    FFieldingInputState CurrentFieldingState;

    bool bIsCalibrated = true; // KB/M doesn't need calibration
};