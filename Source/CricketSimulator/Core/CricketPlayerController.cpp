#include "CricketPlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "CricketInputConfig.h"
#include "Gameplay/CricketBattingPawn.h"
#include "Gameplay/CricketBowlingPawn.h"
#include "Gameplay/CricketFieldingPawn.h"

ACricketPlayerController::ACricketPlayerController()
{
    bReplicates = true;
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;
}

void ACricketPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Apply default mapping context
    if (DefaultMappingContext)
    {
        ApplyMappingContext(DefaultMappingContext, true);
    }

    SetControlMode(ECricketControlMode::Spectator);
}

void ACricketPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogTemp, Error, TEXT("CricketPlayerController: Enhanced Input not enabled!"));
        return;
    }

    // Bind all action sets
    BindCommonActions(EnhancedInput);
    BindBattingActions(EnhancedInput);
    BindBowlingActions(EnhancedInput);
    BindFieldingActions(EnhancedInput);
    BindPhoneBatActions(EnhancedInput);
}

void ACricketPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    // Cache pawn references based on type
    if (ACricketBattingPawn* BatPawn = Cast<ACricketBattingPawn>(InPawn))
    {
        BattingPawn = BatPawn;
        SetControlMode(ECricketControlMode::Batting);
    }
    else if (ACricketBowlingPawn* BowlPawn = Cast<ACricketBowlingPawn>(InPawn))
    {
        BowlingPawn = BowlPawn;
        SetControlMode(ECricketControlMode::Bowling);
    }
    else if (ACricketFieldingPawn* FieldPawn = Cast<ACricketFieldingPawn>(InPawn))
    {
        FieldingPawn = FieldPawn;
        SetControlMode(ECricketControlMode::Fielding);
    }
}

void ACricketPlayerController::OnUnPossess()
{
    BattingPawn.Reset();
    BowlingPawn.Reset();
    FieldingPawn.Reset();

    SetControlMode(ECricketControlMode::Spectator);

    Super::OnUnPossess();
}

void ACricketPlayerController::SetControlMode(ECricketControlMode NewMode)
{
    if (CurrentControlMode == NewMode)
    {
        return;
    }

    // Remove old mapping context
    switch (CurrentControlMode)
    {
        case ECricketControlMode::Batting:
            if (BattingMappingContext) ApplyMappingContext(BattingMappingContext, false);
            break;
        case ECricketControlMode::Bowling:
            if (BowlingMappingContext) ApplyMappingContext(BowlingMappingContext, false);
            break;
        case ECricketControlMode::Fielding:
            if (FieldingMappingContext) ApplyMappingContext(FieldingMappingContext, false);
            break;
        case ECricketControlMode::Spectator:
        default:
            break;
    }

    CurrentControlMode = NewMode;

    // Add new mapping context
    switch (NewMode)
    {
        case ECricketControlMode::Batting:
            if (BattingMappingContext) ApplyMappingContext(BattingMappingContext, true);
            break;
        case ECricketControlMode::Bowling:
            if (BowlingMappingContext) ApplyMappingContext(BowlingMappingContext, true);
            break;
        case ECricketControlMode::Fielding:
            if (FieldingMappingContext) ApplyMappingContext(FieldingMappingContext, true);
            break;
        case ECricketControlMode::Spectator:
        default:
            break;
    }

    UE_LOG(LogTemp, Log, TEXT("Control mode changed to: %s"), *UEnum::GetValueAsString(NewMode));
}

void ACricketPlayerController::ApplyMappingContext(UInputMappingContext* Context, bool bAdd)
{
    if (!Context || !GetLocalPlayer())
    {
        return;
    }

    if (bAdd)
    {
        GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()->AddMappingContext(Context, 0);
    }
    else
    {
        GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()->RemoveMappingContext(Context);
    }
}

// Batting Input Handlers
void ACricketPlayerController::OnShotTypeSelected(int32 ShotTypeIndex)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->SelectShotType(ShotTypeIndex);
    }
}

void ACricketPlayerController::OnFootworkInput(const FVector2D& Direction, float Magnitude)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->SetFootworkInput(Direction, Magnitude);
    }
}

void ACricketPlayerController::OnBatAngleInput(const FVector2D& Angle)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->SetBatAngleInput(Angle.X, Angle.Y); // X = Face, Y = Wrist
    }
}

void ACricketPlayerController::OnTimingTriggerPressed()
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->OnSwingCommit();
    }
}

void ACricketPlayerController::OnTimingTriggerReleased()
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->OnSwingRelease();
    }
}

void ACricketPlayerController::OnPowerModulatorPressed(bool bAggressive)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->SetPowerModifier(bAggressive);
    }
}

// Bowling Input Handlers
void ACricketPlayerController::OnRunUpMarkerPlaced(FVector Location)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetRunUpMarker(Location);
    }
}

void ACricketPlayerController::OnRunUpRhythmInput()
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->OnRunUpRhythmTap();
    }
}

void ACricketPlayerController::OnDeliveryTypeSelected(int32 DeliveryTypeIndex)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SelectDeliveryType(DeliveryTypeIndex);
    }
}

void ACricketPlayerController::OnAimInput(const FVector2D& AimOffset)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetAimOffset(AimOffset);
    }
}

void ACricketPlayerController::OnReleasePointInput(float Height, float Angle)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetReleasePoint(Height, Angle);
    }
}

void ACricketPlayerController::OnSeamOrientationInput(const FVector& Orientation)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetSeamOrientation(Orientation);
    }
}

void ACricketPlayerController::OnSpinAxisInput(const FVector& Axis, float RPM)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetSpinAxis(Axis, RPM);
    }
}

void ACricketPlayerController::OnEffortInput(float Effort)
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->SetEffort(Effort);
    }
}

void ACricketPlayerController::OnBowlButtonPressed()
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->OnBowlButtonPressed();
    }
}

void ACricketPlayerController::OnBowlButtonReleased()
{
    if (BowlingPawn.IsValid())
    {
        BowlingPawn->OnBowlButtonReleased();
    }
}

// Fielding Input Handlers
void ACricketPlayerController::OnMovementInput(const FVector2D& Direction)
{
    if (FieldingPawn.IsValid())
    {
        FieldingPawn->SetMovementInput(Direction);
    }
}

void ACricketPlayerController::OnDiveInput(const FVector2D& Direction)
{
    if (FieldingPawn.IsValid())
    {
        FieldingPawn->OnDive(Direction);
    }
}

void ACricketPlayerController::OnThrowInput(const FVector& TargetLocation, float Power)
{
    if (FieldingPawn.IsValid())
    {
        FieldingPawn->OnThrow(TargetLocation, Power);
    }
}

void ACricketPlayerController::OnSwitchFielderInput(int32 Direction)
{
    // Would communicate with GameMode to switch controlled fielder
    if (ACricketGameMode* GameMode = Cast<ACricketGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // GameMode->SwitchControlledFielder(Direction);
    }
}

// Phone Bat Input Handlers
void ACricketPlayerController::OnPhoneBatPoseUpdate(const FTransform& BatTransform, const FVector& LinearVelocity, const FVector& AngularVelocity)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->UpdatePhoneBatPose(BatTransform, LinearVelocity, AngularVelocity);
    }
}

void ACricketPlayerController::OnPhoneBatSwingPhaseChanged(int32 Phase)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->OnPhoneBatSwingPhaseChanged(Phase);
    }
}

void ACricketPlayerController::OnPhoneBatContactPredicted(float TimeToContact, float Confidence)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->OnPhoneBatContactPredicted(TimeToContact, Confidence);
    }
}

void ACricketPlayerController::OnPhoneBatCalibrationComplete(const FTransform& PhoneToBatTransform)
{
    if (BattingPawn.IsValid())
    {
        BattingPawn->SetPhoneBatCalibration(PhoneToBatTransform);
    }
}

// Camera
void ACricketPlayerController::ToggleCameraPerspective()
{
    SetCameraPerspective(!bFirstPersonCamera);
}

void ACricketPlayerController::SetCameraPerspective(bool bFirstPerson)
{
    bFirstPersonCamera = bFirstPerson;

    if (BattingPawn.IsValid())
    {
        BattingPawn->SetCameraPerspective(bFirstPerson);
    }
}

// Pawn Getters
ACricketBattingPawn* ACricketPlayerController::GetBattingPawn() const
{
    return BattingPawn.Get();
}

ACricketBowlingPawn* ACricketPlayerController::GetBowlingPawn() const
{
    return BowlingPawn.Get();
}

ACricketFieldingPawn* ACricketPlayerController::GetFieldingPawn() const
{
    return FieldingPawn.Get();
}

// Input Binding Functions
void ACricketPlayerController::BindCommonActions(UEnhancedInputComponent* EnhancedInput)
{
    if (!InputConfig) return;

    // Camera toggle - available in all modes
    if (InputConfig->ToggleCameraAction)
    {
        EnhancedInput->BindAction(InputConfig->ToggleCameraAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::ToggleCameraPerspective);
    }

    // Pause menu
    if (InputConfig->PauseAction)
    {
        EnhancedInput->BindAction(InputConfig->PauseAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::PauseMatch);
    }
}

void ACricketPlayerController::BindBattingActions(UEnhancedInputComponent* EnhancedInput)
{
    if (!InputConfig) return;

    // Shot type selection (1-9 keys / D-pad)
    if (InputConfig->ShotTypeAction)
    {
        EnhancedInput->BindAction(InputConfig->ShotTypeAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnShotTypeSelected);
    }

    // Footwork (WASD / Left Stick)
    if (InputConfig->FootworkAction)
    {
        EnhancedInput->BindAction(InputConfig->FootworkAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnFootworkInput);
    }

    // Bat angle (Mouse / Right Stick)
    if (InputConfig->BatAngleAction)
    {
        EnhancedInput->BindAction(InputConfig->BatAngleAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnBatAngleInput);
    }

    // Timing trigger (Space / R2)
    if (InputConfig->TimingTriggerAction)
    {
        EnhancedInput->BindAction(InputConfig->TimingTriggerAction, ETriggerEvent::Started, this, &ACricketPlayerController::OnTimingTriggerPressed);
        EnhancedInput->BindAction(InputConfig->TimingTriggerAction, ETriggerEvent::Completed, this, &ACricketPlayerController::OnTimingTriggerReleased);
    }

    // Power modifier (Shift / L2)
    if (InputConfig->PowerModAction)
    {
        EnhancedInput->BindAction(InputConfig->PowerModAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnPowerModulatorPressed);
    }
}

void ACricketPlayerController::BindBowlingActions(UEnhancedInputComponent* EnhancedInput)
{
    if (!InputConfig) return;

    // Run-up marker placement
    if (InputConfig->RunUpMarkerAction)
    {
        EnhancedInput->BindAction(InputConfig->RunUpMarkerAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnRunUpMarkerPlaced);
    }

    // Run-up rhythm tap
    if (InputConfig->RunUpRhythmAction)
    {
        EnhancedInput->BindAction(InputConfig->RunUpRhythmAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnRunUpRhythmInput);
    }

    // Delivery type selection
    if (InputConfig->DeliveryTypeAction)
    {
        EnhancedInput->BindAction(InputConfig->DeliveryTypeAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnDeliveryTypeSelected);
    }

    // Aim input
    if (InputConfig->AimAction)
    {
        EnhancedInput->BindAction(InputConfig->AimAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnAimInput);
    }

    // Release point
    if (InputConfig->ReleasePointAction)
    {
        EnhancedInput->BindAction(InputConfig->ReleasePointAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnReleasePointInput);
    }

    // Seam orientation
    if (InputConfig->SeamOrientationAction)
    {
        EnhancedInput->BindAction(InputConfig->SeamOrientationAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnSeamOrientationInput);
    }

    // Spin axis
    if (InputConfig->SpinAxisAction)
    {
        EnhancedInput->BindAction(InputConfig->SpinAxisAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnSpinAxisInput);
    }

    // Effort
    if (InputConfig->EffortAction)
    {
        EnhancedInput->BindAction(InputConfig->EffortAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnEffortInput);
    }

    // Bowl button
    if (InputConfig->BowlAction)
    {
        EnhancedInput->BindAction(InputConfig->BowlAction, ETriggerEvent::Started, this, &ACricketPlayerController::OnBowlButtonPressed);
        EnhancedInput->BindAction(InputConfig->BowlAction, ETriggerEvent::Completed, this, &ACricketPlayerController::OnBowlButtonReleased);
    }
}

void ACricketPlayerController::BindFieldingActions(UEnhancedInputComponent* EnhancedInput)
{
    if (!InputConfig) return;

    // Movement
    if (InputConfig->MoveAction)
    {
        EnhancedInput->BindAction(InputConfig->MoveAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnMovementInput);
    }

    // Dive
    if (InputConfig->DiveAction)
    {
        EnhancedInput->BindAction(InputConfig->DiveAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnDiveInput);
    }

    // Throw
    if (InputConfig->ThrowAction)
    {
        EnhancedInput->BindAction(InputConfig->ThrowAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnThrowInput);
    }

    // Switch fielder
    if (InputConfig->SwitchFielderAction)
    {
        EnhancedInput->BindAction(InputConfig->SwitchFielderAction, ETriggerEvent::Triggered, this, &ACricketPlayerController::OnSwitchFielderInput);
    }
}

void ACricketPlayerController::BindPhoneBatActions(UEnhancedInputComponent* EnhancedInput)
{
    if (!InputConfig) return;

    // Phone bat actions are typically driven by network messages from the companion app
    // These would be bound to custom delegates that receive UDP/Bluetooth packets
    // For now, we expose the functions as BlueprintCallable for the PhoneBatReceiverComponent to call
}