#include "DRSSystem.h"
#include "CricketBall.h"
#include "CricketGameMode.h"
#include "PitchActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UDRSSystem::UDRSSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UDRSSystem::BeginPlay()
{
    Super::BeginPlay();

    GameMode = Cast<ACricketGameMode>(UGameplayStatics::GetGameMode(GetWorld()));

    // Find pitch actor
    TArray<AActor*> PitchActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APitchActor::StaticClass(), PitchActors);
    if (PitchActors.Num() > 0)
    {
        PitchActor = Cast<APitchActor>(PitchActors[0]);
    }
}

void UDRSSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bReviewActive)
    {
        UpdateStageProgress(DeltaTime);
    }
}

void UDRSSystem::RequestReview(const FDRSReviewRequest& Request)
{
    if (bReviewActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("DRS review already in progress"));
        return;
    }

    // Check if team has reviews remaining
    // Would check with GameState

    bReviewActive = true;
    CurrentRequest = Request;
    CurrentStage = EDRSStage::Initiated;
    StageProgress = 0.0f;

    // Initialize result
    CurrentResult = FDRSResult();
    CurrentResult.BallTracking = FBallTrackingData();
    CurrentResult.UltraEdge = FUltraEdgeData();
    CurrentResult.HotSpot = FHotSpotData();

    BroadcastStageChange(EDRSStage::Initiated);

    // Start first stage
    AdvanceStage();
}

void UDRSSystem::CancelReview()
{
    if (!bReviewActive)
    {
        return;
    }

    bReviewActive = false;
    CurrentStage = EDRSStage::Complete;
    BroadcastStageChange(EDRSStage::Complete);
}

void UDRSSystem::RunBallTracking()
{
    if (CurrentStage != EDRSStage::BallTracking)
    {
        return;
    }

    CurrentResult.BallTracking = SimulateBallTracking(CurrentRequest);
    StageProgress = 1.0f;
}

void UDRSSystem::RunUltraEdge()
{
    if (CurrentStage != EDRSStage::UltraEdge)
    {
        return;
    }

    CurrentResult.UltraEdge = SimulateUltraEdge(CurrentRequest);
    StageProgress = 1.0f;
}

void UDRSSystem::RunHotSpot()
{
    if (CurrentStage != EDRSStage::HotSpot)
    {
        return;
    }

    CurrentResult.HotSpot = SimulateHotSpot(CurrentRequest);
    StageProgress = 1.0f;
}

void UDRSSystem::MakeDecision()
{
    if (CurrentStage != EDRSStage::Decision)
    {
        return;
    }

    EDRSOutcome Decision = EDRSOutcome::Inconclusive;

    switch (CurrentRequest.ReviewType)
    {
        case EDRSReviewType::LBW:
            Decision = DetermineLBWDecision(CurrentResult.BallTracking, CurrentResult.UltraEdge, CurrentResult.HotSpot);
            break;
        case EDRSReviewType::CaughtBehind:
        case EDRSReviewType::Caught:
            Decision = DetermineCaughtDecision(CurrentResult.UltraEdge, CurrentResult.HotSpot);
            break;
        case EDRSReviewType::NoBall:
            Decision = DetermineNoBallDecision(CurrentRequest);
            break;
        case EDRSReviewType::Boundary:
            Decision = DetermineBoundaryDecision(CurrentRequest);
            break;
        default:
            Decision = EDRSOutcome::Inconclusive;
            break;
    }

    CurrentResult.FinalDecision = Decision;

    // Check if original decision overturned
    bool bOriginalOut = CurrentRequest.UmpireDecision.Equals(TEXT("Out"), ESearchCase::IgnoreCase);
    bool bNewOut = (Decision == EDRSOutcome::Out);

    CurrentResult.bOriginalDecisionOverturned = (bOriginalOut != bNewOut);

    // Build reasoning
    BuildReasoning(Decision);

    StageProgress = 1.0f;
}

void UDRSSystem::AdvanceStage()
{
    switch (CurrentStage)
    {
        case EDRSStage::Initiated:
            CurrentStage = EDRSStage::BallTracking;
            StageDuration = BallTrackingDuration;
            break;
        case EDRSStage::BallTracking:
            CurrentStage = EDRSStage::UltraEdge;
            StageDuration = UltraEdgeDuration;
            break;
        case EDRSStage::UltraEdge:
            CurrentStage = EDRSStage::HotSpot;
            StageDuration = HotSpotDuration;
            break;
        case EDRSStage::HotSpot:
            CurrentStage = EDRSStage::Decision;
            StageDuration = DecisionDuration;
            break;
        case EDRSStage::Decision:
            CurrentStage = EDRSStage::Complete;
            StageDuration = 0.0f;
            break;
        case EDRSStage::Complete:
            bReviewActive = false;
            OnDRSCompleted.Broadcast(CurrentResult);
            return;
    }

    StageProgress = 0.0f;
    BroadcastStageChange(CurrentStage);
}

void UDRSSystem::UpdateStageProgress(float DeltaTime)
{
    if (StageDuration <= 0.0f)
    {
        return;
    }

    StageProgress = FMath::Clamp(StageProgress + DeltaTime / StageDuration, 0.0f, 1.0f);

    // Auto-advance when stage complete (for simulation)
    if (StageProgress >= 1.0f)
    {
        switch (CurrentStage)
        {
            case EDRSStage::BallTracking:
                RunBallTracking();
                break;
            case EDRSStage::UltraEdge:
                RunUltraEdge();
                break;
            case EDRSStage::HotSpot:
                RunHotSpot();
                break;
            case EDRSStage::Decision:
                MakeDecision();
                break;
            default:
                break;
        }

        // Small delay before next stage
        FTimerHandle NextStageTimer;
        GetWorld()->GetTimerManager().SetTimer(NextStageTimer, [this]()
        {
            AdvanceStage();
        }, 1.0f, false);
    }
}

void UDRSSystem::BroadcastStageChange(EDRSStage NewStage)
{
    CurrentStage = NewStage;
    OnDRSStageChanged.Broadcast(NewStage);
}

FBallTrackingData UDRSSystem::SimulateBallTracking(const FDRSReviewRequest& Request) const
{
    FBallTrackingData Result;

    if (!Request.BallActor)
    {
        Result.Confidence = 0.0f;
        return Result;
    }

    ACricketBall* Ball = Request.BallActor;
    FBallPhysicsState Physics = Ball->GetPhysicsState();

    // Get actual trajectory from physics simulation
    Result.ActualPath = Ball->PredictTrajectory(0.05f, 100, true);

    // Predict forward from current state
    FVector CurrentPos = Physics.Position;
    FVector CurrentVel = Physics.Velocity;
    FVector CurrentSpin = Physics.AngularVelocity;

    // Simulate forward to stumps
    float TimeStep = 0.01f;
    int32 MaxSteps = 500;
    FVector Pos = CurrentPos;
    FVector Vel = CurrentVel;
    FVector Spin = CurrentSpin;

    for (int32 i = 0; i < MaxSteps; i++)
    {
        Result.PredictedPath.Add(Pos);

        // Simple physics step
        FVector Accel = FVector(0.0f, 0.0f, -980.0f); // Gravity

        // Magnus force
        FVector Magnus = FVector::CrossProduct(Spin, Vel) * 1.2e-4f;
        Accel += Magnus / 0.156f; // Mass

        // Drag
        float Speed = Vel.Size();
        if (Speed > 10.0f)
        {
            FVector DragDir = -Vel.GetSafeNormal();
            float DragMag = 0.5f * 1.225f * 0.5f * Speed * Speed * PI * 0.036f * 0.036f;
            Accel += DragDir * DragMag / 0.156f;
        }

        Vel += Accel * TimeStep;
        Pos += Vel * TimeStep;

        // Check for pitch bounce
        if (Pos.Z <= 0.0f && Vel.Z < 0.0f)
        {
            // Bounce
            Pos.Z = 0.0f;
            Vel.Z = -Vel.Z * 0.6f; // Coefficient of restitution
            Vel.X *= 0.9f; // Friction
            Vel.Y *= 0.9f;

            // Spin deviation
            float SpinDev = FVector::CrossProduct(Spin, FVector::UpVector).Size() * 0.3f;
            Vel.Y += SpinDev * 100.0f;
        }

        // Check if past stumps
        if (Pos.X > 0.0f && Pos.X > Request.StumpPosition.X)
        {
            Result.PredictedStumpImpact = Pos;
            break;
        }
    }

    // Analyze LBW criteria
    // Pitch in line: ball pitches between wicket-to-wicket
    FVector PitchPoint;
    // Find bounce point
    for (int32 i = 1; i < Result.PredictedPath.Num(); i++)
    {
        if (Result.PredictedPath[i-1].Z > 10.0f && Result.PredictedPath[i].Z <= 10.0f)
        {
            PitchPoint = Result.PredictedPath[i];
            break;
        }
    }

    Result.bPitchesInLine = FMath::Abs(PitchPoint.Y) < 0.7f; // Within 70cm of center

    // Impact in line: ball hits pads in line with stumps
    Result.PredictedImpactPoint = Physics.Position; // Approximate
    Result.bImpactInLine = FMath::Abs(Result.PredictedImpactPoint.Y) < 0.3f; // Within 30cm

    // Hits stumps
    Result.bHitsStumps = FMath::Abs(Result.PredictedStumpImpact.Y) < 0.2f && // Within stump width
                         Result.PredictedStumpImpact.Z < 0.7f; // Below bail height

    Result.StumpImpactHeight = Result.PredictedStumpImpact.Z;
    Result.StumpImpactOffset = Result.PredictedStumpImpact.Y;

    // Confidence based on how far we predicted
    float DistanceToStumps = Request.StumpPosition.X - CurrentPos.X;
    Result.Confidence = FMath::Clamp(1.0f - DistanceToStumps / 2000.0f, 0.5f, 1.0f);

    return Result;
}

FUltraEdgeData UDRSSystem::SimulateUltraEdge(const FDRSReviewRequest& Request) const
{
    FUltraEdgeData Result;

    // Simulate audio waveform around ball passing bat
    // In real implementation, this would analyze actual microphone input
    // For simulation, we generate synthetic data

    int32 SampleCount = 1000; // 100ms at 10kHz
    Result.AudioWaveform.SetNum(SampleCount);
    Result.SyncedWaveform.SetNum(SampleCount);

    // Base noise
    for (int32 i = 0; i < SampleCount; i++)
    {
        Result.AudioWaveform[i] = FMath::RandRange(-0.1f, 0.1f);
        Result.SyncedWaveform[i] = Result.AudioWaveform[i];
    }

    // Determine if there was an edge based on ball physics
    bool bActuallyEdged = false;
    if (Request.BallActor)
    {
        // Would check if ball state was HitBat and contact was near edge
        // For simulation, use random with bias based on review type
        if (Request.ReviewType == EDRSReviewType::CaughtBehind || Request.ReviewType == EDRSReviewType::Caught)
        {
            bActuallyEdged = FMath::FRand() < 0.6f; // 60% chance of actual edge for caught reviews
        }
    }

    if (bActuallyEdged)
    {
        // Add spike at contact time
        int32 SpikeIndex = SampleCount / 2; // Middle of window
        Result.SpikeTime = 0.0f; // Relative to ball pass
        Result.SpikeMagnitude = FMath::RandRange(0.5f, 1.0f);
        Result.bSpikeDetected = true;
        Result.bSpikeAlignedWithBall = true;

        Result.SyncedWaveform[SpikeIndex] += Result.SpikeMagnitude;
        if (SpikeIndex > 0) Result.SyncedWaveform[SpikeIndex - 1] += Result.SpikeMagnitude * 0.5f;
        if (SpikeIndex < SampleCount - 1) Result.SyncedWaveform[SpikeIndex + 1] += Result.SpikeMagnitude * 0.5f;

        Result.Confidence = 0.8f + FMath::FRand() * 0.2f;
    }
    else
    {
        Result.SpikeMagnitude = 0.0f;
        Result.bSpikeDetected = false;
        Result.bSpikeAlignedWithBall = false;
        Result.Confidence = 0.9f; // Confident no edge
    }

    return Result;
}

FHotSpotData UDRSSystem::SimulateHotSpot(const FDRSReviewRequest& Request) const
{
    FHotSpotData Result;

    // Simulate thermal imaging
    // HotSpot detects friction heat at contact point

    bool bActuallyEdged = false;
    if (Request.BallActor)
    {
        // Same logic as UltraEdge
        if (Request.ReviewType == EDRSReviewType::CaughtBehind || Request.ReviewType == EDRSReviewType::Caught)
        {
            bActuallyEdged = FMath::FRand() < 0.6f;
        }
    }

    if (bActuallyEdged)
    {
        // Generate hot spot on bat edge
        int32 SpotCount = FMath::RandRange(1, 3);
        Result.HotSpots.SetNum(SpotCount);
        Result.Temperatures.SetNum(SpotCount);

        for (int32 i = 0; i < SpotCount; i++)
        {
            // UV coordinates on bat (0-1)
            Result.HotSpots[i] = FVector2D(FMath::FRand(), FMath::FRand());
            Result.Temperatures[i] = FMath::RandRange(2.0f, 8.0f); // Temperature rise in Celsius
        }

        Result.bHotSpotOnBat = true;
        Result.PrimaryHotSpotUV = Result.HotSpots[0];
        Result.PrimaryTemperature = Result.Temperatures[0];

        // Occasionally also show on pad (glancing)
        if (FMath::FRand() < 0.3f)
        {
            Result.bHotSpotOnPad = true;
        }

        Result.Confidence = 0.75f + FMath::FRand() * 0.15f;
    }
    else
    {
        Result.bHotSpotOnBat = false;
        Result.bHotSpotOnPad = false;
        Result.Confidence = 0.85f;
    }

    return Result;
}

void UDRSSystem::BuildReasoning(EDRSOutcome Decision)
{
    FString Reason;

    switch (CurrentRequest.ReviewType)
    {
        case EDRSReviewType::LBW:
            if (Decision == EDRSOutcome::Out)
            {
                Reason = TEXT("Ball tracking shows: Pitches in line, impact in line, hitting stumps. ");
                if (CurrentResult.UltraEdge.bSpikeDetected)
                {
                    Reason += TEXT("UltraEdge shows no spike (bat not involved). ");
                }
                if (!CurrentResult.HotSpot.bHotSpotOnBat)
                {
                    Reason += TEXT("HotSpot confirms no bat contact. ");
                }
            }
            else if (Decision == EDRSOutcome::NotOut)
            {
                if (!CurrentResult.BallTracking.bPitchesInLine)
                {
                    Reason = TEXT("Ball pitches outside leg stump. ");
                }
                else if (!CurrentResult.BallTracking.bImpactInLine)
                {
                    Reason = TEXT("Impact outside off stump. ");
                }
                else if (!CurrentResult.BallTracking.bHitsStumps)
                {
                    Reason = FString::Printf(TEXT("Ball missing stumps (impact height: %.2fm, offset: %.2fm). "),
                        CurrentResult.BallTracking.StumpImpactHeight, CurrentResult.BallTracking.StumpImpactOffset);
                }
                else
                {
                    Reason = TEXT("Umpire's call - marginal. ");
                }

                if (CurrentResult.UltraEdge.bSpikeDetected)
                {
                    Reason += TEXT("UltraEdge shows spike - bat involved. ");
                }
            }
            else
            {
                Reason = TEXT("Umpire's call - evidence inconclusive. ");
            }
            break;

        case EDRSReviewType::CaughtBehind:
        case EDRSReviewType::Caught:
            if (Decision == EDRSOutcome::Out)
            {
                Reason = TEXT("Clear edge detected. ");
                if (CurrentResult.UltraEdge.bSpikeDetected)
                {
                    Reason += TEXT("UltraEdge shows spike aligned with ball pass. ");
                }
                if (CurrentResult.HotSpot.bHotSpotOnBat)
                {
                    Reason += TEXT("HotSpot confirms contact on bat. ");
                }
            }
            else if (Decision == EDRSOutcome::NotOut)
            {
                Reason = TEXT("No conclusive evidence of bat contact. ");
                if (!CurrentResult.UltraEdge.bSpikeDetected)
                {
                    Reason += TEXT("UltraEdge shows no spike. ");
                }
                if (!CurrentResult.HotSpot.bHotSpotOnBat)
                {
                    Reason += TEXT("HotSpot shows no bat contact. ");
                }
            }
            else
            {
                Reason = TEXT("Inconclusive - conflicting evidence. ");
            }
            break;

        case EDRSReviewType::NoBall:
            if (Decision == EDRSOutcome::Out)
            {
                Reason = TEXT("Front foot over the line - No Ball. ");
            }
            else
            {
                Reason = TEXT("Front foot behind the line - Fair delivery. ");
            }
            break;

        case EDRSReviewType::Boundary:
            if (Decision == EDRSOutcome::Out) // Six
            {
                Reason = TEXT("Ball cleared boundary on the full - Six. ");
            }
            else
            {
                Reason = TEXT("Ball bounced before boundary - Four. ");
            }
            break;

        default:
            Reason = TEXT("Review type not recognized. ");
            break;
    }

    CurrentResult.Reasoning = Reason;
    CurrentResult.OverallConfidence = (CurrentResult.BallTracking.Confidence +
                                        CurrentResult.UltraEdge.Confidence +
                                        CurrentResult.HotSpot.Confidence) / 3.0f;
}

EDRSOutcome UDRSSystem::DetermineLBWDecision(const FBallTrackingData& Tracking, const FUltraEdgeData& Edge, const FHotSpotData& HotSpot) const
{
    // If edge detected, not out (caught behind takes precedence)
    if (Edge.bSpikeDetected && Edge.bSpikeAlignedWithBall)
    {
        return EDRSOutcome::NotOut;
    }

    if (HotSpot.bHotSpotOnBat)
    {
        return EDRSOutcome::NotOut;
    }

    // Check LBW criteria
    if (!Tracking.bPitchesInLine)
    {
        return EDRSOutcome::NotOut; // Pitches outside leg
    }

    if (!Tracking.bImpactInLine)
    {
        // Impact outside off - not out if playing shot
        // For simulation, assume playing shot
        return EDRSOutcome::NotOut;
    }

    if (!Tracking.bHitsStumps)
    {
        // Check if umpire's call zone
        float HeightMargin = FMath::Abs(Tracking.StumpImpactHeight - 0.35f); // Middle of stumps
        float OffsetMargin = FMath::Abs(Tracking.StumpImpactOffset);

        if (HeightMargin < 0.1f && OffsetMargin < 0.05f)
        {
            return EDRSOutcome::UmpiresCall;
        }
        return EDRSOutcome::NotOut;
    }

    // All three reds
    return EDRSOutcome::Out;
}

EDRSOutcome UDRSSystem::DetermineCaughtDecision(const FUltraEdgeData& Edge, const FHotSpotData& HotSpot) const
{
    bool bEdgeEvidence = Edge.bSpikeDetected && Edge.bSpikeAlignedWithBall;
    bool bHotSpotEvidence = HotSpot.bHotSpotOnBat;

    if (bEdgeEvidence && bHotSpotEvidence)
    {
        return EDRSOutcome::Out; // Both confirm
    }
    else if (bEdgeEvidence || bHotSpotEvidence)
    {
        // One piece of evidence - check confidence
        float Confidence = bEdgeEvidence ? Edge.Confidence : HotSpot.Confidence;
        if (Confidence > 0.85f)
        {
            return EDRSOutcome::Out;
        }
        else
        {
            return EDRSOutcome::UmpiresCall;
        }
    }
    else
    {
        return EDRSOutcome::NotOut;
    }
}

EDRSOutcome UDRSSystem::DetermineNoBallDecision(const FDRSReviewRequest& Request) const
{
    // Would check actual front foot position from bowling component
    // For simulation
    bool bNoBall = FMath::FRand() < 0.3f; // 30% chance of no-ball in reviews

    return bNoBall ? EDRSOutcome::Out : EDRSOutcome::NotOut; // "Out" = No Ball called
}

EDRSOutcome UDRSSystem::DetermineBoundaryDecision(const FDRSReviewRequest& Request) const
{
    // Would check ball trajectory and boundary contact
    bool bSix = FMath::FRand() < 0.5f;

    return bSix ? EDRSOutcome::Out : EDRSOutcome::NotOut; // "Out" = Six, "Not Out" = Four
}