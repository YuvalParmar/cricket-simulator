#include "CricketFieldingPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CricketFieldingComponent.h"
#include "CricketBall.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "CricketGameplayAbilities.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Controller.h"
#include "AIBlueprintHelperLibrary.h"

UAbilitySystemComponent* ACricketFieldingPawn::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

ACricketFieldingPawn::ACricketFieldingPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Configure character movement
    GetCharacterMovement()->MaxWalkSpeed = 600.0f; // 6 m/s base
    GetCharacterMovement()->MaxWalkSpeedCrouched = 300.0f;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
    GetCharacterMovement()->JumpZVelocity = 400.0f;
    GetCharacterMovement()->AirControl = 0.2f;

    // Camera
    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(RootComponent);
    CameraSpringArm->TargetArmLength = 400.0f;
    CameraSpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
    CameraSpringArm->bUsePawnControlRotation = true;
    CameraSpringArm->bInheritPitch = true;
    CameraSpringArm->bInheritYaw = true;
    CameraSpringArm->bInheritRoll = false;
    CameraSpringArm->bDoCollisionTest = true;
    CameraSpringArm->CameraLagSpeed = 5.0f;

    FieldingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FieldingCamera"));
    FieldingCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
    FieldingCamera->bUsePawnControlRotation = false;

    // Component
    FieldingComponent = CreateDefaultSubobject<UCricketFieldingComponent>(TEXT("FieldingComponent"));

    // Defaults
    FieldingPosition = EFieldingPosition::Cover;
    CurrentAction = EFieldingAction::None;
    bAIControlled = true;
    HomePosition = FVector::ZeroVector;
    DiveTimeRemaining = 0.0f;
    ThrowChargeTime = 0.0f;
    CatchWindowDuration = 0.3f;

    // Default attributes
    Attributes = FFieldingAttributes();
}

void ACricketFieldingPawn::BeginPlay()
{
    Super::BeginPlay();

    // Initialize GAS
    InitializeAbilities();

    // Set home position based on fielding position
    HomePosition = GetPositionCoordinates(FieldingPosition);
    SetActorLocation(HomePosition);

    // Set movement speed based on attributes
    GetCharacterMovement()->MaxWalkSpeed = 600.0f * Attributes.Speed;
}

void ACricketFieldingPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bAIControlled)
    {
        UpdateFieldingAI(DeltaSeconds);
    }
    else
    {
        UpdateMovement(DeltaSeconds);
    }

    UpdateDive(DeltaSeconds);
    UpdateThrow(DeltaSeconds);
    UpdateCatch(DeltaSeconds);

    // Update GAS attributes
    UpdateGASAttributes(DeltaSeconds);
}

void ACricketFieldingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Enhanced input bindings handled by PlayerController
}

void ACricketFieldingPawn::SetFieldingPosition(EFieldingPosition Position)
{
    FieldingPosition = Position;
    HomePosition = GetPositionCoordinates(Position);

    if (bAIControlled)
    {
        MoveToPosition(HomePosition);
    }
}

void ACricketFieldingPawn::MoveToPosition(const FVector& TargetLocation, bool bSprint)
{
    if (bAIControlled)
    {
        // Use AI move to
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(GetController(), TargetLocation);
    }
    else
    {
        // Player controlled - movement input handles this
        bWantsToSprint = bSprint;
    }
}

void ACricketFieldingPawn::ReturnToPosition()
{
    MoveToPosition(HomePosition, true);
}

void ACricketFieldingPawn::SetMovementInput(const FVector2D& Direction)
{
    MovementInput = Direction.GetSafeNormal();

    if (!bAIControlled)
    {
        // Apply movement input
        FVector WorldDirection = FVector(Direction.X, Direction.Y, 0.0f);
        if (!WorldDirection.IsNearlyZero())
        {
            AddMovementInput(WorldDirection, 1.0f, false);
        }
    }
}

void ACricketFieldingPawn::OnDive(const FVector2D& Direction)
{
    if (CurrentAction == EFieldingAction::Dive || CurrentAction == EFieldingAction::Slide)
    {
        return; // Already diving
    }

    DiveDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
    DiveTimeRemaining = 1.0f; // 1 second dive
    SetAction(EFieldingAction::Dive);

    // Launch character
    FVector LaunchVelocity = DiveDirection * 1500.0f * Attributes.Agility;
    LaunchVelocity.Z = 200.0f;
    LaunchCharacter(LaunchVelocity, true, true);

    // Play dive animation
    PlayFieldingAnimation(FName("Dive"), 1.0f);
}

void ACricketFieldingPawn::OnThrow(const FVector& TargetLocation, float Power)
{
    if (CurrentAction == EFieldingAction::Throw)
    {
        return;
    }

    ThrowTarget = TargetLocation;
    ThrowPower = FMath::Clamp(Power, 0.0f, 1.0f);
    ThrowChargeTime = 0.0f;
    SetAction(EFieldingAction::Throw);

    // Play throw windup animation
    PlayFieldingAnimation(FName("Throw_Windup"), 0.5f);
}

void ACricketFieldingPawn::AttemptCatch()
{
    if (bCatchAttempted)
    {
        return;
    }

    bCatchAttempted = true;
    CatchWindowStart = GetWorld()->GetTimeSeconds();
    SetAction(EFieldingAction::Catch);

    // Play catch animation
    PlayFieldingAnimation(FName("Catch"), 1.0f);
}

void ACricketFieldingPawn::AttemptRunOut(const FVector& StumpLocation)
{
    OnThrow(StumpLocation, 1.0f); // Maximum power for run-out
}

void ACricketFieldingPawn::AttemptStumping()
{
    // Wicketkeeper specific - quick throw to stumps
    SetAction(EFieldingAction::Stumping);
    PlayFieldingAnimation(FName("Stumping"), 1.5f);
}

void ACricketFieldingPawn::EnableAIControl(bool bEnable)
{
    bAIControlled = bEnable;

    if (bEnable)
    {
        GetCharacterMovement()->bOrientRotationToMovement = true;
        MoveToPosition(HomePosition);
    }
    else
    {
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
}

void ACricketFieldingPawn::SetBallTarget(AActor* BallActor)
{
    BallTarget = BallActor;
}

void ACricketFieldingPawn::SetPriorityTarget(const FVector& Location, float Priority)
{
    PriorityTargetLocation = Location;
    PriorityTargetWeight = Priority;
}

float ACricketFieldingPawn::GetDistanceToBall() const
{
    if (BallTarget)
    {
        return FVector::Dist(GetActorLocation(), BallTarget->GetActorLocation());
    }
    return FLT_MAX;
}

void ACricketFieldingPawn::UpdateFieldingAI(float DeltaSeconds)
{
    if (!BallTarget)
    {
        // No ball - return to home position
        float DistToHome = FVector::Dist2D(GetActorLocation(), HomePosition);
        if (DistToHome > 100.0f)
        {
            MoveToPosition(HomePosition);
        }
        return;
    }

    ACricketBall* Ball = Cast<ACricketBall>(BallTarget);
    if (!Ball)
    {
        return;
    }

    // Predict ball trajectory
    FVector BallPos = Ball->GetActorLocation();
    FVector BallVel = Ball->GetVelocity();
    float BallSpeed = BallVel.Size();

    // Calculate interception point
    CalculateInterceptionPoint();

    // Determine if we can reach the ball
    float TimeToBall = 0.0f;
    if (BallSpeed > 10.0f)
    {
        TimeToBall = FVector::Dist2D(GetActorLocation(), BallPos) / (600.0f * Attributes.Speed);
    }

    bool bCanReach = CanReachBall(TimeToBall);

    // Check for catch opportunity
    if (Ball->GetBallState() == EBallState::InFlight && bCanReach && BallPos.Z < 300.0f)
    {
        // Ball is catchable height
        float TimeUntilCatch = (BallPos.Z - 50.0f) / FMath::Abs(BallVel.Z); // Time until ball at catch height

        if (TimeUntilCatch > 0.1f && TimeUntilCatch < 2.0f)
        {
            // Move to interception point and attempt catch
            MoveToPosition(PriorityTargetLocation);
            AttemptCatch();
        }
    }
    // Ground fielding
    else if (Ball->GetBallState() == EBallState::OnPitch || Ball->GetBallState() == EBallState::Bouncing)
    {
        if (bCanReach)
        {
            MoveToPosition(PriorityTargetLocation);

            // Dive if close but not quite reachable
            float DistToBall = FVector::Dist2D(GetActorLocation(), BallPos);
            if (DistToBall > 200.0f && DistToBall < 500.0f && Attributes.Agility > 1.2f)
            {
                FVector2D Dir = FVector2D(BallPos - GetActorLocation()).GetSafeNormal();
                OnDive(Dir);
            }
        }
    }
    // Ball stopped - pick up and throw
    else if (Ball->GetBallState() == EBallState::Dead && bCanReach)
    {
        MoveToPosition(BallPos);

        // If close enough, pick up and throw
        if (FVector::Dist(GetActorLocation(), BallPos) < 100.0f)
        {
            // Find target (wicketkeeper or bowler's end)
            FVector ThrowTargetLoc = FVector::ZeroVector; // Would find actual target
            OnThrow(ThrowTargetLoc, 0.8f);
        }
    }
}

void ACricketFieldingPawn::UpdateMovement(float DeltaSeconds)
{
    if (bWantsToSprint)
    {
        GetCharacterMovement()->MaxWalkSpeed = 1000.0f * Attributes.Speed;
    }
    else
    {
        GetCharacterMovement()->MaxWalkSpeed = 600.0f * Attributes.Speed;
    }

    // Apply movement input
    if (!MovementInput.IsNearlyZero())
    {
        FVector WorldDirection = FVector(MovementInput.X, MovementInput.Y, 0.0f);
        AddMovementInput(WorldDirection, 1.0f, false);
    }
}

void ACricketFieldingPawn::UpdateDive(float DeltaSeconds)
{
    if (DiveTimeRemaining > 0.0f)
    {
        DiveTimeRemaining -= DeltaSeconds;

        if (DiveTimeRemaining <= 0.0f)
        {
            SetAction(EFieldingAction::None);
            // Character will naturally slide to stop
        }
    }
}

void ACricketFieldingPawn::UpdateThrow(float DeltaSeconds)
{
    if (CurrentAction != EFieldingAction::Throw)
    {
        return;
    }

    ThrowChargeTime += DeltaSeconds;

    // Release throw after charge time (or immediately for quick throws)
    float RequiredChargeTime = 0.3f + (1.0f - ThrowPower) * 0.5f; // 0.3-0.8s

    if (ThrowChargeTime >= RequiredChargeTime)
    {
        // Execute throw
        FThrowResult Result = CalculateThrowResult();
        OnThrowCompleted.Broadcast(Result);

        // Play throw release animation
        PlayFieldingAnimation(FName("Throw_Release"), 1.0f);

        SetAction(EFieldingAction::None);
        bIsThrowing = false;
        ThrowChargeTime = 0.0f;
    }
}

void ACricketFieldingPawn::UpdateCatch(float DeltaSeconds)
{
    if (!bCatchAttempted)
    {
        return;
    }

    float TimeSinceAttempt = GetWorld()->GetTimeSeconds() - CatchWindowStart;

    if (TimeSinceAttempt > CatchWindowDuration)
    {
        // Catch window expired
        bool bSuccess = false; // Would check actual ball position
        OnCatchAttempt.Broadcast(bSuccess);
        bCatchAttempted = false;
        SetAction(EFieldingAction::None);
    }
}

void ACricketFieldingPawn::CalculateInterceptionPoint()
{
    if (!BallTarget)
    {
        return;
    }

    ACricketBall* Ball = Cast<ACricketBall>(BallTarget);
    if (!Ball)
    {
        return;
    }

    // Simple prediction: where will ball be at ground level
    FVector BallPos = Ball->GetActorLocation();
    FVector BallVel = Ball->GetVelocity();

    if (BallVel.Z < -10.0f) // Ball coming down
    {
        float TimeToGround = BallPos.Z / FMath::Abs(BallVel.Z);
        FVector PredictedPos = BallPos + BallVel * TimeToGround;
        PredictedPos.Z = 0.0f;

        PriorityTargetLocation = PredictedPos;
        PriorityTargetWeight = 1.0f;
    }
    else
    {
        // Ball on ground or rolling
        PriorityTargetLocation = BallPos;
        PriorityTargetWeight = 1.0f;
    }
}

FVector ACricketFieldingPawn::GetPositionCoordinates(EFieldingPosition Position) const
{
    // Standard field positions (relative to striker's end at origin, pitch along X axis)
    // Distances in cm
    switch (Position)
    {
        case EFieldingPosition::Wicketkeeper: return FVector(-50.0f, 0.0f, 0.0f); // Behind stumps
        case EFieldingPosition::Slip1: return FVector(-200.0f, -150.0f, 0.0f);
        case EFieldingPosition::Slip2: return FVector(-300.0f, -200.0f, 0.0f);
        case EFieldingPosition::Slip3: return FVector(-400.0f, -250.0f, 0.0f);
        case EFieldingPosition::Slip4: return FVector(-500.0f, -300.0f, 0.0f);
        case EFieldingPosition::Gully: return FVector(-300.0f, -300.0f, 0.0f);
        case EFieldingPosition::Point: return FVector(0.0f, -400.0f, 0.0f);
        case EFieldingPosition::BackwardPoint: return FVector(-200.0f, -400.0f, 0.0f);
        case EFieldingPosition::Cover: return FVector(200.0f, -400.0f, 0.0f);
        case EFieldingPosition::ExtraCover: return FVector(400.0f, -300.0f, 0.0f);
        case EFieldingPosition::MidOff: return FVector(600.0f, -100.0f, 0.0f);
        case EFieldingPosition::MidOn: return FVector(600.0f, 100.0f, 0.0f);
        case EFieldingPosition::MidWicket: return FVector(400.0f, 300.0f, 0.0f);
        case EFieldingPosition::SquareLeg: return FVector(0.0f, 400.0f, 0.0f);
        case EFieldingPosition::FineLeg: return FVector(-300.0f, 400.0f, 0.0f);
        case EFieldingPosition::ThirdMan: return FVector(-400.0f, -500.0f, 0.0f);
        case EFieldingPosition::LongOn: return FVector(1200.0f, 200.0f, 0.0f);
        case EFieldingPosition::LongOff: return FVector(1200.0f, -200.0f, 0.0f);
        case EFieldingPosition::DeepMidWicket: return FVector(1000.0f, 500.0f, 0.0f);
        case EFieldingPosition::DeepSquareLeg: return FVector(200.0f, 700.0f, 0.0f);
        case EFieldingPosition::DeepFineLeg: return FVector(-400.0f, 700.0f, 0.0f);
        case EFieldingPosition::DeepCover: return FVector(800.0f, -500.0f, 0.0f);
        case EFieldingPosition::DeepExtraCover: return FVector(1000.0f, -400.0f, 0.0f);
        case EFieldingPosition::CowCorner: return FVector(800.0f, 500.0f, 0.0f);
        default: return FVector::ZeroVector;
    }
}

void ACricketFieldingPawn::SetAction(EFieldingAction NewAction)
{
    if (CurrentAction != NewAction)
    {
        CurrentAction = NewAction;
        OnFieldingActionChanged.Broadcast(NewAction);
    }
}

bool ACricketFieldingPawn::CanReachBall(float TimeToBall) const
{
    if (TimeToBall <= 0.0f)
    {
        return false;
    }

    float MaxDistance = 600.0f * Attributes.Speed * TimeToBall;
    float DistToBall = GetDistanceToBall();

    // Reaction time reduces effective time
    float ReactionTime = 0.2f / Attributes.Reactions;
    float EffectiveTime = TimeToBall - ReactionTime;

    if (EffectiveTime <= 0.0f)
    {
        return false;
    }

    return MaxDistance >= DistToBall;
}

FThrowResult ACricketFieldingPawn::CalculateThrowResult() const
{
    FThrowResult Result;
    Result.TargetLocation = ThrowTarget;
    Result.Power = ThrowPower;

    if (ThrowTarget.IsNearlyZero())
    {
        Result.bSuccessful = false;
        return Result;
    }

    // Accuracy based on attributes, power, and charge time
    float BaseAccuracy = Attributes.Throwing * 0.5f + 0.5f; // 0.5-1.0
    float PowerAccuracyPenalty = (1.0f - ThrowPower) * 0.2f; // Less power = more accurate
    float ChargeBonus = FMath::Clamp(ThrowChargeTime / 1.0f, 0.0f, 1.0f) * 0.1f;

    Result.Accuracy = FMath::Clamp(BaseAccuracy - PowerAccuracyPenalty + ChargeBonus, 0.0f, 1.0f);

    // Calculate actual landing position
    FVector ThrowDirection = (ThrowTarget - GetActorLocation()).GetSafeNormal();
    float ThrowDistance = FVector::Dist(GetActorLocation(), ThrowTarget);
    float MaxThrowDistance = 8000.0f * Attributes.Throwing * ThrowPower; // cm

    if (ThrowDistance > MaxThrowDistance)
    {
        // Overthrow or fall short
        Result.ActualLocation = GetActorLocation() + ThrowDirection * MaxThrowDistance;
        Result.bOverthrow = false;
    }
    else
    {
        // Add accuracy deviation
        float DeviationAngle = (1.0f - Result.Accuracy) * 30.0f; // Up to 30 degrees
        float DeviationDistance = ThrowDistance * FMath::Tan(FMath::DegreesToRadians(DeviationAngle));

        FVector Perpendicular = FVector(-ThrowDirection.Y, ThrowDirection.X, 0.0f);
        float RandomDeviation = FMath::RandRange(-DeviationDistance, DeviationDistance);

        Result.ActualLocation = ThrowTarget + Perpendicular * RandomDeviation;
        Result.bOverthrow = false;
    }

    // Direct hit check (for run-outs)
    Result.bDirectHit = Result.Accuracy > 0.9f && FVector::Dist(Result.ActualLocation, ThrowTarget) < 50.0f;

    Result.TimeToTarget = ThrowDistance / (MaxThrowDistance * 0.5f); // Rough estimate
    Result.bSuccessful = true;

    return Result;
}

void ACricketFieldingPawn::PlayFieldingAnimation(FName AnimationName, float PlayRate)
{
    if (!GetMesh() || !GetMesh()->GetAnimInstance())
    {
        return;
    }

    UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AnimationName.ToString());
    if (Montage)
    {
        GetMesh()->GetAnimInstance()->Montage_Play(Montage, PlayRate);
    }
}

// GAS Functions
void ACricketFieldingPawn::InitializeAbilities()
{
    InitializeAttributeSet();
    GrantDefaultAbilities();

    // Grant fielding abilities
    if (AbilitySystemComponent)
    {
        for (TSubclassOf<UCricketFieldingAbility> AbilityClass : AvailableFieldingAbilities)
        {
            if (AbilityClass)
            {
                FGameplayAbilitySpec Spec(AbilityClass, 1, 0, this);
                AbilitySystemComponent->GiveAbility(Spec);
            }
        }

        // Grant wicketkeeping ability
        if (WicketkeepingAbilityClass)
        {
            FGameplayAbilitySpec Spec(WicketkeepingAbilityClass, 1, 0, this);
            AbilitySystemComponent->GiveAbility(Spec);
        }
    }
}

void ACricketFieldingPawn::ActivateFieldingAbility(EFieldingActionType ActionType)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    UCricketFieldingAbility* Ability = FindFieldingAbility(ActionType);
    if (Ability)
    {
        AbilitySystemComponent->TryActivateAbility(Ability->GetAbilitySpecHandle());
    }
}

void ACricketFieldingPawn::ActivateWicketkeepingAbility()
{
    if (!AbilitySystemComponent || !WicketkeepingAbilityClass)
    {
        return;
    }

    // Find the wicketkeeping ability from granted abilities
    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        if (Spec.Ability && Spec.Ability->IsA<UCricketWicketkeepingAbility>())
        {
            ActiveWicketkeepingAbility = Cast<UCricketWicketkeepingAbility>(Spec.Ability);
            AbilitySystemComponent->TryActivateAbility(Spec.Handle);
            break;
        }
    }
}

void ACricketFieldingPawn::EndActiveFieldingAbility()
{
    if (ActiveFieldingAbility && AbilitySystemComponent)
    {
        AbilitySystemComponent->CancelAbilityHandle(ActiveFieldingAbility->GetAbilitySpecHandle());
        ActiveFieldingAbility = nullptr;
    }

    if (ActiveWicketkeepingAbility && AbilitySystemComponent)
    {
        AbilitySystemComponent->CancelAbilityHandle(ActiveWicketkeepingAbility->GetAbilitySpecHandle());
        ActiveWicketkeepingAbility = nullptr;
    }
}

void ACricketFieldingPawn::ApplyStaminaCost(float Cost)
{
    CurrentStamina = FMath::Clamp(CurrentStamina - Cost, 0.0f, MaxStamina);

    // Apply fatigue based on stamina cost
    if (Cost > 0.0f)
    {
        AddFatigue(Cost * 0.5f);
    }

    // Update GAS attribute
    if (AttributeSet)
    {
        AttributeSet->SetCurrentStamina(CurrentStamina);
    }
}

void ACricketFieldingPawn::RegenerateStamina(float DeltaTime)
{
    if (CurrentStamina < MaxStamina)
    {
        float RegenRate = 5.0f * (1.0f - FatigueLevel * 0.5f); // Fatigue slows regen
        CurrentStamina = FMath::Clamp(CurrentStamina + RegenRate * DeltaTime, 0.0f, MaxStamina);

        if (AttributeSet)
        {
            AttributeSet->SetCurrentStamina(CurrentStamina);
        }
    }
}

void ACricketFieldingPawn::SetPressureLevel(float Level)
{
    PressureLevel = FMath::Clamp(Level, 0.0f, 1.0f);
    if (AttributeSet)
    {
        AttributeSet->SetPressureLevel(PressureLevel);
    }
}

void ACricketFieldingPawn::UpdateForm(float PerformanceDelta)
{
    FormLevel = FMath::Clamp(FormLevel + PerformanceDelta, -1.0f, 1.0f);
    if (AttributeSet)
    {
        AttributeSet->SetFormLevel(FormLevel);
    }
}

void ACricketFieldingPawn::AddFatigue(float Amount)
{
    FatigueLevel = FMath::Clamp(FatigueLevel + Amount, 0.0f, 1.0f);
    if (AttributeSet)
    {
        AttributeSet->SetFatigueLevel(FatigueLevel);
    }
}

void ACricketFieldingPawn::RecoverFatigue(float DeltaTime)
{
    if (FatigueLevel > 0.0f)
    {
        float RecoveryRate = 0.02f * (CurrentStamina / MaxStamina); // Stamina helps recovery
        FatigueLevel = FMath::Clamp(FatigueLevel - RecoveryRate * DeltaTime, 0.0f, 1.0f);

        if (AttributeSet)
        {
            AttributeSet->SetFatigueLevel(FatigueLevel);
        }
    }
}

UCricketFieldingAbility* ACricketFieldingPawn::FindFieldingAbility(EFieldingActionType ActionType) const
{
    if (!AbilitySystemComponent)
    {
        return nullptr;
    }

    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        UCricketFieldingAbility* FieldingAbility = Cast<UCricketFieldingAbility>(Spec.Ability);
        if (FieldingAbility && FieldingAbility->GetFieldingActionType() == ActionType)
        {
            return FieldingAbility;
        }
    }
    return nullptr;
}

void ACricketFieldingPawn::OnFieldingAbilityActivated(UCricketFieldingAbility* Ability)
{
    if (!Ability)
    {
        return;
    }

    ActiveFieldingAbility = Ability;

    // Apply pressure/form/fatigue modifiers to ability
    if (AbilitySystemComponent && AttributeSet)
    {
        // Pressure reduces effectiveness
        float PressureModifier = 1.0f - (PressureLevel * 0.3f);
        // Form improves effectiveness
        float FormModifier = 1.0f + (FormLevel * 0.2f);
        // Fatigue reduces effectiveness
        float FatigueModifier = 1.0f - (FatigueLevel * 0.4f);

        float CombinedModifier = PressureModifier * FormModifier * FatigueModifier;
        CombinedModifier = FMath::Clamp(CombinedModifier, 0.3f, 1.5f);

        // Apply modifier via gameplay effect if needed
        Ability->SetEffectivenessModifier(CombinedModifier);
    }
}

void ACricketFieldingPawn::OnFieldingAbilityEnded(UCricketFieldingAbility* Ability)
{
    if (ActiveFieldingAbility == Ability)
    {
        ActiveFieldingAbility = nullptr;
    }
}

void ACricketFieldingPawn::OnFieldingAbilityComplete(UCricketFieldingAbility* Ability, EFieldingOutcome Outcome)
{
    // Handle fielding action completion
    // Could update form, stamina, trigger events based on outcome
    switch (Outcome)
    {
        case EFieldingOutcome::Success:
            UpdateForm(0.02f); // Small form boost
            break;
        case EFieldingOutcome::Partial:
            UpdateForm(0.0f);
            break;
        case EFieldingOutcome::Failed:
            UpdateForm(-0.03f); // Form penalty
            AddFatigue(0.01f);  // Extra fatigue for failed attempt
            break;
    }

    // Reset action state
    SetAction(EFieldingAction::None);
}

void ACricketFieldingPawn::UpdateGASAttributes(float DeltaTime)
{
    RegenerateStamina(DeltaTime);
    RecoverFatigue(DeltaTime);

    // Natural pressure decay
    if (PressureLevel > 0.0f)
    {
        PressureLevel = FMath::Max(0.0f, PressureLevel - DeltaTime * 0.1f);
        if (AttributeSet)
        {
            AttributeSet->SetPressureLevel(PressureLevel);
        }
    }

    // Natural form drift toward zero
    if (FMath::Abs(FormLevel) > 0.01f)
    {
        FormLevel = FMath::FInterpTo(FormLevel, 0.0f, DeltaTime, 0.05f);
        if (AttributeSet)
        {
            AttributeSet->SetFormLevel(FormLevel);
        }
    }
}

// GAS Internal
void ACricketFieldingPawn::InitializeAttributeSet()
{
    if (AttributeSet)
    {
        // Initialize fielding-specific attributes
        AttributeSet->SetCurrentStamina(MaxStamina);
        AttributeSet->SetMaxStamina(MaxStamina);
        AttributeSet->SetPressureLevel(0.0f);
        AttributeSet->SetFormLevel(0.0f);
        AttributeSet->SetFatigueLevel(0.0f);

        // Set fielding attributes from configuration
        AttributeSet->SetAgility(Attributes.Agility);
        AttributeSet->SetReflexes(Attributes.Reactions);
        AttributeSet->SetThrowing(Attributes.Throwing);
        AttributeSet->SetCatching(Attributes.Catching);
        AttributeSet->SetAnticipation(Attributes.Anticipation);
    }

    CurrentStamina = MaxStamina;
    PressureLevel = 0.0f;
    FormLevel = 0.0f;
    FatigueLevel = 0.0f;
}

void ACricketFieldingPawn::GrantDefaultAbilities()
{
    if (AbilitySystemComponent)
    {
        for (TSubclassOf<UCricketGameplayAbility> AbilityClass : DefaultAbilities)
        {
            if (AbilityClass)
            {
                FGameplayAbilitySpec Spec(AbilityClass, 1, 0, this);
                AbilitySystemComponent->GiveAbility(Spec);
            }
        }
    }
}