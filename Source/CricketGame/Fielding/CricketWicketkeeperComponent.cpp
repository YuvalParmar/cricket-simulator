#include "CricketWicketkeeperComponent.h"
#include "CricketFieldingPawn.h"
#include "CricketBall.h"
#include "CricketGameMode.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIBlueprintHelperLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UCricketWicketkeeperComponent::UCricketWicketkeeperComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // Override default fielding position
    FieldingPosition = EFieldingPosition::Wicketkeeper;

    // Wicketkeeper specific defaults
    WKAttributes.StandingUp = 1.0f;
    WKAttributes.StumpingSpeed = 1.0f;
    WKAttributes.Collection = 1.0f;
    WKAttributes.Concentration = 1.0f;
}

void UCricketWicketkeeperComponent::BeginPlay()
{
    Super::BeginPlay();

    // Set initial position behind stumps
    HomePosition = GetPositionCoordinates(EFieldingPosition::Wicketkeeper);

    if (FieldingPawn)
    {
        FieldingPawn->SetActorLocation(HomePosition);
    }
}

void UCricketWicketkeeperComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateWicketkeeperAI(DeltaTime);
    UpdateStance(DeltaTime);
}

void UCricketWicketkeeperComponent::SetWicketkeeperPawn(ACricketFieldingPawn* Pawn)
{
    SetFieldingPawn(Pawn);
}

void UCricketWicketkeeperComponent::SetStance(EWicketkeeperStance Stance)
{
    CurrentStance = Stance;

    switch (Stance)
    {
        case EWicketkeeperStance::Standing:
            StanceDistance = 50.0f; // Very close to stumps
            break;
        case EWicketkeeperStance::Crouching:
            StanceDistance = 200.0f; // Normal distance
            break;
        case EWicketkeeperStance::UpToStumps:
            StanceDistance = 100.0f; // Close for spinners
            break;
    }

    // Update home position
    HomePosition = GetPositionCoordinates(EFieldingPosition::Wicketkeeper);
    HomePosition.X = -StanceDistance; // Behind stumps

    if (FieldingPawn && bAIControlled)
    {
        FieldingPawn->MoveToPosition(HomePosition);
    }
}

void UCricketWicketkeeperComponent::SetWicketkeeperAttributes(const FWicketkeeperAttributes& NewAttributes)
{
    WKAttributes = NewAttributes;
    Attributes = NewAttributes; // Base attributes

    if (FieldingPawn && FieldingPawn->GetCharacterMovement())
    {
        FieldingPawn->GetCharacterMovement()->MaxWalkSpeed = 600.0f * Attributes.Speed;
    }
}

void UCricketWicketkeeperComponent::CollectBall(const FVector& BallLocation, const FVector& BallVelocity)
{
    if (!FieldingPawn || !TargetBall)
    {
        return;
    }

    bBallCollected = true;
    LastCollectionPoint = BallLocation;

    // Check if collection is clean
    float DistToBall = FVector::Dist(FieldingPawn->GetActorLocation(), BallLocation);
    float MaxReach = 150.0f + Attributes.Catching * 50.0f + WKAttributes.Collection * 50.0f;

    bool bClean = DistToBall <= MaxReach;

    // Ball speed affects collection
    float BallSpeed = BallVelocity.Size();
    if (BallSpeed > 2000.0f)
    {
        bClean = bClean && (FMath::FRand() < 0.8f); // 80% chance even if in range
    }

    OnCollection.Broadcast(bClean);

    if (!bClean)
    {
        // Fumbled - ball may deflect
        ProcessMissedBall(BallLocation, BallVelocity);
    }
}

void UCricketWicketkeeperComponent::AttemptStumpingAt(const FVector& StumpLocation)
{
    if (CurrentAction == EFieldingAction::Stumping)
    {
        return;
    }

    // Quick stumping action
    FStumpingResult Result = ExecuteStumping(StumpLocation);
    OnStumpingAttempt.Broadcast(Result);

    if (Result.bSuccessful)
    {
        Stumpings++;
    }
}

void UCricketWicketkeeperComponent::AttemptRunOutStriker(const FVector& StumpLocation)
{
    AttemptRunOut(StumpLocation);
    if (/* result successful */ false) // Would check actual result
    {
        RunOuts++;
    }
}

void UCricketWicketkeeperComponent::AttemptRunOutNonStriker(const FVector& StumpLocation)
{
    // Non-striker run out - throw to bowler's end
    AttemptRunOut(StumpLocation);
    if (/* result successful */ false)
    {
        RunOuts++;
    }
}

void UCricketWicketkeeperComponent::MoveDownLegSide(float Distance)
{
    if (!FieldingPawn)
    {
        return;
    }

    FVector TargetPos = HomePosition;
    TargetPos.Y += Distance; // Leg side is positive Y for right-handed batter

    MoveToLocation(TargetPos);
}

void UCricketWicketkeeperComponent::MoveDownOffSide(float Distance)
{
    if (!FieldingPawn)
    {
        return;
    }

    FVector TargetPos = HomePosition;
    TargetPos.Y -= Distance; // Off side is negative Y

    MoveToLocation(TargetPos);
}

float UCricketWicketkeeperComponent::CalculateStumpingChance() const
{
    if (!TargetBall || !FieldingPawn)
    {
        return 0.0f;
    }

    // Base chance from attributes
    float BaseChance = 0.3f + WKAttributes.StumpingSpeed * 0.2f; // 0.3 to 0.7

    // Ball speed factor
    float BallSpeed = TargetBall->GetVelocity().Size();
    float SpeedPenalty = FMath::Clamp(BallSpeed / 3000.0f, 0.0f, 0.4f);

    // Distance to stumps
    FVector StumpLoc = GetStumpLocation(true);
    float DistToStumps = FVector::Dist(FieldingPawn->GetActorLocation(), StumpLoc);
    float DistanceFactor = FMath::Clamp(1.0f - DistToStumps / 200.0f, 0.5f, 1.0f);

    // Concentration
    float ConcentrationFactor = 0.5f + WKAttributes.Concentration * 0.5f;

    return FMath::Clamp(BaseChance * (1.0f - SpeedPenalty) * DistanceFactor * ConcentrationFactor, 0.0f, 1.0f);
}

FByesResult UCricketWicketkeeperComponent::PredictByes(const FVector& BallLocation, const FVector& BallVelocity) const
{
    FByesResult Result;

    if (!FieldingPawn)
    {
        return Result;
    }

    // Distance from wicketkeeper to ball
    float DistToBall = FVector::Dist(FieldingPawn->GetActorLocation(), BallLocation);

    // Time for wicketkeeper to reach ball
    float WKTime = DistToBall / (600.0f * Attributes.Speed * WKAttributes.Collection);

    // Time for ball to pass stumps and travel to boundary
    float BallSpeed = BallVelocity.Size();
    float TimeToBoundary = (6000.0f - BallLocation.X) / BallSpeed; // Rough: 60m to boundary

    // Byes = how far ball travels past stumps before collection
    if (WKTime < TimeToBoundary)
    {
        // Wicketkeeper gets there first
        float BallTravelDuringCollection = BallSpeed * WKTime;
        int32 Byes = FMath::FloorToInt(BallTravelDuringCollection / 100.0f); // Rough conversion

        Result.Byes = FMath::Clamp(Byes, 0, 4);
        Result.bWicketkeeperError = false;
    }
    else
    {
        // Ball beats wicketkeeper - more byes
        Result.Byes = FMath::RandRange(1, 4);
        Result.bWicketkeeperError = true;
        Result.BallDeflection = BallVelocity.GetSafeNormal2D() * 100.0f;
    }

    return Result;
}

void UCricketWicketkeeperComponent::UpdateWicketkeeperAI(float DeltaTime)
{
    if (!bAIControlled || !TargetBall || !FieldingPawn)
    {
        return;
    }

    ACricketBall* Ball = Cast<ACricketBall>(TargetBall);
    if (!Ball)
    {
        return;
    }

    FVector BallPos = Ball->GetActorLocation();
    FVector BallVel = Ball->GetVelocity();
    EBallState BallState = Ball->GetBallState();

    // Auto-adjust stance based on bowler type
    // Would check bowler's delivery type from GameMode
    // For now, default to crouching for pace, up to stumps for spin

    // Ball in flight - track and prepare to collect
    if (BallState == EBallState::InFlight)
    {
        // Predict where ball will pass stumps
        float TimeToStumps = 0.0f;
        if (BallVel.X > 10.0f)
        {
            TimeToStumps = (0.0f - BallPos.X) / BallVel.X; // Stumps at X=0
        }

        if (TimeToStumps > 0.0f && TimeToStumps < 1.0f)
        {
            // Ball arriving at stumps soon
            FVector PredictedStumpPos = BallPos + BallVel * TimeToStumps;

            // Move to collect
            FVector CollectPos = PredictedStumpPos;
            CollectPos.X -= StanceDistance; // Behind stumps

            MoveToLocation(CollectPos);

            // Attempt collection when ball arrives
            if (TimeToStumps < 0.1f)
            {
                CollectBall(PredictedStumpPos, BallVel);
            }
        }
    }
    // Ball bounced - react to deflection
    else if (BallState == EBallState::Bouncing || BallState == EBallState::OnPitch)
    {
        // Quick reaction to edges
        float ReactionTime = CalculateReactionTime();
        float TimeToStumps = (BallPos.X > 0.0f) ? BallPos.X / BallVel.X : 0.0f;

        if (TimeToStumps < ReactionTime + 0.1f)
        {
            // Very late reaction needed - dive if necessary
            float LateralDist = FMath::Abs(BallPos.Y - FieldingPawn->GetActorLocation().Y);
            if (LateralDist > 100.0f && Attributes.Agility > 1.2f)
            {
                FVector2D DiveDir = FVector2D(BallPos.Y > FieldingPawn->GetActorLocation().Y ? 1.0f : -1.0f, 0.0f);
                Dive(DiveDir);
            }
        }

        MoveToLocation(FVector(-StanceDistance, BallPos.Y, 0.0f));
    }
    // Ball hit bat - track for catch
    else if (BallState == EBallState::HitBat)
    {
        // Ball popped up - move under it
        FVector PredictedLandPos = BallPos; // Would predict trajectory
        MoveToLocation(FVector(-StanceDistance, PredictedLandPos.Y, 0.0f));
        AttemptCatch();
    }
    // Ball missed bat - check for byes/stumping
    else if (BallState == EBallState::Dead && bBallCollected)
    {
        // Check if batter out of crease for stumping
        // Would need batter reference from GameMode

        bBallCollected = false; // Reset for next ball
    }
}

void UCricketWicketkeeperComponent::UpdateStance(float DeltaTime)
{
    if (!FieldingPawn)
    {
        return;
    }

    // Smooth stance transitions
    FVector TargetPos = HomePosition;
    TargetPos.X = -StanceDistance;

    FVector CurrentPos = FieldingPawn->GetActorLocation();
    float Dist = FVector::Dist2D(CurrentPos, TargetPos);

    if (Dist > 50.0f && bAIControlled)
    {
        MoveToLocation(TargetPos);
    }
}

FStumpingResult UCricketWicketkeeperComponent::ExecuteStumping(const FVector& StumpLocation)
{
    FStumpingResult Result;

    if (!FieldingPawn || !TargetBall)
    {
        Result.bSuccessful = false;
        Result.FailureReason = FGameplayTag::RequestGameplayTag(FName("StumpingFail.NoBall"));
        return Result;
    }

    // Check if ball collected cleanly
    Result.bBallCollectedCleanly = bBallCollected;

    if (!bBallCollected)
    {
        Result.bSuccessful = false;
        Result.FailureReason = FGameplayTag::RequestGameplayTag(FName("StumpingFail.NotCollected"));
        return Result;
    }

    // Speed of stumping action
    float StumpingTime = 0.3f / FMath::Clamp(WKAttributes.StumpingSpeed, 0.5f, 2.0f); // 0.15-0.6s
    Result.SpeedScore = FMath::Clamp(1.0f - StumpingTime / 0.5f, 0.0f, 1.0f);

    // Accuracy of throw to stumps
    FVector WKPos = FieldingPawn->GetActorLocation();
    float ThrowDist = FVector::Dist(WKPos, StumpLocation);

    // Wicketkeeper throws are typically very accurate at short range
    float ThrowAccuracy = 0.9f + WKAttributes.Throwing * 0.05f; // 0.9 to 1.0
    ThrowAccuracy *= (1.0f - StumpingTime / 0.5f) * 0.5f + 0.5f; // Faster = slightly less accurate

    Result.bSuccessful = FMath::FRand() < ThrowAccuracy;

    if (!Result.bSuccessful)
    {
        Result.FailureReason = FGameplayTag::RequestGameplayTag(FName("StumpingFail.MissedStumps"));
    }

    return Result;
}

FByesResult UCricketWicketkeeperComponent::CalculateByes(const FVector& BallLocation, const FVector& BallVelocity) const
{
    return PredictByes(BallLocation, BallVelocity);
}

bool UCricketWicketkeeperComponent::IsBallCollectable(const FVector& BallLocation, const FVector& BallVelocity) const
{
    if (!FieldingPawn)
    {
        return false;
    }

    float Dist = FVector::Dist(FieldingPawn->GetActorLocation(), BallLocation);
    float MaxReach = 150.0f + Attributes.Catching * 50.0f + WKAttributes.Collection * 50.0f;

    return Dist <= MaxReach;
}

FVector UCricketWicketkeeperComponent::GetStumpLocation(bool bStrikerEnd) const
{
    // Striker's end stumps at origin (0,0,0)
    // Non-striker's end at X = 2012cm (22 yards = 20.12m)
    if (bStrikerEnd)
    {
        return FVector(0.0f, 0.0f, 0.0f);
    }
    else
    {
        return FVector(2012.0f, 0.0f, 0.0f);
    }
}

void UCricketWicketkeeperComponent::ProcessMissedBall(const FVector& BallLocation, const FVector& BallVelocity)
{
    // Ball deflected off wicketkeeper - calculate byes
    FByesResult Byes = CalculateByes(BallLocation, BallVelocity);
    Byes.bWicketkeeperError = true;
    ByesConceded += Byes.Byes;

    OnByesConceded.Broadcast(Byes);
}

float UCricketWicketkeeperComponent::CalculateReactionTime() const
{
    // Wicketkeepers have faster reactions
    float BaseReaction = 0.15f; // Faster than regular fielders (0.2s)
    return BaseReaction / FMath::Clamp(Attributes.Reactions * WKAttributes.Concentration, 0.5f, 2.0f);
}