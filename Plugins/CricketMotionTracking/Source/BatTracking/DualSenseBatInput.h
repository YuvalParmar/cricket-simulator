#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "IBatInputInterface.h"
#include "DualSenseBatInput.generated.h"

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API UDualSenseBatInput : public UObject, public IBatInputInterface
{
    GENERATED_BODY()

public:
    UDualSenseBatInput();

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

    // DualSense specific functions
    UFUNCTION(BlueprintCallable, Category = "DualSense")
    void SetLightBarColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "DualSense")
    void SetPlayerIndicator(int32 PlayerIndex);

    UFUNCTION(BlueprintCallable, Category = "DualSense")
    void SetMicLED(bool bEnabled, FLinearColor Color = FLinearColor::White);

    // Haptic presets for cricket
    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayBatImpactFeedback(float Intensity, bool bSweetSpot);

    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayBallHitPadFeedback();

    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayEdgeFeedback();

    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayRunUpRhythmFeedback();

    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayWicketFeedback();

    UFUNCTION(BlueprintCallable, Category = "DualSense|Haptics")
    void PlayBoundaryFeedback();

    // Adaptive trigger profiles
    UFUNCTION(BlueprintCallable, Category = "DualSense|Triggers")
    void SetTriggerProfile_Batting(float BatWeight);

    UFUNCTION(BlueprintCallable, Category = "DualSense|Triggers")
    void SetTriggerProfile_Bowling(float Effort);

    UFUNCTION(BlueprintCallable, Category = "DualSense|Triggers")
    void SetTriggerProfile_Fielding();

    UFUNCTION(BlueprintCallable, Category = "DualSense|Triggers")
    void ResetTriggerProfile();

    // Connection management
    UFUNCTION(BlueprintCallable, Category = "DualSense")
    bool ConnectController(int32 ControllerIndex);

    UFUNCTION(BlueprintCallable, Category = "DualSense")
    void DisconnectController();

    // Input event handlers (called from PlayerController)
    void OnShotTypeSelected(int32 ShotTypeIndex);
    void OnFootworkInput(const FVector2D& Direction, float Magnitude);
    void OnBatAngleInput(const FVector2D& Angle); // X=Face, Y=Wrist
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

protected:
    // Current input states
    FBattingInputState CurrentBattingState;
    FBowlingInputState CurrentBowlingState;
    FFieldingInputState CurrentFieldingState;

    // Controller state
    bool bControllerConnected = false;
    int32 ControllerIndex = 0;
    bool bIsCalibrated = false;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> BattingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> BowlingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UInputMappingContext> FieldingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float StickDeadZone = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float TriggerDeadZone = 0.05f;

    // Haptic curves (would be DataAssets in practice)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Haptics")
    TObjectPtr<class UCurveFloat> BatImpactCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Haptics")
    TObjectPtr<class UCurveFloat> RunUpRhythmCurve;
};