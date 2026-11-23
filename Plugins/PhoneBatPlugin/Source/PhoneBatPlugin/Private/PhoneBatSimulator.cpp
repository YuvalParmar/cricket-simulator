#include "PhoneBatSimulator.h"
#include "PhoneBatNetworkManager.h"
#include "PhoneBatSensorFusion.h"
#include "PhoneBatCalibrationManager.h"
#include "PhoneBatMotionInterpreter.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Math/UnrealMathUtility.h"

UPhoneBatSimulator::UPhoneBatSimulator()
    : BatState()
    , bIsSimulating(false)
    , bCalibrationApplied(false)
    , BallRadius(0.036f)
    , NetworkManager(nullptr)
    , SensorFusion(nullptr)
    , CalibrationManager(nullptr)
    , MotionInterpreter(nullptr)
{
    // Initialize bat state
    BatState.BatTransform = FTransform::Identity;
    BatState.LinearVelocity = FVector::ZeroVector;
    BatState.AngularVelocity = FVector::ZeroVector;
    BatState.BatMass = 1.2f;
    BatState.BatLength = 0.965f;
    BatState.BatWidth = 0.108f;
    BatState.SweetSpotRadius = 0.04f;
    StateHistory.Empty();
    TargetBall.Reset();
}

UPhoneBatSimulator& UPhoneBatSimulator::Get()
{
    static UPhoneBatSimulator* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<UPhoneBatSimulator>();
        Instance->AddToRoot();
    }
    return *Instance;
}

void UPhoneBatSimulator::Initialize()
{
    NetworkManager = &UPhoneBatNetworkManager::Get();
    SensorFusion = &UPhoneBatSensorFusion::Get();
    CalibrationManager = &UPhoneBatCalibrationManager::Get();
    MotionInterpreter = &UPhoneBatMotionInterpreter::Get();

    // Bind network callbacks
    NetworkManager->OnSensorFrameReceived.AddUObject(this, &UPhoneBatSimulator::OnSensorFrameReceived);
    NetworkManager->OnSwingPhaseReceived.AddUObject(this, &UPhoneBatSimulator::OnSwingPhaseReceived);
    NetworkManager->OnContactPredictionReceived.AddUObject(this, &UPhoneBatSimulator::OnContactPredictionReceived);
    NetworkManager->OnSwingResultReceived.AddUObject(this, &UPhoneBatSimulator::OnSwingResultReceived);
    NetworkManager->OnCalibrationReceived.AddUObject(this, &UPhoneBatSimulator::OnCalibrationReceived);
    NetworkManager->OnClockSyncUpdate.AddUObject(this, &UPhoneBatSimulator::OnClockSyncUpdate);

    bIsSimulating = true;
    StateHistory.Empty();
    PreviousBallPosition = FVector::ZeroVector;
    PreviousBallVelocity = FVector::ZeroVector;

    UE_LOG(LogTemp, Log, TEXT("PhoneBatSimulator initialized"));
}

void UPhoneBatSimulator::Shutdown()
{
    if (NetworkManager)
    {
        NetworkManager->OnSensorFrameReceived.RemoveAll(this);
        NetworkManager->OnSwingPhaseReceived.RemoveAll(this);
        NetworkManager->OnContactPredictionReceived.RemoveAll(this);
        NetworkManager->OnSwingResultReceived.RemoveAll(this);
        NetworkManager->OnCalibrationReceived.RemoveAll(this);
        NetworkManager->OnClockSyncUpdate.RemoveAll(this);
    }

    bIsSimulating = false;
    TargetBall.Reset();
    StateHistory.Empty();

    UE_LOG(LogTemp, Log, TEXT("PhoneBatSimulator shutdown"));
}

void UPhoneBatSimulator::Update(float InDeltaTime)
{
    DeltaTime = InDeltaTime;
    if (!bIsSimulating) return;

    // Update from phone data
    UpdateFromPhoneData();

    // Update sweet spot
    BatState.SweetSpotLocation = CalculateSweetSpotLocation();
    BatState.SweetSpotNormal = CalculateSweetSpotNormal();

    // Check for ball contact
    if (TargetBall.IsValid())
    {
        FBattedBallResult Result;
        if (bEnableSubFramePrediction)
        {
            Result = DetectBallContactSubFrame(DeltaTime);
        }
        else
        {
            Result = DetectBallContact(DeltaTime);
        }

        if (Result.bContact)
        {
            Result.ContactDetectionTimeUs = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL;
            OnBattedBall.Broadcast(Result);
        }
    }

    // Update history
    StateHistory.Add(BatState);
    if (StateHistory.Num() > MAX_HISTORY_SIZE)
    {
        StateHistory.RemoveAt(0);
    }

    // Broadcast state update
    OnBatStateUpdated.Broadcast(BatState);
}

void UPhoneBatSimulator::SetTargetBall(AActor* BallActor, float InBallRadius)
{
    TargetBall = BallActor;
    BallRadius = InBallRadius;

    if (TargetBall.IsValid())
    {
        BallPosition = TargetBall->GetActorLocation();
        BallVelocity = FVector::ZeroVector;
        FVector::ZeroVector;
        PreviousBallPosition = BallPosition;
        PreviousBallVelocity = BallVelocity;
    }
}

void UPhoneBatSimulator::ApplyCalibration(const FPhoneBatCalibrationResult& Calibration)
{
    if (Calibration.bIsValid)
    {
        bCalibrationApplied = true;
        UE_LOG(LogTemp, Log, TEXT("PhoneBatSimulator: Calibration applied, quality: %.2f"), Calibration.QualityScore);
    }
}

void UPhoneBatSimulator::SetBatState(const FBState& NewState)
{
    BatState = NewState;
    OnBatStateUpdated.Broadcast(BatState);
}

void UPhoneBatSimulator::UpdateFromPhoneData()
{
    if (!NetworkManager) return;

    // Get latest sensor fusion state
    if (SensorFusion && SensorFusion->GetCurrentState().bIsInitialized)
    {
        const FPhoneBatSensorFusion::FSensorFusionState& FusionState = SensorFusion->GetCurrentState();

        // Apply calibration transform if available
        if (bCalibrationApplied && CalibrationManager)
        {
            const FPhoneBatCalibrationResult& Cal = CalibrationManager->GetResult();
            if (Cal.bIsValid)
            {
                // Transform orientation from phone space to bat space
                FQuat BatOrientation = Cal.PhoneToBatTransform.GetRotation() * FusionState.Orientation;
                BatState.BatTransform.SetRotation(BatOrientation);

                // Position: grip position + bat offset along handle
                FVector GripPos = Cal.PhoneToBatTransform.GetLocation();
                FVector BatDirection = BatOrientation.RotateVector(FVector(0.0f, -1.0f, 0.0f)); // Bat extends along -Y
                BatState.BatTransform.SetLocation(GripPos + BatDirection * Cal.SweetSpotDistance);
            }
            else
            {
                BatState.BatTransform.SetRotation(FusionState.Orientation);
            }
        }
        else
        {
            BatState.BatTransform.SetRotation(FusionState.Orientation);
        }

        BatState.LinearVelocity = FusionState.LinearAcceleration;
        BatState.AngularVelocity = FusionState.AngularVelocity;
        BatState.EstimatedLatencyMs = NetworkManager->GetConnectionStats().LatencyMS;
        BatState.LastPacketTimeUs = NetworkManager->GetConnectionStats().LastPacketTimeUs;
    }
}

FVector UPhoneBatSimulator::CalculateSweetSpotLocation() const
{
    FVector BatDirection = BatState.BatTransform.GetRotation().RotateVector(FVector(0.0f, -1.0f, 0.0f));
    return BatState.BatTransform.GetLocation() + BatDirection * BatState.SweetSpotRadius;
}

FVector UPhoneBatSimulator::CalculateSweetSpotNormal() const
{
    // Normal points along bat face (perpendicular to bat length)
    return BatState.BatTransform.GetRotation().RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FVector UPhoneBatSimulator::CalculateBatVelocityAtPoint(const FVector& Point) const
{
    // v = v_cm + ω × (r - r_cm)
    FVector CM = BatState.BatTransform.GetLocation();
    FVector R = Point - CM;
    return BatState.LinearVelocity + FVector::CrossProduct(BatState.AngularVelocity, R);
}

FBattedBallResult UPhoneBatSimulator::DetectBallContact(float /*DeltaTime*/)
{
    FBattedBallResult Result;

    if (!TargetBall.IsValid()) return Result;

    // Get current ball state
    FVector CurrentBallPos = TargetBall->GetActorLocation();
    FVector CurrentBallVel = (CurrentBallPos - PreviousBallPosition) / DeltaTime;

    // Check collision between bat and ball
    FVector SweetSpot = CalculateSweetSpotLocation();
    FVector BatToBall = CurrentBallPos - SweetSpot;
    float Distance = BatToBall.Size();

    if (Distance <= (BatState.SweetSpotRadius + BallRadius))
    {
        Result.bContact = true;
        Result.ContactPoint = SweetSpot + BatToBall.GetSafeNormal() * BatState.SweetSpotRadius;
        Result.BallVelocityBefore = CurrentBallVel;
        Result.BatVelocityAtContact = CalculateBatVelocityAtPoint(Result.ContactPoint);
        Result.BallVelocityAfter = CurrentBallVel;

        Result.ContactAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Abs(FVector::DotProduct(
            CurrentBallVel.GetSafeNormal(),
            CalculateSweetSpotNormal()
        ))));

        Result.bSweetSpot = Distance <= BatState.SweetSpotRadius;
        Result.bEdge = !Result.bSweetSpot && Distance <= (BatState.SweetSpotRadius * 2.0f);

        ApplyBallPhysics(Result);
    }

    PreviousBallPosition = CurrentBallPos;
    PreviousBallVelocity = CurrentBallVel;

    return Result;
}

FBattedBallResult UPhoneBatSimulator::DetectBallContactSubFrame(float /*DeltaTime*/)
{
    FBattedBallResult Result;

    if (!TargetBall.IsValid()) return Result;

    FVector CurrentBallPos = TargetBall->GetActorLocation();
    FVector CurrentBallVel = (CurrentBallPos - PreviousBallPosition) / DeltaTime;

    FVector PrevSweetSpot = PreviousBallPosition;
    FVector CurrSweetSpot = CalculateSweetSpotLocation();

    FVector BallTrajectory = CurrentBallPos - PreviousBallPosition;
    FVector BatTrajectory = CurrSweetSpot - PrevSweetSpot;

    float BallSpeed = CurrentBallVel.Size();
    if (BallSpeed < 0.1f)
    {
        return DetectBallContact(DeltaTime);
    }

    FVector BatVel = BatState.LinearVelocity;
    FVector RelPos = PreviousBallPosition - PrevSweetSpot;
    FVector RelVel = CurrentBallVel - BatVel;

    float R = BatState.SweetSpotRadius + BallRadius;
    float A = FVector::DotProduct(RelVel, RelVel);
    float B = 2.0f * FVector::DotProduct(RelPos, RelVel);
    float C = FVector::DotProduct(RelPos, RelPos) - R * R;

    float Discriminant = B * B - 4.0f * A * C;
    if (Discriminant < 0.0f)
    {
        return Result;
    }

    float SqrtD = FMath::Sqrt(Discriminant);
    float t1 = (-B - SqrtD) / (2.0f * A);
    float t2 = (-B + SqrtD) / (2.0f * A);

    float ContactT = -1.0f;
    if (t1 >= 0.0f && t1 <= 1.0f) ContactT = t1;
    else if (t2 >= 0.0f && t2 <= 1.0f) ContactT = t2;

    if (ContactT < 0.0f) return Result;

    Result.bContact = true;
    Result.SubFrameContactTime = ContactT;

    FVector ContactBallPos = PreviousBallPosition + CurrentBallVel * ContactT;
    FVector ContactSweetSpot = PrevSweetSpot + BatState.LinearVelocity * ContactT;

    Result.SubFrameContactPoint = ContactSweetSpot;
    Result.SubFrameBallVelocity = CurrentBallVel;
    Result.SubFrameBatVelocity = BatState.LinearVelocity;

    FVector BallToBat = ContactBallPos - ContactSweetSpot;
    Result.ContactPoint = ContactSweetSpot + BallToBat.GetSafeNormal() * BatState.SweetSpotRadius;

    Result.BallVelocityBefore = CurrentBallVel;
    Result.BatVelocityAtContact = CalculateBatVelocityAtPoint(Result.ContactPoint);

    Result.ContactAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Abs(FVector::DotProduct(
        CurrentBallVel.GetSafeNormal(),
        CalculateSweetSpotNormal()
    ))));

    Result.bSweetSpot = BallToBat.Size() <= BatState.SweetSpotRadius;
    Result.bEdge = !Result.bSweetSpot && BallToBat.Size() <= (BatState.SweetSpotRadius * 2.0f);

    ApplyBallPhysics(Result);

    PreviousBallPosition = CurrentBallPos;
    PreviousBallVelocity = CurrentBallVel;

    return Result;
}

void UPhoneBatSimulator::ApplyBallPhysics(FBattedBallResult& Result)
{
    if (!Result.bContact) return;

    FVector Normal = CalculateSweetSpotNormal();
    FVector RelVel = Result.BallVelocityBefore - Result.BatVelocityAtContact;
    float SpeedAlongNormal = FVector::DotProduct(RelVel, Normal);

    if (SpeedAlongNormal >= 0) return;

    // Coefficient of restitution
    float COR = CoefficientOfRestitution;
    if (Result.bSweetSpot)
    {
        COR += SweetSpotCORBoost;
    }
    else if (Result.bEdge)
    {
        COR -= EdgeCORPenalty;
    }
    COR = FMath::Clamp(COR, 0.0f, 1.0f);

    // Impulse calculation
    float ImpulseMagnitude = -(1.0f + COR) * SpeedAlongNormal;

    // Apply velocity change
    Result.BallVelocityAfter = Result.BallVelocityBefore - Normal * (2.0f * SpeedAlongNormal + ImpulseMagnitude);

    // Calculate ratings
    Result.TimingQuality = FMath::Clamp(1.0f - FMath::Abs(Result.ContactAngle) / 45.0f, 0.0f, 1.0f);
    Result.PowerRating = FMath::Clamp(Result.BatVelocityAtContact.Size() / 30.0f, 0.0f, 1.0f);
    Result.ControlRating = Result.bSweetSpot ? 1.0f : (Result.bEdge ? 0.5f : 0.2f);

    // Predict runs based on shot
    float BallSpeed = Result.BallVelocityAfter.Size();
    if (BallSpeed > 30.0f) Result.PredictedRuns = 6;
    else if (BallSpeed > 25.0f) Result.PredictedRuns = 4;
    else if (BallSpeed > 15.0f) Result.PredictedRuns = 2;
    else if (BallSpeed > 5.0f) Result.PredictedRuns = 1;
    else Result.PredictedRuns = 0;

    // Get shot type from motion interpreter
    if (MotionInterpreter)
    {
        Result.ShotType = MotionInterpreter->GetLastShotClassification().ShotType;
    }
}

void UPhoneBatSimulator::OnSensorFrameReceived(const FPhoneBatSensorFramePayload& /*Frame*/)
{
    // Handled in UpdateFromPhoneData
}

void UPhoneBatSimulator::OnSwingPhaseReceived(const FPhoneBatSwingPhasePayload& /*Phase*/)
{
    // Could trigger visual/audio cues
}

void UPhoneBatSimulator::OnContactPredictionReceived(const FPhoneBatContactPredictPayload& /*Prediction*/)
{
    // Could use for early preparation
}

void UPhoneBatSimulator::OnSwingResultReceived(const FPhoneBatSwingResultPayload& /*Result*/)
{
    // Swing complete
}

void UPhoneBatSimulator::OnCalibrationReceived(const FPhoneBatCalibrationPayload& Calibration)
{
    if (Calibration.bComplete && Calibration.bIsValid)
    {
        ApplyCalibration(FPhoneBatCalibrationResult());
    }
}

void UPhoneBatSimulator::OnClockSyncUpdate(int64 /*ClockOffsetUs*/, int32 /*RTTUs*/)
{
    // Latency info available in NetworkManager stats
}