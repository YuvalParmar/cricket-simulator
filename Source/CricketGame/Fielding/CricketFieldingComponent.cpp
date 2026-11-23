#include "CricketFieldingComponent.h"
#include "CricketFieldingPawn.h"
#include "CricketBall.h"
#include "CricketGameMode.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIBlueprintHelperLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UCricketFieldingComponent::UCricketFieldingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    MaxStamina = 100.0f;
    CurrentStamina = MaxStamina;
}

void UCricketFieldingComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UCricketFieldingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateStamina(DeltaTime);

    if (bAIControlled)
    {
        UpdateAI(DeltaTime);
    }
    else
    {
        UpdateMovement(DeltaTime);
    }

    UpdateDive(DeltaTime);
    UpdateThrow(DeltaTime);
    UpdateCatch(DeltaTime);
}

void UCricketFieldingComponent::SetFieldingPawn(ACricketFieldingPawn* Pawn)
{
    FieldingPawn = Pawn;

    if (Pawn)
    {
        HomePosition = GetPositionCoordinates(FieldingPosition);
        Pawn->SetActorLocation(HomePosition);

        if (UCharacterMovementComponent* MoveComp = Pawn->GetCharacterMovement())
        {
            MoveComp->MaxWalkSpeed = 600.0f * Attributes.Speed;
        }
    }
}

void UCricketFieldingComponent::SetTargetBall(ACricketBall* Ball)
{
    TargetBall = Ball;
}

void UCricketFieldingComponent::SetGameMode(ACricketGameMode* InGameMode)
{
    GameMode = InGameMode;
}

void UCricketFieldingComponent::SetFieldingPosition(EFieldingPosition Position)
{
    FieldingPosition = Position;
    HomePosition = GetPositionCoordinates(Position);

    if (FieldingPawn && bAIControlled)
    {
        FieldingPawn->MoveToPosition(HomePosition);
    }
}

void UCricketFieldingComponent::SetAttributes(const FFieldingAttributes& NewAttributes)
{
    Attributes = NewAttributes;

    if (FieldingPawn && FieldingPawn->GetCharacterMovement())
    {
        FieldingPawn->GetCharacterMovement()->MaxWalkSpeed = 600.0f * Attributes.Speed;
    }
}

void UCricketFieldingComponent::EnableAIControl(bool bEnable)
{
    bAIControlled = bEnable;

    if (FieldingPawn)
    {
        if (bEnable)
        {
            FieldingPawn->GetCharacterMovement()->bOrientRotationToMovement = true;
            MoveToLocation(HomePosition);
        }
        else
        {
            FieldingPawn->GetCharacterMovement()->bOrientRotationToMovement = false;
        }
    }
}

void UCricketFieldingComponent::SetPriorityTarget(const FVector& Location, float Priority)
{
    PriorityTargetLocation = Location;
    PriorityTargetWeight = Priority;
}

void UCricketFieldingComponent::MoveToLocation(const FVector& Location, bool bSprint)
{
    if (!FieldingPawn)
    {
        return;
    }

    bWantsToSprint = bSprint;

    if (bAIControlled)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(FieldingPawn->GetController(), Location);
    }
    else
    {
        // Player controlled - movement input handled by pawn
    }
}

void UCricketFieldingComponent::Dive(const FVector2D& Direction)
{
    if (CurrentAction == EFieldingAction::Dive || CurrentAction == EFieldingAction::Slide)
    {
        return;
    }

    DiveDirection = FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
    DiveTimeRemaining = 1.0f;
    SetAction(EFieldingAction::Dive);

    if (FieldingPawn)
    {
        FVector LaunchVelocity = DiveDirection * 1500.0f * Attributes.Agility;
        LaunchVelocity.Z = 200.0f;
        FieldingPawn->LaunchCharacter(LaunchVelocity, true, true);
    }
}

void UCricketFieldingComponent::ThrowToLocation(const FVector& TargetLocation, float Power)
{
    if (CurrentAction == EFieldingAction::Throw)
    {
        return;
    }

    ThrowTarget = TargetLocation;
    ThrowPower = FMath::Clamp(Power, 0.0f, 1.0f);
    ThrowChargeTime = 0.0f;
    SetAction(EFieldingAction::Throw);
    bIsThrowing = true;
}

void UCricketFieldingComponent::AttemptCatch()
{
    if (bCatchAttempted)
    {
        return;
    }

    bCatchAttempted = true;
    CatchWindowStart = GetWorld()->GetTimeSeconds();
    SetAction(EFieldingAction::Catch);
}

void UCricketFieldingComponent::AttemptRunOut(const FVector& StumpLocation)
{
    ThrowToLocation(StumpLocation, 1.0f);
}

void UCricketFieldingComponent::AttemptStumping()
{
    SetAction(EFieldingAction::Stumping);
}

FInterceptionPoint UCricketFieldingComponent::CalculateInterceptionPoint() const
{
    return CalculateBallInterception();
}

bool UCricketFieldingComponent::CanReachBall(float TimeToBall) const
{
    if (TimeToBall <= 0.0f || !FieldingPawn || !TargetBall)
    {
        return false;
    }

    float ReactionTime = CalculateReactionTime();
    float EffectiveTime = TimeToBall - ReactionTime;

    if (EffectiveTime <= 0.0f)
    {
        return false;
    }

    float MaxDistance = CalculateMaxRunDistance(EffectiveTime);
    float DistToBall = GetDistanceToBall();

    return MaxDistance >= DistToBall;
}

float UCricketFieldingComponent::GetDistanceToBall() const
{
    if (FieldingPawn && TargetBall)
    {
        return FVector::Dist(FieldingPawn->GetActorLocation(), TargetBall->GetActorLocation());
    }
    return FLT_MAX;
}

void UCricketFieldingComponent::UpdateAI(float DeltaTime)
{
    if (!TargetBall || !FieldingPawn)
    {
        // No ball - return to home position
        if (FieldingPawn)
        {
            float DistToHome = FVector::Dist2D(FieldingPawn->GetActorLocation(), HomePosition);
            if (DistToHome > 100.0f)
            {
                MoveToLocation(HomePosition, true);
            }
        }
        return;
    }

    ACricketBall* Ball = Cast<ACricketBall>(TargetBall);
    if (!Ball)
    {
        return;
    }

    // Calculate interception point
    FInterceptionPoint Interception = CalculateBallInterception();
    OnInterceptionCalculated.Broadcast(Interception);

    if (!Interception.bCanIntercept)
    {
        // Cannot reach - return to home or backup position
        if (PriorityTargetWeight > 0.0f)
        {
            MoveToLocation(PriorityTargetLocation);
        }
        else
        {
            MoveToLocation(HomePosition);
        }
        return;
    }

    // Move to interception point
    MoveToLocation(Interception.Location, Interception.bRequiresDive);

    // Attempt catch if ball is in air and catchable
    if (Ball->GetBallState() == EBallState::InFlight && Interception.bCanIntercept)
    {
        if (Interception.BallTimeToArrive > 0.1f && Interception.BallTimeToArrive < 2.0f)
        {
            // Check if at catch height
            FVector BallPos = Ball->GetActorLocation();
            FVector BallVel = Ball->GetVelocity();

            if (BallPos.Z < 300.0f && BallVel.Z < 0.0f) // Coming down, catchable height
            {
                AttemptCatch();
            }
        }
    }
    // Ground fielding
    else if (Ball->GetBallState() == EBallState::OnPitch || Ball->GetBallState() == EBallState::Bouncing)
    {
        if (Interception.bRequiresDive)
        {
            FVector2D DiveDir = FVector2D(Interception.Location - FieldingPawn->GetActorLocation()).GetSafeNormal();
            Dive(DiveDir);
        }
    }
    // Ball stopped - pick up and throw
    else if (Ball->GetBallState() == EBallState::Dead && Interception.bCanIntercept)
    {
        MoveToLocation(Ball->GetActorLocation());

        if (GetDistanceToBall() < 100.0f)
        {
            // Find throw target (wicketkeeper or bowler's end)
            FVector ThrowTargetLoc = FVector::ZeroVector;

            if (GameMode)
            {
                // Would get actual target from game mode
                // ThrowTargetLoc = GameMode->GetCurrentThrowTarget();
            }

            if (!ThrowTargetLoc.IsNearlyZero())
            {
                ThrowToLocation(ThrowTargetLoc, 0.8f);
            }
        }
    }
}

void UCricketFieldingComponent::UpdateMovement(float DeltaTime)
{
    if (!FieldingPawn)
    {
        return;
    }

    UCharacterMovementComponent* MoveComp = FieldingPawn->GetCharacterMovement();
    if (!MoveComp)
    {
        return;
    }

    MoveComp->MaxWalkSpeed = bWantsToSprint ? 1000.0f * Attributes.Speed : 600.0f * Attributes.Speed;

    if (!MovementInput.IsNearlyZero())
    {
        FVector WorldDirection = FVector(MovementInput.X, MovementInput.Y, 0.0f);
        FieldingPawn->AddMovementInput(WorldDirection, 1.0f, false);
    }
}

void UCricketFieldingComponent::UpdateDive(float DeltaTime)
{
    if (DiveTimeRemaining > 0.0f)
    {
        DiveTimeRemaining -= DeltaTime;

        if (DiveTimeRemaining <= 0.0f)
        {
            SetAction(EFieldingAction::None);
            bIsDiving = false;
        }
    }
}

void UCricketFieldingComponent::UpdateThrow(float DeltaTime)
{
    if (!bIsThrowing || CurrentAction != EFieldingAction::Throw)
    {
        return;
    }

    ThrowChargeTime += DeltaTime;

    float RequiredChargeTime = 0.3f + (1.0f - ThrowPower) * 0.5f;

    if (ThrowChargeTime >= RequiredChargeTime)
    {
        FThrowResult Result = CalculateThrowResult();
        OnThrowExecuted.Broadcast(Result);

        SetAction(EFieldingAction::None);
        bIsThrowing = false;
        ThrowChargeTime = 0.0f;
    }
}

void UCricketFieldingComponent::UpdateCatch(float DeltaTime)
{
    if (!bCatchAttempted)
    {
        return;
    }

    float TimeSinceAttempt = GetWorld()->GetTimeSeconds() - CatchWindowStart;

    if (TimeSinceAttempt > CatchWindowDuration)
    {
        FCatchResult Result = CalculateCatchResult();
        OnCatchAttempted.Broadcast(Result);

        bCatchAttempted = false;
        SetAction(EFieldingAction::None);
    }
}

void UCricketFieldingComponent::UpdateStamina(float DeltaTime)
{
    // Stamina drain during sprint
    if (bWantsToSprint && CurrentAction == EFieldingAction::Move)
    {
        CurrentStamina = FMath::Max(0.0f, CurrentStamina - DeltaTime * 10.0f / Attributes.Stamina);
    }

    // Recovery when not sprinting
    if (!bWantsToSprint && CurrentAction != EFieldingAction::Dive)
    {
        CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + DeltaTime * 5.0f * Attributes.Stamina);
    }

    // Dive costs stamina
    if (bIsDiving)
    {
        CurrentStamina = FMath::Max(0.0f, CurrentStamina - 5.0f);
    }
}

FInterceptionPoint UCricketFieldingComponent::CalculateBallInterception() const
{
    FInterceptionPoint Result;

    if (!FieldingPawn || !TargetBall)
    {
        return Result;
    }

    FVector BallPos = TargetBall->GetActorLocation();
    FVector BallVel = TargetBall->GetVelocity();
    FVector FielderPos = FieldingPawn->GetActorLocation();

    float BallSpeed = BallVel.Size();

    if (BallSpeed < 10.0f)
    {
        // Ball nearly stopped
        Result.Location = BallPos;
        Result.TimeToReach = FVector::Dist2D(FielderPos, BallPos) / (600.0f * Attributes.Speed);
        Result.BallTimeToArrive = 0.0f;
        Result.bCanIntercept = true;
        Result.Confidence = 1.0f;
        return Result;
    }

    // Predict ball trajectory
    if (BallVel.Z < -50.0f) // Ball coming down - catch opportunity
    {
        float TimeToGround = BallPos.Z / FMath::Abs(BallVel.Z);
        FVector PredictedGroundPos = BallPos + BallVel * TimeToGround;
        PredictedGroundPos.Z = 0.0f;

        float FielderDist = FVector::Dist2D(FielderPos, PredictedGroundPos);
        float FielderSpeed = 600.0f * Attributes.Speed;
        float TimeToReach = FielderDist / FielderSpeed;

        Result.Location = PredictedGroundPos;
        Result.TimeToReach = TimeToReach;
        Result.BallTimeToArrive = TimeToGround;
        Result.bCanIntercept = TimeToReach <= TimeToGround + 0.5f; // Can arrive before or shortly after
        Result.bRequiresDive = TimeToReach > TimeToGround * 0.8f && Attributes.Agility > 1.0f;
        Result.Confidence = Result.bCanIntercept ? FMath::Clamp(1.0f - (TimeToReach / (TimeToGround + 0.5f)), 0.0f, 1.0f) : 0.0f;
    }
    else
    {
        // Ball on ground or rolling - intercept
        float TimeToReach = FVector::Dist2D(FielderPos, BallPos) / (600.0f * Attributes.Speed);
        float BallTimeToStop = BallSpeed / 500.0f; // Rough deceleration estimate

        Result.Location = BallPos + BallVel.GetSafeNormal2D() * 100.0f; // Slightly ahead
        Result.TimeToReach = TimeToReach;
        Result.BallTimeToArrive = BallTimeToStop;
        Result.bCanIntercept = TimeToReach <= BallTimeToStop + 1.0f;
        Result.bRequiresDive = TimeToReach > BallTimeToStop * 0.7f && Attributes.Agility > 1.2f;
        Result.Confidence = Result.bCanIntercept ? FMath::Clamp(1.0f - (TimeToReach / (BallTimeToStop + 1.0f)), 0.0f, 1.0f) : 0.0f;
    }

    return Result;
}

FVector UCricketFieldingComponent::GetPositionCoordinates(EFieldingPosition Position) const
{
    switch (Position)
    {
        case EFieldingPosition::Wicketkeeper: return FVector(-50.0f, 0.0f, 0.0f);
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

FThrowResult UCricketFieldingComponent::CalculateThrowResult() const
{
    FThrowResult Result;
    Result.TargetLocation = ThrowTarget;
    Result.Power = ThrowPower;

    if (ThrowTarget.IsNearlyZero() || !FieldingPawn)
    {
        Result.bSuccessful = false;
        return Result;
    }

    // Accuracy based on attributes, power, and charge time
    float BaseAccuracy = Attributes.Throwing * 0.5f + 0.5f;
    float PowerAccuracyPenalty = (1.0f - ThrowPower) * 0.2f;
    float ChargeBonus = FMath::Clamp(ThrowChargeTime / 1.0f, 0.0f, 1.0f) * 0.1f;
    float StaminaFactor = CurrentStamina / MaxStamina;

    Result.Accuracy = FMath::Clamp(BaseAccuracy - PowerAccuracyPenalty + ChargeBonus, 0.0f, 1.0f) * StaminaFactor;

    // Calculate actual landing position
    FVector FielderPos = FieldingPawn->GetActorLocation();
    FVector ThrowDirection = (ThrowTarget - FielderPos).GetSafeNormal();
    float ThrowDistance = FVector::Dist(FielderPos, ThrowTarget);
    float MaxThrowDistance = 8000.0f * Attributes.Throwing * ThrowPower;

    if (ThrowDistance > MaxThrowDistance)
    {
        Result.ActualLocation = FielderPos + ThrowDirection * MaxThrowDistance;
        Result.bOverthrow = false;
    }
    else
    {
        float DeviationAngle = (1.0f - Result.Accuracy) * 30.0f;
        float DeviationDistance = ThrowDistance * FMath::Tan(FMath::DegreesToRadians(DeviationAngle));

        FVector Perpendicular = FVector(-ThrowDirection.Y, ThrowDirection.X, 0.0f);
        float RandomDeviation = FMath::RandRange(-DeviationDistance, DeviationDistance);

        Result.ActualLocation = ThrowTarget + Perpendicular * RandomDeviation;
        Result.bOverthrow = false;
    }

    Result.bDirectHit = Result.Accuracy > 0.9f && FVector::Dist(Result.ActualLocation, ThrowTarget) < 50.0f;
    Result.TimeToTarget = ThrowDistance / (MaxThrowDistance * 0.5f);
    Result.bSuccessful = true;

    return Result;
}

FCatchResult UCricketFieldingComponent::CalculateCatchResult() const
{
    FCatchResult Result;

    if (!FieldingPawn || !TargetBall)
    {
        Result.bSuccessful = false;
        return Result;
    }

    // Check if ball is actually at catch position
    FVector BallPos = TargetBall->GetActorLocation();
    FVector FielderPos = FieldingPawn->GetActorLocation();
    float DistToBall = FVector::Dist(BallPos, FielderPos);

    // Catch radius based on attributes
    float CatchRadius = 100.0f + Attributes.Catching * 50.0f; // 100-200cm

    if (DistToBall > CatchRadius)
    {
        Result.bSuccessful = false;
        Result.bDropped = true;
        Result.DropReason = FGameplayTag::RequestGameplayTag(FName("DropReason.OutOfReach"));
        return Result;
    }

    // Base catch probability
    float BaseCatchChance = 0.5f + Attributes.Catching * 0.25f; // 0.5 to 1.0

    // Ball speed factor
    float BallSpeed = TargetBall->GetVelocity().Size();
    float SpeedPenalty = FMath::Clamp(BallSpeed / 3000.0f, 0.0f, 0.3f); // Up to 30% penalty for fast balls

    // Height factor (too high or too low)
    float HeightFactor = 1.0f;
    if (BallPos.Z > 250.0f)
    {
        HeightFactor = 0.7f; // High catch
    }
    else if (BallPos.Z < 30.0f)
    {
        HeightFactor = 0.6f; // Low catch
    }

    // Reaction time factor
    float ReactionFactor = Attributes.Reactions * 0.5f + 0.5f;

    Result.CatchQuality = BaseCatchChance * (1.0f - SpeedPenalty) * HeightFactor * ReactionFactor;
    Result.CatchQuality = FMath::Clamp(Result.CatchQuality, 0.0f, 1.0f);

    Result.bSuccessful = FMath::FRand() < Result.CatchQuality;

    if (!Result.bSuccessful)
    {
        Result.bDropped = true;
        if (BallSpeed > 2000.0f)
        {
            Result.DropReason = FGameplayTag::RequestGameplayTag(FName("DropReason.HardHit"));
        }
        else if (BallPos.Z > 250.0f)
        {
            Result.DropReason = FGameplayTag::RequestGameplayTag(FName("DropReason.HighCatch"));
        }
        else
        {
            Result.DropReason = FGameplayTag::RequestGameplayTag(FName("DropReason.Fumbled"));
        }
    }

    return Result;
}

void UCricketFieldingComponent::SetAction(EFieldingAction NewAction)
{
    if (CurrentAction != NewAction)
    {
        CurrentAction = NewAction;

        if (FieldingPawn)
        {
            FieldingPawn->OnFieldingActionChanged.Broadcast(NewAction);
        }
    }
}

float UCricketFieldingComponent::CalculateReactionTime() const
{
    // Base reaction time 0.2s, modified by reactions attribute
    return 0.2f / FMath::Clamp(Attributes.Reactions, 0.5f, 2.0f);
}

float UCricketFieldingComponent::CalculateMaxRunDistance(float TimeAvailable) const
{
    float BaseSpeed = 600.0f * Attributes.Speed; // cm/s
    float StaminaFactor = CurrentStamina / MaxStamina;
    return BaseSpeed * StaminaFactor * TimeAvailable;
}