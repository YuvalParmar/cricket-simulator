#include "KeyboardMouseBatInput.h"
#include "InputMappingContext.h"

UKeyboardMouseBatInput::UKeyboardMouseBatInput()
{
    CurrentBattingState = FBattingInputState();
    CurrentBowlingState = FBowlingInputState();
    CurrentFieldingState = FFieldingInputState();
}

EInputDeviceType UKeyboardMouseBatInput::GetDeviceType() const
{
    return EInputDeviceType::KeyboardMouse;
}

FString UKeyboardMouseBatInput::GetDeviceName() const
{
    return TEXT("Keyboard & Mouse");
}

bool UKeyboardMouseBatInput::IsConnected() const
{
    return true; // Always connected
}

void UKeyboardMouseBatInput::GetBattingInputState(FBattingInputState& OutState) const
{
    OutState = CurrentBattingState;
}

void UKeyboardMouseBatInput::SetBattingInputState(const FBattingInputState& InState)
{
    CurrentBattingState = InState;
}

void UKeyboardMouseBatInput::GetBowlingInputState(FBowlingInputState& OutState) const
{
    OutState = CurrentBowlingState;
}

void UKeyboardMouseBatInput::SetBowlingInputState(const FBowlingInputState& InState)
{
    CurrentBowlingState = InState;
}

void UKeyboardMouseBatInput::GetFieldingInputState(FFieldingInputState& OutState) const
{
    OutState = CurrentFieldingState;
}

void UKeyboardMouseBatInput::SetFieldingInputState(const FFieldingInputState& InState)
{
    CurrentFieldingState = InState;
}

void UKeyboardMouseBatInput::TriggerHapticFeedback(FGameplayTag FeedbackType, float Intensity)
{
    // Keyboard/mouse doesn't have haptics
    // Could trigger visual feedback instead
}

void UKeyboardMouseBatInput::TriggerAdaptiveTriggerResistance(FGameplayTag TriggerType, float Resistance)
{
    // Not applicable for KB/M
}

void UKeyboardMouseBatInput::StartCalibration()
{
    // No calibration needed for KB/M
    bIsCalibrated = true;
}

bool UKeyboardMouseBatInput::IsCalibrated() const
{
    return bIsCalibrated;
}

UInputMappingContext* UKeyboardMouseBatInput::GetBattingMappingContext() const
{
    return BattingMappingContext;
}

UInputMappingContext* UKeyboardMouseBatInput::GetBowlingMappingContext() const
{
    return BowlingMappingContext;
}

UInputMappingContext* UKeyboardMouseBatInput::GetFieldingMappingContext() const
{
    return FieldingMappingContext;
}

// Batting input handlers
void UKeyboardMouseBatInput::OnShotTypeSelected(int32 ShotTypeIndex)
{
    CurrentBattingState.SelectedShotType = FMath::Clamp(ShotTypeIndex, 0, 20);
}

void UKeyboardMouseBatInput::OnFootworkInput(const FVector2D& Direction, float Magnitude)
{
    CurrentBattingState.FootworkDirection = Direction.GetSafeNormal();
    CurrentBattingState.FootworkMagnitude = FMath::Clamp(Magnitude, 0.0f, 1.0f);
}

void UKeyboardMouseBatInput::OnBatFaceAngleInput(float Angle)
{
    CurrentBattingState.BatFaceAngle = FMath::Clamp(Angle, -90.0f, 90.0f);
}

void UKeyboardMouseBatInput::OnWristRotationInput(float Rotation)
{
    CurrentBattingState.WristRotation = FMath::Clamp(Rotation, -45.0f, 45.0f);
}

void UKeyboardMouseBatInput::OnTimingTriggerPressed()
{
    CurrentBattingState.bTimingTriggerPressed = true;
    CurrentBattingState.TimingTriggerPressure = 1.0f;
}

void UKeyboardMouseBatInput::OnTimingTriggerReleased()
{
    CurrentBattingState.bTimingTriggerPressed = false;
    CurrentBattingState.TimingTriggerPressure = 0.0f;
}

void UKeyboardMouseBatInput::OnTimingTriggerPressure(float Pressure)
{
    CurrentBattingState.TimingTriggerPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
    CurrentBattingState.bTimingTriggerPressed = Pressure > 0.5f;
}

void UKeyboardMouseBatInput::OnPowerModifierPressed(bool bAggressive)
{
    CurrentBattingState.bPowerModifierActive = true;
    CurrentBattingState.bAggressiveMode = bAggressive;
}

void UKeyboardMouseBatInput::OnRunUpMarkerPlaced(const FVector& Location)
{
    CurrentBowlingState.RunUpMarkerLocation = Location;
}

void UKeyboardMouseBatInput::OnRunUpRhythmTap()
{
    CurrentBowlingState.bRunUpRhythmActive = true;
    CurrentBowlingState.RunUpRhythmTiming = 1.0f; // Perfect timing
}

void UKeyboardMouseBatInput::OnDeliveryTypeSelected(int32 DeliveryTypeIndex)
{
    CurrentBowlingState.SelectedDeliveryType = FMath::Clamp(DeliveryTypeIndex, 0, 12);
}

void UKeyboardMouseBatInput::OnAimInput(const FVector2D& AimOffset)
{
    CurrentBowlingState.AimOffset = AimOffset;
}

void UKeyboardMouseBatInput::OnReleasePointInput(float Height, float Angle)
{
    CurrentBowlingState.ReleaseHeight = FMath::Clamp(Height, 0.0f, 3.0f);
    CurrentBowlingState.ReleaseAngle = FMath::Clamp(Angle, -30.0f, 30.0f);
}

void UKeyboardMouseBatInput::OnSeamOrientationInput(const FVector& Orientation)
{
    CurrentBowlingState.SeamOrientation = Orientation.GetSafeNormal();
}

void UKeyboardMouseBatInput::OnSpinAxisInput(const FVector& Axis, float RPM)
{
    CurrentBowlingState.SpinAxis = Axis.GetSafeNormal();
    CurrentBowlingState.SpinRPM = FMath::Max(RPM, 0.0f);
}

void UKeyboardMouseBatInput::OnEffortInput(float Effort)
{
    CurrentBowlingState.Effort = FMath::Clamp(Effort, 0.0f, 1.0f);
}

void UKeyboardMouseBatInput::OnBowlButtonPressed()
{
    CurrentBowlingState.bBowlButtonPressed = true;
    CurrentBowlingState.BowlButtonPressure = 1.0f;
}

void UKeyboardMouseBatInput::OnBowlButtonReleased()
{
    CurrentBowlingState.bBowlButtonPressed = false;
    CurrentBowlingState.BowlButtonPressure = 0.0f;
}

void UKeyboardMouseBatInput::OnBowlButtonPressure(float Pressure)
{
    CurrentBowlingState.BowlButtonPressure = FMath::Clamp(Pressure, 0.0f, 1.0f);
    CurrentBowlingState.bBowlButtonPressed = Pressure > 0.5f;
}

void UKeyboardMouseBatInput::OnMovementInput(const FVector2D& Direction)
{
    CurrentFieldingState.MovementDirection = Direction.GetSafeNormal();
}

void UKeyboardMouseBatInput::OnDiveInput(const FVector2D& Direction)
{
    CurrentFieldingState.bDivePressed = true;
    CurrentFieldingState.DiveDirection = Direction.GetSafeNormal();
}

void UKeyboardMouseBatInput::OnThrowInput(const FVector& TargetLocation, float Power)
{
    CurrentFieldingState.ThrowTarget = TargetLocation;
    CurrentFieldingState.ThrowPower = FMath::Clamp(Power, 0.0f, 1.0f);
}

void UKeyboardMouseBatInput::OnSwitchFielderInput(int32 Direction)
{
    CurrentFieldingState.SwitchFielderDirection = FMath::Clamp(Direction, -1, 1);
}