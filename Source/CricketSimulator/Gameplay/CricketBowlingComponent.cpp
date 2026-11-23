#include "CricketBowlingComponent.h"
#include "CricketBowlingPawn.h"
#include "CricketBall.h"
#include "PitchActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UCricketBowlingComponent::UCricketBowlingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    MaxStamina = 100.0f;
    CurrentStamina = MaxStamina;
}

void UCricketBowlingComponent::BeginPlay()
{
    Super::BeginPlay();

    // Load delivery definitions from DataTable
    if (DeliveryDefinitionsTable)
    {
        TArray<FDeliveryDefinition*> Rows;
        DeliveryDefinitionsTable->GetAllRows(TEXT(""), Rows);
        for (FDeliveryDefinition* Row : Rows)
        {
            if (Row)
            {
                DeliveryDefinitions.Add(*Row);
            }
        }
    }
}

void UCricketBowlingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateStamina(DeltaTime);

    if (CurrentPhase == EBowlingPhase::RunUp && CurrentBall)
    {
        UpdateBallPhysics(DeltaTime);
    }
}

void UCricketBowlingComponent::SetBowlingPawn(ACricketBowlingPawn* Pawn)
{
    BowlingPawn = Pawn;
}

void UCricketBowlingComponent::SetPitchActor(APitchActor* Pitch)
{
    PitchActor = Pitch;
}

void UCricketBowlingComponent::PrepareDelivery(const FDeliverySetup& Setup)
{
    CurrentSetup = Setup;

    // Validate delivery type exists
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(Setup.DeliveryType);
    if (!DelDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("Unknown delivery type: %d"), (int32)Setup.DeliveryType);
        return;
    }

    // Analyze predicted trajectory
    FDeliveryAnalysis Analysis;
    SimulateBallTrajectory(Setup, Analysis);

    // Broadcast for UI
    OnDeliveryPrepared.Broadcast(Analysis);

    // Transition to run-up phase
    CurrentPhase = EBowlingPhase::RunUpMarker;
}

void UCricketBowlingComponent::UpdateRunUpProgress(float Progress)
{
    RunUpProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
    OnRunUpProgress.Broadcast(RunUpProgress);
}

void UCricketBowlingComponent::OnRhythmTap(float TimingScore)
{
    // Smooth rhythm score
    RhythmScore = (RhythmScore * 0.8f) + (TimingScore * 0.2f);
    RhythmScore = FMath::Clamp(RhythmScore, 0.0f, 1.0f);
}

void UCricketBowlingComponent::SetEffort(float Effort)
{
    CurrentSetup.Effort = FMath::Clamp(Effort, 0.0f, 1.0f);
}

void UCricketBowlingComponent::SetSeamOrientation(const FVector& Orientation)
{
    CurrentSetup.SeamOrientation = Orientation.GetSafeNormal();
}

void UCricketBowlingComponent::SetSpinAxis(const FVector& Axis, float RPM)
{
    CurrentSetup.SpinAxis = Axis.GetSafeNormal();
    CurrentSetup.SpinRPM = FMath::Max(RPM, 0.0f);
}

void UCricketBowlingComponent::ReleaseBall(const FVector& ReleaseLocation, const FVector& ReleaseVelocity, const FVector& ReleaseSpin)
{
    if (CurrentPhase != EBowlingPhase::RunUp && CurrentPhase != EBowlingPhase::Release)
    {
        return;
    }

    CurrentPhase = EBowlingPhase::Release;

    // Spawn ball
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = BowlingPawn;
    SpawnParams.Instigator = BowlingPawn ? BowlingPawn->GetInstigator() : nullptr;

    CurrentBall = GetWorld()->SpawnActor<ACricketBall>(ACricketBall::StaticClass(), ReleaseLocation, FRotator::ZeroRotator, SpawnParams);

    if (CurrentBall)
    {
        FBallPhysicsParams BallParams;
        BallParams.Mass = 0.156f;
        BallParams.Radius = 0.036f;

        CurrentBall->Initialize(EBallType::RedBall, BallParams);
        CurrentBall->ReleaseFromHand(ReleaseLocation, ReleaseVelocity, ReleaseSpin);
        CurrentBall->PhysicsState.SeamOrientation = CurrentSetup.SeamOrientation;

        // Set pitch reference
        if (PitchActor)
        {
            CurrentBall->PitchActor = PitchActor;
        }
    }

    // Execute delivery logic
    FDeliveryResult Result = ExecuteDeliveryInternal();
    LastDeliveryResult = Result;

    // Check no-ball
    Result.bNoBall = CheckNoBall(ReleaseLocation);

    // Check wide (simplified)
    Result.bWide = CheckWide(Result.PredictedPitchLocation, true); // Assume right-handed batter

    // Apply stamina cost
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(CurrentSetup.DeliveryType);
    if (DelDef)
    {
        CurrentStamina = FMath::Max(0.0f, CurrentStamina - DelDef->EffortCost * 100.0f * CurrentSetup.Effort);
    }

    // Broadcast
    OnDeliveryBowled.Broadcast(Result);

    // Transition to follow through
    CurrentPhase = EBowlingPhase::FollowThrough;

    // Reset after delay
    FTimerHandle ResetTimer;
    GetWorld()->GetTimerManager().SetTimer(ResetTimer, [this]()
    {
        CurrentPhase = EBowlingPhase::SettingField;
        RunUpProgress = 0.0f;
        RhythmScore = 1.0f;
        BallsInCurrentOver++;

        if (BallsInCurrentOver >= 6)
        {
            BallsInCurrentOver = 0;
            OversBowled++;
            CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + 20.0f); // Recover some stamina per over
        }
    }, 2.0f, false);
}

FDeliveryAnalysis UCricketBowlingComponent::AnalyzeCurrentDelivery() const
{
    FDeliveryAnalysis Analysis;
    SimulateBallTrajectory(CurrentSetup, Analysis);
    return Analysis;
}

void UCricketBowlingComponent::UpdateStamina(float DeltaTime)
{
    // Passive stamina recovery
    if (CurrentPhase == EBowlingPhase::SettingField || CurrentPhase == EBowlingPhase::Idle)
    {
        CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + DeltaTime * 5.0f * Attributes.Stamina);
    }
}

FDeliveryResult UCricketBowlingComponent::ExecuteDeliveryInternal()
{
    FDeliveryResult Result;
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(CurrentSetup.DeliveryType);

    if (DelDef)
    {
        Result.DeliveryType = CurrentSetup.DeliveryType;

        // Speed calculation
        float SpeedVariation = FMath::RandRange(-3.0f, 3.0f);
        float EffortMultiplier = FMath::Lerp(0.85f, 1.0f, CurrentSetup.Effort);
        float RhythmMultiplier = 0.9f + RhythmScore * 0.2f;
        float AccuracyMultiplier = 0.9f + Attributes.Accuracy * 0.1f;

        Result.ActualSpeed = DelDef->BaseSpeed * EffortMultiplier * RhythmMultiplier * AccuracyMultiplier + SpeedVariation;
        Result.ActualSpeed = FMath::Clamp(Result.ActualSpeed, DelDef->BaseSpeed * 0.75f, DelDef->MaxSpeed);

        // Spin
        Result.ActualSpinRPM = CurrentSetup.SpinRPM * CurrentSetup.Effort;
        Result.ActualSpinAxis = CurrentSetup.SpinAxis;

        // Swing
        Result.ActualSwing = CalculateSwingAmount(CurrentSetup);

        // Release info
        Result.ReleaseLocation = CalculateReleaseLocation();
        Result.ReleaseVelocity = CalculateReleaseVelocity();
        Result.ReleaseSpin = CalculateReleaseSpin();

        // Pitch prediction
        Result.PredictedPitchLocation = CurrentSetup.TargetPitchLocation;

        // Scores
        Result.TimingScore = RhythmScore;
        float AimAccuracy = 1.0f - FMath::Clamp(FVector::Dist2D(CurrentSetup.TargetPitchLocation, Result.PredictedPitchLocation) / 100.0f, 0.0f, 1.0f);
        Result.AccuracyScore = (AimAccuracy + RhythmScore) * 0.5f;

        // Apply attributes
        ApplyAttributesToDelivery(Result);
    }

    return Result;
}

FVector UCricketBowlingComponent::CalculateReleaseVelocity() const
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(CurrentSetup.DeliveryType);
    float SpeedKMH = DelDef ? DelDef->BaseSpeed : 130.0f;

    // Apply effort and attributes
    SpeedKHM *= FMath::Lerp(0.85f, 1.0f, CurrentSetup.Effort) * Attributes.Pace;
    float SpeedCMS = SpeedKMH * 100000.0f / 3600.0f;

    // Direction towards target
    FVector BowlerPos = BowlingPawn ? BowlingPawn->GetActorLocation() : FVector::ZeroVector;
    FVector Direction = (CurrentSetup.TargetPitchLocation - BowlerPos).GetSafeNormal2D();

    // Add vertical component from release angle
    Direction.Z = FMath::Sin(FMath::DegreesToRadians(CurrentSetup.ReleaseAngle));
    Direction = Direction.GetSafeNormal();

    return Direction * SpeedCMS;
}

FVector UCricketBowlingComponent::CalculateReleaseSpin() const
{
    float SpinRadPerSec = CurrentSetup.SpinRPM * (PI / 30.0f); // RPM to rad/s
    return CurrentSetup.SpinAxis * SpinRadPerSec;
}

FVector UCricketBowlingComponent::CalculateReleaseLocation() const
{
    if (!BowlingPawn)
    {
        return FVector::ZeroVector;
    }

    FVector Location = BowlingPawn->GetActorLocation();
    Location.Z = CurrentSetup.ReleaseHeight * 100.0f; // Convert to cm

    // Arm extension
    FVector Forward = BowlingPawn->GetActorForwardVector();
    Location += Forward * 50.0f;

    return Location;
}

void UCricketBowlingComponent::ApplyAttributesToDelivery(FDeliveryResult& Result) const
{
    // Pace attribute directly affects speed
    Result.ActualSpeed *= Attributes.Pace;

    // Accuracy tightens the distribution
    // (Already factored into base calculation)

    // Swing attribute
    Result.ActualSwing *= Attributes.Swing;

    // Seam attribute affects seam deviation
    // (Handled in ball physics)

    // Spin attribute
    Result.ActualSpinRPM *= Attributes.Spin;

    // Variations - affects deception (not directly modeled here)
}

void UCricketBowlingComponent::SimulateBallTrajectory(const FDeliverySetup& Setup, FDeliveryAnalysis& OutAnalysis) const
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(Setup.DeliveryType);
    if (!DelDef || !PitchActor)
    {
        return;
    }

    // Initial conditions
    FVector ReleaseLoc = CalculateReleaseLocation();
    FVector ReleaseVel = CalculateReleaseVelocity();
    FVector ReleaseSpin = CalculateReleaseSpin();

    // Simplified simulation - just predict pitch location and batter contact
    // Real implementation would step through physics

    // Time to pitch
    float HorizontalSpeed = FVector(ReleaseVel.X, ReleaseVel.Y, 0.0f).Size();
    float TimeToPitch = 0.0f;

    if (HorizontalSpeed > 10.0f)
    {
        float DistanceToPitch = FVector::Dist2D(ReleaseLoc, Setup.TargetPitchLocation);
        TimeToPitch = DistanceToPitch / HorizontalSpeed;
    }

    // Predicted pitch location (with swing/seam)
    float SwingDeviation = CalculateSwingAmount(Setup) * TimeToPitch * 100.0f; // Convert to cm
    float SeamDeviation = CalculateSeamDeviation(Setup);

    OutAnalysis.PredictedPitchLocation = Setup.TargetPitchLocation;
    OutAnalysis.PredictedPitchLocation.Y += SwingDeviation + SeamDeviation;

    // Predicted speed at pitch (slight decay)
    OutAnalysis.PredictedSpeed = ReleaseVel.Size() * 3.6f / 100.0f * 0.9f; // km/h, ~10% decay

    // Predicted swing
    OutAnalysis.PredictedSwing = SwingDeviation / 100.0f; // meters

    // Predicted spin deviation (after bounce)
    OutAnalysis.PredictedSpinDeviation = CalculateSpinDeviation(Setup);

    // Bounce prediction
    if (PitchActor)
    {
        FPitchZoneData ZoneData = PitchActor->GetZoneDataAtLocation(OutAnalysis.PredictedPitchLocation);
        OutAnalysis.PredictedBounceHeight = ZoneData.Restitution * ReleaseVel.Z * 0.5f; // Rough
        OutAnalysis.PredictedBounceDeviation = ZoneData.SpinDeviation * Setup.SpinRPM / 3000.0f;
    }

    // Batter contact point
    FVector BatterCrease = FVector(122.0f, 0.0f, 0.0f); // Popping crease
    float TimeAfterBounce = (BatterCrease.X - OutAnalysis.PredictedPitchLocation.X) / HorizontalSpeed;
    OutAnalysis.PredictedBatterContactPoint = OutAnalysis.PredictedPitchLocation + FVector(HorizontalSpeed * TimeAfterBounce, 0.0f, 0.0f);
    OutAnalysis.PredictedBatterContactPoint.Z = 0.5f; // Knee height approx

    // No-ball check
    OutAnalysis.bNoBallRisk = false; // Would check front foot

    // Wide check
    OutAnalysis.bWideRisk = FMath::Abs(OutAnalysis.PredictedBatterContactPoint.Y) > 85.0f; // Wide line
}

const FDeliveryDefinition* UCricketBowlingComponent::GetDeliveryDefinition(EBowlingDeliveryType Type) const
{
    for (const FDeliveryDefinition& Def : DeliveryDefinitions)
    {
        if (Def.DeliveryType == Type)
        {
            return &Def;
        }
    }
    return nullptr;
}

float UCricketBowlingComponent::CalculateSwingAmount(const FDeliverySetup& Setup) const
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(Setup.DeliveryType);
    if (!DelDef)
    {
        return 0.0f;
    }

    // Base swing from delivery type
    float BaseSwing = DelDef->SwingAmount;

    // Seam position effect (more upright seam = more swing)
    float SeamEffect = 1.0f - FMath::Abs(Setup.SeamOrientation.Z) * 0.5f;

    // Speed effect (optimal swing around 120-135 km/h)
    float SpeedKMH = DelDef->BaseSpeed * Setup.Effort;
    float SpeedEffect = 1.0f - FMath::Abs(SpeedKMH - 130.0f) / 130.0f * 0.5f;

    // Ball age effect (newer ball swings more)
    // Would need ball reference

    // Atmospheric conditions
    // Would need environment reference

    return BaseSwing * SeamEffect * SpeedEffect * Attributes.Swing * 0.5f; // Meters of deviation
}

float UCricketBowlingComponent::CalculateSeamDeviation(const FDeliverySetup& Setup) const
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition(Setup.DeliveryType);
    if (!DelDef)
    {
        return 0.0f;
    }

    // Seam position determines deviation direction
    float SeamDeviation = DelDef->SeamPosition * Setup.SeamOrientation.Y * Attributes.Seam * 0.1f; // meters

    // Random component
    SeamDeviation += FMath::RandRange(-0.05f, 0.05f) * Attributes.Seam;

    return SeamDeviation;
}

float UCricketBowlingComponent::CalculateSpinDeviation(const FDeliverySetup& Setup) const
{
    // Spin deviation after bounce
    // Depends on pitch friction, spin RPM, and spin axis
    float SpinFactor = Setup.SpinRPM / 3000.0f; // Normalize to 3000 RPM max
    float AxisEffect = FMath::Abs(Setup.SpinAxis.Z); // Vertical spin = more turn

    return SpinFactor * AxisEffect * Attributes.Spin * 0.5f; // meters
}

bool UCricketBowlingComponent::CheckNoBall(const FVector& FrontFootLocation) const
{
    // Popping crease at X = 122cm from stumps (which are at X=0)
    // Bowling crease at X = -122cm
    float PoppingCreaseX = 122.0f;

    if (FrontFootLocation.X > PoppingCreaseX)
    {
        return true;
    }

    return false;
}

bool UCricketBowlingComponent::CheckWide(const FVector& PitchLocation, bool bRightHandedBatter) const
{
    // Wide line: 85cm (0.85m) from off stump for right-handed batter
    // Off stump at Y = 22.86cm (9 inches) from center
    float WideLine = 85.0f + 22.86f; // ~108cm from center

    if (bRightHandedBatter)
    {
        // Off side wide
        return PitchLocation.Y > WideLine;
    }
    else
    {
        // Leg side wide (stricter)
        return PitchLocation.Y < -WideLine * 0.5f;
    }
}

void UCricketBowlingComponent::UpdateBallPhysics(float DeltaTime)
{
    if (CurrentBall)
    {
        CurrentBall->SimulatePhysics(DeltaTime);
    }
}