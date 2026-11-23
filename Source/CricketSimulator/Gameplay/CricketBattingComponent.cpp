#include "CricketBattingComponent.h"
#include "CricketBattingPawn.h"
#include "CricketBall.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UCricketBattingComponent::UCricketBattingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Every frame for timing precision
}

void UCricketBattingComponent::BeginPlay()
{
    Super::BeginPlay();

    // Load shot definitions from DataTable if provided
    if (ShotDefinitionsTable)
    {
        TArray<FShotDefinition*> Rows;
        ShotDefinitionsTable->GetAllRows(TEXT(""), Rows);
        for (FShotDefinition* Row : Rows)
        {
            if (Row)
            {
                ShotDefinitions.Add(*Row);
            }
        }
    }
}

void UCricketBattingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bShotPending && TargetBall)
    {
        UpdateTimingWindow(DeltaTime);

        // Broadcast swing progress for animation sync
        if (BattingPawn)
        {
            OnBatSwingProgress.Broadcast(BattingPawn->GetSwingProgress(), BattingPawn->GetActorTransform());
        }
    }
}

void UCricketBattingComponent::SetBattingPawn(ACricketBattingPawn* Pawn)
{
    BattingPawn = Pawn;
}

void UCricketBattingComponent::SetTargetBall(ACricketBall* Ball)
{
    TargetBall = Ball;

    if (Ball)
    {
        CurrentBallArrivalTime = PredictBallArrivalTime();
    }
}

void UCricketBattingComponent::UpdateBatPhysicsState(const FBatPhysicsState& NewState)
{
    BatPhysicsState = NewState;
}

void UCricketBattingComponent::SetBattingAttributes(const FBattingAttributes& NewAttributes)
{
    Attributes = NewAttributes;
}

float UCricketBattingComponent::PredictBallArrivalTime() const
{
    if (!TargetBall)
    {
        return 0.0f;
    }

    const FBallPhysicsState& BallPhysics = TargetBall->GetPhysicsState();
    FVector BallPos = BallPhysics.Position;
    FVector BallVel = BallPhysics.Velocity;

    if (!BattingPawn)
    {
        return 0.0f;
    }

    FVector BatterPos = BattingPawn->GetActorLocation();
    float CreaseDistance = 122.0f; // cm from stumps to popping crease
    float BatterCreaseX = BatterPos.X + CreaseDistance;

    // Simple prediction: time until ball reaches batter's crease line
    // Assuming ball travels roughly along X axis
    float DistanceToCrease = FMath::Abs(BatterCreaseX - BallPos.X);

    if (BallVel.X > 10.0f) // Ball moving towards batter
    {
        float TimeToCrease = DistanceToCrease / BallVel.X;

        // Account for bounce - if ball hasn't bounced yet, add bounce time
        if (BallPhysics.Bounces == 0)
        {
            // Predict bounce point and add post-bounce time
            FVector PredictedBounce = TargetBall->GetPredictedPitchLocation(0.0f);
            if (PredictedBounce.X < BatterCreaseX)
            {
                // Ball will bounce before crease
                float TimeToBounce = FMath::Abs(PredictedBounce.X - BallPos.X) / BallVel.X;
                float PostBounceSpeed = BallVel.X * TargetBall->PhysicsParams.CoefficientOfRestitution;
                float PostBounceDistance = BatterCreaseX - PredictedBounce.X;
                float TimeAfterBounce = PostBounceDistance / PostBounceSpeed;

                return TimeToBounce + TimeAfterBounce;
            }
        }

        return TimeToCrease;
    }

    return FLT_MAX;
}

FVector UCricketBattingComponent::PredictBallContactPoint() const
{
    if (!TargetBall || !BattingPawn)
    {
        return FVector::ZeroVector;
    }

    float ArrivalTime = PredictBallArrivalTime();
    return TargetBall->GetActorLocation() + TargetBall->GetVelocity() * ArrivalTime;
}

void UCricketBattingComponent::ExecuteShot(EBattingShotType ShotType, const FTransform& BatTransform, const FVector& BatVelocity, float SwingTime, float BallArrivalTime)
{
    if (ShotType == EBattingShotType::None)
    {
        return;
    }

    // Detect contact
    FContactResult Contact = DetectBatBallContact(BatTransform, BatVelocity);

    if (!Contact.bContact)
    {
        // Missed shot
        FShotResult MissResult;
        MissResult.ShotType = ShotType;
        MissResult.Timing = DetermineTimingWindow(SwingTime, BallArrivalTime);
        MissResult.BatSpeedAtContact = CalculateBatSpeedAtContact(BatVelocity, BatTransform.GetLocation());
        MissResult.TimingScore = 0.0f;
        MissResult.PredictedRuns = 0;
        MissResult.bIsWicket = false;

        LastShotResult = MissResult;
        OnShotExecuted.Broadcast(MissResult);

        if (BattingPawn)
        {
            BattingPawn->LastShotResult = MissResult;
            BattingPawn->OnShotPlayed.Broadcast(MissResult);
        }

        bShotPending = false;
        return;
    }

    // Get shot definition
    const FShotDefinition* ShotDef = GetShotDefinition(ShotType);
    if (!ShotDef)
    {
        UE_LOG(LogTemp, Warning, TEXT("Shot definition not found for type: %d"), (int32)ShotType);
        return;
    }

    // Calculate shot result
    FShotResult Result;
    CalculateShotResult(*ShotDef, Contact, Result);

    LastShotResult = Result;
    OnShotExecuted.Broadcast(Result);

    if (BattingPawn)
    {
        BattingPawn->LastShotResult = Result;
        BattingPawn->OnShotPlayed.Broadcast(Result);
    }

    // Apply ball physics
    if (TargetBall)
    {
        FVector BatNormal = CalculateSweetSpotNormal(BatTransform);
        TargetBall->HandleBatCollision(Contact.ContactPoint, BatVelocity, BatNormal, BatPhysicsState.BatMass);
    }

    bShotPending = false;
    PendingShotType = EBattingShotType::None;
}

void UCricketBattingComponent::UpdateTimingWindow(float DeltaTime)
{
    if (!TargetBall)
    {
        return;
    }

    float ArrivalTime = PredictBallArrivalTime();

    // If we have a pending swing, evaluate timing
    if (bShotPending && PendingSwingTime > 0.0f)
    {
        float SwingTime = GetWorld()->GetTimeSeconds() - PendingSwingTime;
        ETimingWindow NewWindow = DetermineTimingWindow(SwingTime, ArrivalTime);

        if (NewWindow != CurrentTimingWindow)
        {
            CurrentTimingWindow = NewWindow;
            OnTimingWindowChanged.Broadcast(NewWindow);

            if (BattingPawn)
            {
                BattingPawn->OnTimingWindowUpdate.Broadcast(NewWindow);
            }
        }
    }
    else
    {
        // Update predictive timing window for UI
        float TimeUntilArrival = ArrivalTime - GetWorld()->GetTimeSeconds();

        if (TimeUntilArrival > 0.0f && TimeUntilArrival < 1.0f)
        {
            // Ball arriving soon - show predictive window
            // This would show "prepare for contact" UI
        }
    }
}

ETimingWindow UCricketBattingComponent::DetermineTimingWindow(float SwingTime, float BallArrivalTime) const
{
    float TimeDiff = SwingTime - BallArrivalTime; // Positive = late, Negative = early

    // Apply attributes - better timing = wider windows
    float TimingMultiplier = FMath::Clamp(Attributes.Timing, 0.5f, 2.0f);

    float VeryEarlyLimit = -VeryEarlyThreshold * TimingMultiplier;
    float EarlyLimit = -EarlyThreshold * TimingMultiplier;
    float PerfectLimit = PerfectThreshold * TimingMultiplier;
    float LateLimit = LateThreshold * TimingMultiplier;
    float VeryLateLimit = VeryLateThreshold * TimingMultiplier;

    if (TimeDiff < VeryEarlyLimit)
    {
        return ETimingWindow::VeryEarly;
    }
    else if (TimeDiff < EarlyLimit)
    {
        return ETimingWindow::Early;
    }
    else if (TimeDiff <= PerfectLimit && TimeDiff >= -PerfectLimit)
    {
        return ETimingWindow::Perfect;
    }
    else if (TimeDiff <= LateLimit)
    {
        return ETimingWindow::Late;
    }
    else if (TimeDiff <= VeryLateLimit)
    {
        return ETimingWindow::VeryLate;
    }
    else
    {
        return ETimingWindow::Missed;
    }
}

void UCricketBattingComponent::CalculateShotResult(const FShotDefinition& ShotDef, const FContactResult& Contact, FShotResult& OutResult)
{
    OutResult.ShotType = ShotDef.ShotType;
    OutResult.Timing = CurrentTimingWindow;
    OutResult.BatSpeedAtContact = Contact.RelativeSpeed;
    OutResult.ContactAngle = Contact.ContactAngle;
    OutResult.ContactPoint = Contact.ContactPoint;
    OutResult.BallVelocityAtContact = Contact.BallVelocityBefore;
    OutResult.BatVelocityAtContact = Contact.BatVelocityAtContact;

    // Base power from bat speed and shot definition
    float SpeedFactor = FMath::Clamp(Contact.RelativeSpeed / ShotDef.MaxBatSpeed, 0.0f, 1.0f);
    float BasePower = SpeedFactor * ShotDef.PowerMultiplier;

    // Timing modifier
    float TimingMod = 1.0f;
    switch (CurrentTimingWindow)
    {
        case ETimingWindow::Perfect: TimingMod = 1.2f; break;
        case ETimingWindow::Early: TimingMod = 0.9f; break;
        case ETimingWindow::Late: TimingMod = 0.8f; break;
        case ETimingWindow::VeryEarly: TimingMod = 0.6f; break;
        case ETimingWindow::VeryLate: TimingMod = 0.5f; break;
        default: TimingMod = 0.3f; break;
    }

    // Contact quality modifier
    float ContactMod = Contact.bSweetSpot ? 1.1f : (Contact.bEdge ? 0.4f : 0.8f);

    // Angle modifier - how perpendicular was the bat face?
    float AngleMod = 1.0f - FMath::Clamp(FMath::Abs(Contact.ContactAngle) / 90.0f, 0.0f, 1.0f);

    OutResult.ShotPower = BasePower * TimingMod * ContactMod * AngleMod;
    OutResult.ShotPower = FMath::Clamp(OutResult.ShotPower, 0.0f, 2.0f);

    // Control - placement accuracy
    float BaseControl = ShotDef.ControlMultiplier;
    float TimingControlMod = (CurrentTimingWindow == ETimingWindow::Perfect) ? 1.1f : 0.9f;
    float ContactControlMod = Contact.bSweetSpot ? 1.1f : (Contact.bEdge ? 0.3f : 0.8f);

    OutResult.ShotControl = BaseControl * TimingControlMod * ContactControlMod * Attributes.Technique;
    OutResult.ShotControl = FMath::Clamp(OutResult.ShotControl, 0.0f, 2.0f);

    // Timing score (0-1)
    switch (CurrentTimingWindow)
    {
        case ETimingWindow::Perfect: OutResult.TimingScore = 1.0f; break;
        case ETimingWindow::Early: OutResult.TimingScore = 0.8f; break;
        case ETimingWindow::Late: OutResult.TimingScore = 0.7f; break;
        case ETimingWindow::VeryEarly: OutResult.TimingScore = 0.5f; break;
        case ETimingWindow::VeryLate: OutResult.TimingScore = 0.4f; break;
        default: OutResult.TimingScore = 0.0f; break;
    }

    // Placement score based on shot type and control
    OutResult.PlacementScore = OutResult.ShotControl * FMath::FRandRange(0.8f, 1.0f);

    // Power score
    OutResult.PowerScore = OutResult.ShotPower * FMath::FRandRange(0.8f, 1.0f);

    // Apply attributes
    ApplyAttributesToShot(OutResult);

    // Predict runs
    OutResult.PredictedRuns = PredictRuns(OutResult);

    // Check for wicket
    FGameplayTag WicketType;
    OutResult.bIsWicket = CheckWicket(OutResult, WicketType);
    OutResult.WicketType = WicketType;
}

FContactResult UCricketBattingComponent::DetectBatBallContact(const FTransform& BatTransform, const FVector& BatVelocity) const
{
    FContactResult Result;

    if (!TargetBall)
    {
        return Result;
    }

    FVector BallPos = TargetBall->GetActorLocation();
    FVector BallVel = TargetBall->GetVelocity();
    float BallRadius = TargetBall->PhysicsParams.Radius * 100.0f; // Convert to cm

    // Sweet spot in world space
    FVector SweetSpot = CalculateSweetSpotWorldLocation(BatTransform);
    FVector SweetSpotNormal = CalculateSweetSpotNormal(BatTransform);

    // Distance from ball to sweet spot
    float DistToSweetSpot = FVector::Dist(BallPos, SweetSpot);
    float SweetSpotRadius = BatPhysicsState.SweetSpotRadius * 100.0f; // cm

    // Check if ball intersects bat volume
    // Simplified: check distance to sweet spot plane
    FVector ToBall = BallPos - SweetSpot;
    float DistAlongNormal = FVector::DotProduct(ToBall, SweetSpotNormal);
    float DistPerpendicular = FVector::Dist(BallPos, SweetSpot + SweetSpotNormal * DistAlongNormal);

    // Ball radius + bat thickness buffer
    float ContactThreshold = BallRadius + 5.0f; // 5cm bat thickness

    if (FMath::Abs(DistAlongNormal) < ContactThreshold && DistPerpendicular < SweetSpotRadius + BallRadius)
    {
        Result.bContact = true;
        Result.ContactPoint = BallPos;
        Result.ContactNormal = -SweetSpotNormal; // Ball to bat
        Result.PenetrationDepth = ContactThreshold - FMath::Abs(DistAlongNormal);
        Result.RelativeSpeed = (BallVel - BatVelocity).Size();
        Result.BallVelocityBefore = BallVel;
        Result.BatVelocityAtContact = BatVelocity;

        // Check sweet spot
        Result.bSweetSpot = DistPerpendicular < SweetSpotRadius * 0.5f;

        // Check edge (far from sweet spot but still hit)
        Result.bEdge = DistPerpendicular > SweetSpotRadius * 0.8f && DistPerpendicular < SweetSpotRadius + BallRadius;

        // Contact angle
        Result.ContactAngle = CalculateContactAngle(BallVel, SweetSpotNormal);
    }

    return Result;
}

FVector UCricketBattingComponent::CalculateSweetSpotWorldLocation(const FTransform& BatTransform) const
{
    // Sweet spot is typically 15-20cm from toe, center of bat face
    FVector LocalSweetSpot = FVector(BatPhysicsState.BatLength * 100.0f * 0.7f, 0.0f, 0.0f); // 70% down from handle
    return BatTransform.TransformPosition(LocalSweetSpot);
}

FVector UCricketBattingComponent::CalculateSweetSpotNormal(const FTransform& BatTransform) const
{
    // Bat face normal (pointing towards off side for right-handed batter)
    FVector LocalNormal = FVector(0.0f, 1.0f, 0.0f); // Y-axis = bat face
    return BatTransform.TransformVector(LocalNormal).GetSafeNormal();
}

float UCricketBattingComponent::CalculateBatSpeedAtContact(const FVector& BatVelocity, const FVector& ContactPoint) const
{
    // Bat speed at contact point (accounting for rotation)
    if (BatPhysicsState.AngularVelocity.IsNearlyZero())
    {
        return BatVelocity.Size();
    }

    // Add tangential velocity from rotation
    FVector SweetSpot = CalculateSweetSpotWorldLocation(BatPhysicsState.BatTransform);
    FVector RadiusVec = ContactPoint - SweetSpot;
    FVector TangentialVel = FVector::CrossProduct(BatPhysicsState.AngularVelocity, RadiusVec);

    return (BatVelocity + TangentialVel).Size();
}

float UCricketBattingComponent::CalculateContactAngle(const FVector& BallVelocity, const FVector& BatNormal) const
{
    if (BallVelocity.IsNearlyZero())
    {
        return 0.0f;
    }

    FVector BallDir = BallVelocity.GetSafeNormal();
    float Dot = FVector::DotProduct(BallDir, BatNormal);
    float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

    // 0 = perpendicular, 90 = parallel (edge)
    return FMath::Abs(90.0f - Angle);
}

int32 UCricketBattingComponent::PredictRuns(const FShotResult& Result) const
{
    if (Result.bIsWicket)
    {
        return 0;
    }

    // Base runs from power and placement
    float RunPotential = Result.ShotPower * 0.5f + Result.PlacementScore * 0.5f;

    // Shot type modifiers
    const FShotDefinition* ShotDef = GetShotDefinition(Result.ShotType);
    if (ShotDef)
    {
        // Lofted shots = boundary or wicket
        if (ShotDef->LoftProbability > 0.5f)
        {
            if (Result.ShotPower > 1.2f && Result.PlacementScore > 0.7f)
            {
                return 6; // Six
            }
            else if (Result.ShotPower > 0.8f)
            {
                return 4; // Four
            }
            else
            {
                return FMath::RandBool() ? 2 : 1; // Caught or runs
            }
        }

        // Ground shots
        if (Result.ShotPower > 1.0f && Result.PlacementScore > 0.8f)
        {
            return 4; // Boundary
        }
        else if (Result.ShotPower > 0.6f && Result.PlacementScore > 0.6f)
        {
            return 3;
        }
        else if (Result.ShotPower > 0.4f || Result.PlacementScore > 0.5f)
        {
            return FMath::RandRange(1, 2);
        }
        else
        {
            return 1; // Single
        }
    }

    return FMath::Clamp(FMath::RoundToInt(RunPotential * 4.0f), 0, 6);
}

bool UCricketBattingComponent::CheckWicket(const FShotResult& Result, FGameplayTag& OutWicketType) const
{
    // Wicket probability based on shot quality
    float WicketChance = 0.0f;

    // Edges to slips/keeper
    if (Result.ContactResult.bEdge)
    {
        WicketChance += 0.4f;
        if (Result.Timing == ETimingWindow::Late || Result.Timing == ETimingWindow::VeryLate)
        {
            WicketChance += 0.2f; // Late on edge = caught behind
        }
        OutWicketType = FGameplayTag::RequestGameplayTag(FName("Wicket.Caught"));
        return FMath::FRand() < WicketChance;
    }

    // LBW - missed straight balls
    if (!Result.ContactResult.bContact && Result.ShotType != EBattingShotType::None)
    {
        // Check if ball would hit stumps
        // Simplified: if playing defensive/forward and ball in line
        if (Result.ShotType == EBattingShotType::Defensive || Result.ShotType == EBattingShotType::ForwardDefensive)
        {
            WicketChance = 0.15f * (2.0f - Attributes.Technique); // Better technique = less LBW
            OutWicketType = FGameplayTag::RequestGameplayTag(FName("Wicket.LBW"));
            return FMath::FRand() < WicketChance;
        }
    }

    // Bowled - missed completely on straight delivery
    if (!Result.ContactResult.bContact && Result.Timing == ETimingWindow::Missed)
    {
        WicketChance = 0.1f * (2.0f - Attributes.Technique);
        OutWicketType = FGameplayTag::RequestGameplayTag(FName("Wicket.Bowled"));
        return FMath::FRand() < WicketChance;
    }

    // Leading edge / top edge on lofted shots
    if (Result.ShotPower > 1.0f && Result.PlacementScore < 0.3f && Result.ContactResult.bContact)
    {
        WicketChance = 0.2f;
        OutWicketType = FGameplayTag::RequestGameplayTag(FName("Wicket.Caught"));
        return FMath::FRand() < WicketChance;
    }

    return false;
}

const FShotDefinition* UCricketBattingComponent::GetShotDefinition(EBattingShotType ShotType) const
{
    for (const FShotDefinition& Def : ShotDefinitions)
    {
        if (Def.ShotType == ShotType)
        {
            return &Def;
        }
    }
    return nullptr;
}

void UCricketBattingComponent::ApplyAttributesToShot(FShotResult& Result) const
{
    // Power attribute affects boundary probability
    Result.ShotPower *= Attributes.Power;

    // Technique affects control
    Result.ShotControl *= Attributes.Technique;

    // Timing attribute widens timing windows (handled in DetermineTimingWindow)

    // Aggression affects loft probability
    if (const FShotDefinition* ShotDef = GetShotDefinition(Result.ShotType))
    {
        float LoftMod = 1.0f + (Attributes.Aggression - 1.0f) * 0.3f;
        // Not directly used but would affect shot selection AI
    }

    // Temperament - under pressure (not modeled here but would reduce performance)
    // Footwork - affects reach (would modify contact detection)
}