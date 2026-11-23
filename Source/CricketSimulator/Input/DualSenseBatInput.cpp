#include "DualSenseBatInput.h"
#include "InputMappingContext.h"
#include "Gamepad/InputDevice.h"

UDualSenseBatInput::UDualSenseBatInput()
{
    CurrentBattingState = FBattingInputState();
    CurrentBowlingState = FBowlingInputState();
    CurrentFieldingState = FFieldingInputState();
}

EInputDeviceType UDualSenseBatInput::GetDeviceType() const
{
    return EInputDeviceType::DualSense;
}

FString UDualSenseBatInput::GetDeviceName() const
{
    return TEXT("PS5 DualSense");
}

bool UDualSenseBatInput::IsConnected() const
{
    return bControllerConnected;
}

void UDualSenseBatInput::GetBattingInputState(FBattingInputState& OutState) const
{
    OutState = CurrentBattingState;
}

void UDualSenseBatInput::SetBattingInputState(const FBattingInputState& InState)
{
    CurrentBattingState = InState;
}

void UDualSenseBatInput::GetBowlingInputState(FBowlingInputState& OutState) const
{
    OutState = CurrentBowlingState;
}

void UDualSenseBatInput::SetBowlingInputState(const FBowlingInputState& InState)
{
    CurrentBowlingState = InState;
}

void UDualSenseBatInput::GetFieldingInputState(FFieldingInputState& OutState) const
{
    OutState = CurrentFieldingState;
}

void UDualSenseBatInput::SetFieldingInputState(const FFieldingInputState& InState)
{
    CurrentFieldingState = InState;
}

void UDualSenseBatInput::TriggerHapticFeedback(FGameplayTag FeedbackType, float Intensity)
{
    if (!bControllerConnected)
    {
        return;
    }

    // In a real implementation, this would use the DualSense haptic API
    // For now, we log the feedback type
    UE_LOG(LogTemp, Log, TEXT("DualSense Haptic: %s (Intensity: %.2f)"), *FeedbackType.ToString(), Intensity);
}

void UDualSenseBatInput::TriggerAdaptiveTriggerResistance(FGameplayTag TriggerType, float Resistance)
{
    if (!bControllerConnected)
    {
        return;
    }

    // Would use DualSense adaptive trigger API
    UE_LOG(LogTemp, Log, TEXT("DualSense Adaptive Trigger: %s (Resistance: %.2f)"), *TriggerType.ToString(), Resistance);
}

void UDualSenseBatInput::StartCalibration()
{
    // DualSense calibration: center sticks, check triggers
    bIsCalibrated = bControllerConnected;
}

bool UDualSenseBatInput::IsCalibrated() const
{
    return bIsCalibrated;
}

UInputMappingContext* UDualSenseBatInput::GetBattingMappingContext() const
{
    return BattingMappingContext;
}

UInputMappingContext* UDualSenseBatInput::GetBowlingMappingContext() const
{
    return BowlingMappingContext;
}

UInputMappingContext* UDualSenseBatInput::GetFieldingMappingContext() const
{
    return FieldingMappingContext;
}

// DualSense specific functions
void UDualSenseBatInput::SetLightBarColor(FLinearColor Color)
{
    if (!bControllerConnected)
    {
        return;
    }

    // Would use DualSense light bar API
    UE_LOG(LogTemp, Log, TEXT("DualSense Light Bar: R=%.2f G=%.2f B=%.2f"), Color.R, Color.G, Color.B);
}

void UDualSenseBatInput::SetPlayerIndicator(int32 PlayerIndex)
{
    if (!bControllerConnected)
    {
        return;
    }

    // Would use DualSense player indicator API (5 LEDs)
    UE_LOG(LogTemp, Log, TEXT("DualSense Player Indicator: %d"), PlayerIndex);
}

void UDualSenseBatInput::SetMicLED(bool bEnabled, FLinearColor Color)
{
    if (!bControllerConnected)
    {
        return;
    }

    // Would use DualSense mic LED API
    UE_LOG(LogTemp, Log, TEXT("DualSense Mic LED: %s"), bEnabled ? TEXT("On") : TEXT("Off"));
}

// Haptic presets
void UDualSenseBatInput::PlayBatImpactFeedback(float Intensity, bool bSweetSpot)
{
    if (!bControllerConnected)
    {
        return;
    }

    FGameplayTag Tag = bSweetSpot ? FGameplayTag::RequestGameplayTag("Haptic.BatImpact.SweetSpot")
                                   : FGameplayTag::RequestGameplayTag("Haptic.BatImpact.Normal");
    TriggerHapticFeedback(Tag, Intensity);

    // Light bar feedback
    if (bSweetSpot)
    {
        SetLightBarColor(FLinearColor::Green);
    }
    else
    {
        SetLightBarColor(FLinearColor::Yellow);
    }
}

void UDualSenseBatInput::PlayBallHitPadFeedback()
{
    TriggerHapticFeedback(FGameplayTag::RequestGameplayTag("Haptic.BallHitPad"), 0.7f);
    SetLightBarColor(FLinearColor::Blue);
}

void UDualSenseBatInput::PlayEdgeFeedback()
{
    TriggerHapticFeedback(FGameplayTag::RequestGameplayTag("Haptic.BatImpact.Edge"), 1.0f);
    SetLightBarColor(FLinearColor::Red);
}

void UDualSenseBatInput::PlayRunUpRhythmFeedback()
{
    TriggerHapticFeedback(FGameplayTag::RequestGameplayTag("Haptic.RunUpRhythm"), 0.5f);
}

void UDualSenseBatInput::PlayWicketFeedback()
{
    TriggerHapticFeedback(FGameplayTag::RequestGameplayTag("Haptic.Wicket"), 1.0f);
    SetLightBarColor(FLinearColor::Red);
}

void UDualSenseBatInput::PlayBoundaryFeedback()
{
    TriggerHapticFeedback(FGameplayTag::RequestGameplayTag("Haptic.Boundary"), 1.0f);
    SetLightBarColor(FLinearColor::Green);
}

// Adaptive trigger profiles
void UDualSenseBatInput::SetTriggerProfile_Batting(float BatWeight)
{
    // R2 = Swing trigger, resistance simulates bat weight
    // L2 = Power modifier, light resistance
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Batting.Swing"), BatWeight);
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Batting.Power"), 0.3f);
}

void UDualSenseBatInput::SetTriggerProfile_Bowling(float Effort)
{
    // R2 = Run-up/bowl, resistance increases with effort
    // L2 = Aim assist, light resistance
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Bowling.RunUp"), Effort);
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Bowling.Aim"), 0.2f);
}

void UDualSenseBatInput::SetTriggerProfile_Fielding()
{
    // R2 = Throw power, progressive resistance
    // L2 = Dive, clicky resistance
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Fielding.Throw"), 0.5f);
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Fielding.Dive"), 0.8f);
}

void UDualSenseBatInput::ResetTriggerProfile()
{
    TriggerAdaptiveTriggerResistance(FGameplayTag::RequestGameplayTag("Trigger.Default"), 0.0f);
}

// Connection management
bool UDualSenseBatInput::ConnectController(int32 InControllerIndex)
{
    // In a real implementation, this would use the DualSense SDK or SteamInput
    // For now, we simulate connection
    ControllerIndex = InControllerIndex;
    bControllerConnected = true;
    bIsCalibrated = true;

    UE_LOG(LogTemp, Log, TEXT("DualSense connected at index %d"), ControllerIndex);

    // Set default light bar color
    SetLightBarColor(FLinearColor::White);

    return true;
}

void UDualSenseBatInput::DisconnectController()
{
    bControllerConnected = false;
    bIsCalibrated = false;
    ResetTriggerProfile();
    UE_LOG(LogTemp, Log, TEXT("DualSense disconnected"));
}

// Input event handlers
void UDualSenseBatInput::OnShotTypeSelected(int32 ShotTypeIndex)
{
    CurrentBattingState.SelectedShotType = FMath::Clamp(ShotTypeIndex, 0, 20);
}

void UDualSenseBatInput::OnFootworkInput(const FVector2D& Direction, float Magnitude)
{
    // Apply dead zone
    float Mag = Magnitude;
    if (Mag < StickDeadZone)
    {
        Mag = 0.0f;
    }
    else
    {
        Mag = (Mag - StickDeadZone) / (1.0f - StickDeadZone);
    }

    CurrentBattingState.FootworkDirection = Direction.GetSafeNormal();
    CurrentBattingState.FootworkMagnitude = FMath::Clamp(Mag, 0.0f, 1.0f);
}

void UDualSenseBatInput::OnBatAngleInput(const FVector2D& Angle)
{
    // X = Bat face angle (right stick X), Y = Wrist rotation (right stick Y)
    CurrentBattingState.BatFaceAngle = FMath::Clamp(Angle.X * 90.0f, -90.0f, 90.0f);
    CurrentBattingState.WristRotation = FMath::Clamp(Angle.Y * 45.0f, -45.0f, 45.0f);
}

void UDualSenseBatInput::OnTimingTriggerPressed()
{
    CurrentBattingState.bTimingTriggerPressed = true;
    CurrentBattingState.TimingTriggerPressure = 1.0f;
}

void UDualSenseBatInput::OnTimingTriggerReleased()
{
    CurrentBattingState.bTimingTriggerPressed = false;
    CurrentBattingState.TimingTriggerPressure = 0.0f;
}

void UDualSenseBatInput::OnTimingTriggerPressure(float Pressure)
{
    // Apply dead zone
    float P = Pressure;
    if (P < TriggerDeadZone)
    {
        P = 0.0f;
    }
    else
    {
        P = (P - TriggerDeadZone) / (1.0f - TriggerDeadZone);
    }

    CurrentBattingState.TimingTriggerPressure = FMath::Clamp(P, 0.0f, 1.0f);
    CurrentBattingState.bTimingTriggerPressed = P > 0.5f;
}

void UDualSenseBatInput::OnPowerModifierPressed(bool bAggressive)
{
    CurrentBattingState.bPowerModifierActive = true;
    CurrentBattingState.bAggressiveMode = bAggressive;
}

void UDualSenseBatInput::OnRunUpMarkerPlaced(const FVector& Location)
{
    CurrentBowlingState.RunUpMarkerLocation = Location;
}

void UDualSenseBatInput::OnRunUpRhythmTap()
{
    CurrentBowlingState.bRunUpRhythmActive = true;
    CurrentBowlingState.RunUpRhythmTiming = 1.0f;
}

void UDualSenseBatInput::OnDeliveryTypeSelected(int32 DeliveryTypeIndex)
{
    CurrentBowlingState.SelectedDeliveryType = FMath::Clamp(DeliveryTypeIndex, 0, 12);
}

void UDualSenseBatInput::OnAimInput(const FVector2D& AimOffset)
{
    // Apply dead zone
    FVector2D Offset = AimOffset;
    float Mag = Offset.Size();
    if (Mag < StickDeadZone)
    {
        Offset = FVector2D::ZeroVector;
    }
    else
    {
        Offset = Offset.GetSafeNormal() * ((Mag - StickDeadZone) / (1.0f - StickDeadZone));
    }

    CurrentBowlingState.AimOffset = Offset;
}

void UDualSenseBatInput::OnReleasePointInput(float Height, float Angle)
{
    CurrentBowlingState.ReleaseHeight = FMath::Clamp(Height, 0.0f, 3.0f);
    CurrentBowlingState.ReleaseAngle = FMath::Clamp(Angle, -30.0f, 30.0f);
}

void UDualSenseBatInput::OnSeamOrientationInput(const FVector& Orientation)
{
    CurrentBowlingState.SeamOrientation = Orientation.GetSafeNormal();
}

void UDualSenseBatInput::OnSpinAxisInput(const FVector& Axis, float RPM)
{
    CurrentBowlingState.SpinAxis = Axis.GetSafeNormal();
    CurrentBowlingState.SpinRPM = FMath::Max(RPM, 0.0f);
}

void UDualSenseBatInput::OnEffortInput(float Effort)
{
    CurrentBowlingState.Effort = FMath::Clamp(Effort, 0.0f, 1.0f);

    // Update adaptive trigger resistance in real-time
    SetTriggerProfile_Bowling(Effort);
}

void UDualSenseBatInput::OnBowlButtonPressed()
{
    CurrentBowlingState.bBowlButtonPressed = true;
    CurrentBowlingState.BowlButtonPressure = 1.0f;
}

void UDualSenseBatInput::OnBowlButtonReleased()
{
    CurrentBowlingState.bBowlButtonPressed = false;
    CurrentBowlingState.BowlButtonPressure = 0.0f;
}

void UDualSenseBatInput::OnBowlButtonPressure(float Pressure)
{
    // Apply dead zone
    float P = Pressure;
    if (P < TriggerDeadZone)
    {
        P = 0.0f;
    }
    else
    {
        P = (P - TriggerDeadZone) / (1.0f - TriggerDeadZone);
    }

    CurrentBowlingState.BowlButtonPressure = FMath::Clamp(P, 0.0f, 1.0f);
    CurrentBowlingState.bBowlButtonPressed = P > 0.5f;
}

void UDualSenseBatInput::OnMovementInput(const FVector2D& Direction)
{
    // Apply dead zone
    FVector2D Dir = Direction;
    float Mag = Dir.Size();
    if (Mag < StickDeadZone)
    {
        Dir = FVector2D::ZeroVector;
    }
    else
    {
        Dir = Dir.GetSafeNormal() * ((Mag - StickDeadZone) / (1.0f - StickDeadZone));
    }

    CurrentFieldingState.MovementDirection = Dir;
}

void UDualSenseBatInput::OnDiveInput(const FVector2D& Direction)
{
    CurrentFieldingState.bDivePressed = true;
    CurrentFieldingState.DiveDirection = Direction.GetSafeNormal();
}

void UDualSenseBatInput::OnThrowInput(const FVector& TargetLocation, float Power)
{
    CurrentFieldingState.ThrowTarget = TargetLocation;
    CurrentFieldingState.ThrowPower = FMath::Clamp(Power, 0.0f, 1.0f);
}

void UDualSenseBatInput::OnSwitchFielderInput(int32 Direction)
{
    CurrentFieldingState.SwitchFielderDirection = FMath::Clamp(Direction, -1, 1);
}