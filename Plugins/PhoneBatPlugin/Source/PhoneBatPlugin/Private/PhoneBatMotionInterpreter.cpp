#include "PhoneBatMotionInterpreter.h"
#include "PhoneBatSensorFusion.h"
#include "Math/UnrealMathUtility.h"

UPhoneBatMotionInterpreter::UPhoneBatMotionInterpreter()
{
    CurrentPhase = ESwingPhase::Idle;
    PhaseProgress = 0.0f;
    CurrentBatSpeed = 0.0f;
    PeakBatSpeed = 0.0f;
}

UPhoneBatMotionInterpreter& UPhoneBatMotionInterpreter::Get()
{
    static UPhoneBatMotionInterpreter* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<UPhoneBatMotionInterpreter>();
        Instance->AddToRoot();
    }
    return *Instance;
}

void UPhoneBatMotionInterpreter::Initialize()
{
    CurrentPhase = ESwingPhase::Idle;
    PhaseProgress = 0.0f;
    CurrentBatSpeed = 0.0f;
    PeakBatSpeed = 0.0f;
    SpeedHistory.Empty();
    AccelerationHistory.Empty();
    OrientationHistory.Empty();
    TimestampHistory.Empty();
    AngularVelocityHistory.Empty();
    UE_LOG(LogTemp, Log, TEXT("PhoneBatMotionInterpreter initialized"));
}

void UPhoneBatMotionInterpreter::Shutdown()
{
    CurrentPhase = ESwingPhase::Idle;
    UE_LOG(LogTemp, Log, TEXT("PhoneBatMotionInterpreter shutdown"));
}

void UPhoneBatMotionInterpreter::ProcessSensorFrame(const FPhoneBatSensorFramePayload& Payload)
{
    // Update local fused state
    UPhoneBatSensorFusion::Get().ProcessSensorFrame(Payload);

    // Estimate current speed
    CurrentBatSpeed = EstimateBatSpeed(Payload);
    PeakBatSpeed = FMath::Max(PeakBatSpeed, CurrentBatSpeed);

    // Maintain history
    SpeedHistory.Add(CurrentBatSpeed);
    FVector Accel(Payload.Accelerometer.X, Payload.Accelerometer.Y, Payload.Accelerometer.Z);
    AccelerationHistory.Add(Accel.Size());
    OrientationHistory.Add(FQuat(Payload.Orientation.X, Payload.Orientation.Y, Payload.Orientation.Z, Payload.Orientation.W));
    TimestampHistory.Add(Payload.Timestamp);
    AngularVelocityHistory.Add(FVector(Payload.Gyroscope.X, Payload.Gyroscope.Y, Payload.Gyroscope.Z));

    if (SpeedHistory.Num() > HistoryWindowSize)
    {
        SpeedHistory.RemoveAt(0);
        AccelerationHistory.RemoveAt(0);
        OrientationHistory.RemoveAt(0);
        TimestampHistory.RemoveAt(0);
        AngularVelocityHistory.RemoveAt(0);
    }

    // Run phase detection
    DetectPhase(Payload);

    // Update phase progress
    UpdatePhaseProgress(Payload);

    // Run contact prediction if in downswing
    if (CurrentPhase == ESwingPhase::Downswing)
    {
        PredictContact(Payload);
    }
}

void UPhoneBatMotionInterpreter::ProcessSwingPhase(const FPhoneBatSwingPhasePayload& Payload)
{
    ESwingPhase NewPhase = static_cast<ESwingPhase>(Payload.CurrentPhase);

    if (NewPhase != CurrentPhase)
    {
        HandlePhaseTransition(CurrentPhase, NewPhase);
        CurrentPhase = NewPhase;
        OnPhaseChanged.Broadcast(CurrentPhase);
    }

    PhaseProgress = Payload.PhaseProgress;
    CurrentBatSpeed = Payload.BatSpeed / 100.0f;
    PeakBatSpeed = FMath::Max(PeakBatSpeed, Payload.BatSpeed / 100.0f);
}

void UPhoneBatMotionInterpreter::ProcessContactPrediction(const FPhoneBatContactPredictPayload& Payload)
{
    ContactPrediction.TimeToContact = static_cast<float>((Payload.PredictedContactTime - Payload.Timestamp) / 1000000.0f);
    ContactPrediction.PredictedContactPoint = Payload.PredictedContactPoint * 100.0f;
    ContactPrediction.PredictedBatVelocity = Payload.PredictedBatVelocity * 100.0f;
    ContactPrediction.PredictedBatOrientation = FQuat(Payload.PredictedBatOrientation.X, Payload.PredictedBatOrientation.Y,
                                                      Payload.PredictedBatOrientation.Z, Payload.PredictedBatOrientation.W);
    ContactPrediction.Confidence = Payload.Confidence;

    OnContactPredicted.Broadcast(ContactPrediction);
}

void UPhoneBatMotionInterpreter::ProcessSwingResult(const FPhoneBatSwingResultPayload& Result)
{
    CurrentSwingMetrics.MaxBatSpeed = Result.PowerRating * 30.0f;
    CurrentSwingMetrics.ContactTime = static_cast<float>(Result.ContactTimestamp / 1000000.0f);
    CurrentSwingMetrics.BatAngleAtContact = Result.WristRotationAtContact * 0.5f;
    CurrentSwingMetrics.WristRotationAtContact = Result.WristRotationAtContact;

    LastShotClassification.ShotType = Result.ShotType;
    LastShotClassification.TimingQuality = Result.TimingQuality;
    LastShotClassification.PowerRating = Result.PowerRating;
    LastShotClassification.ControlRating = Result.ControlRating;
    LastShotClassification.Confidence = 1.0f;

    OnSwingCompleted.Broadcast(CurrentSwingMetrics);
    OnShotClassified.Broadcast(LastShotClassification);

    HandlePhaseTransition(CurrentPhase, ESwingPhase::Recovery);
    CurrentPhase = ESwingPhase::Recovery;
}

void UPhoneBatMotionInterpreter::DetectPhase(const FPhoneBatSensorFramePayload& Payload)
{
    ESwingPhase NewPhase = CurrentPhase;

    float AccelMag = AccelerationHistory.Num() > 0 ? AccelerationHistory.Last() : 0.0f;

    switch (CurrentPhase)
    {
        case ESwingPhase::Idle:
            if (CurrentBatSpeed > SwingDetectionThreshold && AccelMag > 10.0f)
            {
                NewPhase = ESwingPhase::Backswing;
            }
            break;

        case ESwingPhase::Backswing:
            if (CurrentBatSpeed > PeakBatSpeed * BackswingToDownswingThreshold && AccelMag < 5.0f)
            {
                NewPhase = ESwingPhase::Downswing;
            }
            else if (CurrentBatSpeed < 0.5f)
            {
                NewPhase = ESwingPhase::Idle;
            }
            break;

        case ESwingPhase::Downswing:
            if (AccelMag > ContactAccelerationThreshold && CurrentBatSpeed > PeakBatSpeed * 0.5f)
            {
                NewPhase = ESwingPhase::Contact;
            }
            else if (CurrentBatSpeed < MinDownswingSpeed)
            {
                NewPhase = ESwingPhase::Recovery;
            }
            break;

        case ESwingPhase::Contact:
            NewPhase = ESwingPhase::FollowThrough;
            break;

        case ESwingPhase::FollowThrough:
            if (CurrentBatSpeed < 2.0f)
            {
                NewPhase = ESwingPhase::Recovery;
            }
            break;

        case ESwingPhase::Recovery:
            if (CurrentBatSpeed < 0.5f)
            {
                NewPhase = ESwingPhase::Idle;
                PeakBatSpeed = 0.0f;
            }
            break;

        default:
            break;
    }

    if (NewPhase != CurrentPhase)
    {
        HandlePhaseTransition(CurrentPhase, NewPhase);
        CurrentPhase = NewPhase;
        OnPhaseChanged.Broadcast(CurrentPhase);
    }
}

void UPhoneBatMotionInterpreter::UpdatePhaseProgress(const FPhoneBatSensorFramePayload& Payload)
{
    int64 NowUs = Payload.Timestamp;

    switch (CurrentPhase)
    {
        case ESwingPhase::Backswing:
            PhaseProgress = FMath::Clamp(static_cast<float>(NowUs - SwingStartTimeUs) / 1000000.0f, 0.0f, 1.0f);
            break;
        case ESwingPhase::Downswing:
            PhaseProgress = FMath::Clamp(static_cast<float>(NowUs - BackswingEndTimeUs) / 300000.0f, 0.0f, 1.0f);
            break;
        case ESwingPhase::Contact:
            PhaseProgress = 1.0f;
            break;
        case ESwingPhase::FollowThrough:
            PhaseProgress = FMath::Clamp(static_cast<float>(NowUs - ContactTimeUs) / 500000.0f, 0.0f, 1.0f);
            break;
        default:
            PhaseProgress = 0.0f;
            break;
    }
}

void UPhoneBatMotionInterpreter::HandlePhaseTransition(ESwingPhase OldPhase, ESwingPhase NewPhase)
{
    int64 NowUs = FDateTime::UtcNow().ToUnixTimestamp() * 1000000LL;

    switch (NewPhase)
    {
        case ESwingPhase::Backswing:
            OnBackswingStart();
            SwingStartTimeUs = NowUs;
            PeakBatSpeed = 0.0f;
            break;
        case ESwingPhase::Transition:
            OnTransition();
            break;
        case ESwingPhase::Downswing:
            OnDownswingStart();
            BackswingEndTimeUs = NowUs;
            break;
        case ESwingPhase::Contact:
            OnContact();
            ContactTimeUs = NowUs;
            break;
        case ESwingPhase::FollowThrough:
            OnFollowThrough();
            break;
        case ESwingPhase::Recovery:
            OnRecovery();
            break;
        case ESwingPhase::Idle:
            Initialize();
            break;
    }
}

void UPhoneBatMotionInterpreter::OnBackswingStart() {}
void UPhoneBatMotionInterpreter::OnTransition() {}
void UPhoneBatMotionInterpreter::OnDownswingStart() {}
void UPhoneBatMotionInterpreter::OnContact()
{
    PredictContactFromHistory();
    LastShotClassification = ClassifyShot();
    OnShotClassified.Broadcast(LastShotClassification);
}
void UPhoneBatMotionInterpreter::OnFollowThrough() {}
void UPhoneBatMotionInterpreter::OnRecovery()
{
    OnSwingCompleted.Broadcast(CurrentSwingMetrics);
}

void UPhoneBatMotionInterpreter::PredictContact(const FPhoneBatSensorFramePayload& Payload)
{
    if (SpeedHistory.Num() < 5) return;

    float SpeedSlope = (SpeedHistory.Last() - SpeedHistory[SpeedHistory.Num() - 5]) / 5.0f;

    if (SpeedSlope > 0)
    {
        float TimeToPeak = (PeakBatSpeed - CurrentBatSpeed) / SpeedSlope;
        if (TimeToPeak > 0 && TimeToPeak < 0.5f)
        {
            ContactPrediction.TimeToContact = TimeToPeak;
            ContactPrediction.Confidence = 0.5f;
            OnContactPredicted.Broadcast(ContactPrediction);
        }
    }
}

void UPhoneBatMotionInterpreter::PredictContactFromHistory()
{
    CurrentSwingMetrics.ContactTime = FDateTime::UtcNow().ToUnixTimestamp();
    CurrentSwingMetrics.MaxBatSpeed = PeakBatSpeed;
    CurrentSwingMetrics.BatAngleAtContact = CalculateBatAngle();
    CurrentSwingMetrics.WristRotationAtContact = CalculateWristRotation();
    CurrentSwingMetrics.SwingPlaneAngle = CalculateSwingPlane();
    CurrentSwingMetrics.Tempo = CalculateTempo();
}

FShotClassification UPhoneBatMotionInterpreter::ClassifyShot()
{
    FShotClassification Shot;
    float Angle = CalculateBatAngle();
    float Plane = CalculateSwingPlane();

    if (Angle > 30.0f) Shot.ShotType = TEXT("Cover Drive");
    else if (Angle < -30.0f) Shot.ShotType = TEXT("On Drive");
    else if (Plane > 20.0f) Shot.ShotType = TEXT("Pull Shot");
    else if (Plane < -20.0f) Shot.ShotType = TEXT("Cut Shot");
    else Shot.ShotType = TEXT("Straight Drive");

    Shot.Confidence = 0.8f;
    Shot.PowerRating = FMath::Clamp(PeakBatSpeed / 30.0f, 0.0f, 1.0f);
    Shot.ControlRating = 1.0f - (FMath::Abs(Angle) / 90.0f);

    return Shot;
}

float UPhoneBatMotionInterpreter::EstimateBatSpeed(const FPhoneBatSensorFramePayload& Payload)
{
    if (TimestampHistory.Num() < 1) return 0.0f;

    float dt = (Payload.Timestamp - TimestampHistory.Last()) / 1000000.0f;
    if (dt <= 0) dt = 0.01f;

    FVector Accel(Payload.LinearAcceleration.X, Payload.LinearAcceleration.Y, Payload.LinearAcceleration.Z);
    float AccelAlongBat = -Accel.Z;

    float NewSpeed = CurrentBatSpeed + AccelAlongBat * dt;
    return FMath::Max(0.0f, NewSpeed);
}

float UPhoneBatMotionInterpreter::CalculateBatAngle() const
{
    if (OrientationHistory.Num() == 0) return 0.0f;
    FQuat Q = OrientationHistory.Last();
    FVector Forward = Q.RotateVector(FVector(0, 0, -1));
    FVector HorizontalForward = FVector(Forward.X, Forward.Y, 0).GetSafeNormal();
    return FMath::Atan2(HorizontalForward.Y, HorizontalForward.X) * 180.0f / PI;
}

float UPhoneBatMotionInterpreter::CalculateWristRotation() const
{
    if (AngularVelocityHistory.Num() == 0) return 0.0f;
    return AngularVelocityHistory.Last().Size() * 180.0f / PI;
}

float UPhoneBatMotionInterpreter::CalculateSwingPlane() const
{
    if (AccelerationHistory.Num() == 0) return 0.0f;
    return 0.0f;
}

float UPhoneBatMotionInterpreter::CalculateTempo() const
{
    if (BackswingEndTimeUs > SwingStartTimeUs && ContactTimeUs > BackswingEndTimeUs)
    {
        float BackswingDur = (BackswingEndTimeUs - SwingStartTimeUs) / 1000000.0f;
        float DownswingDur = (ContactTimeUs - BackswingEndTimeUs) / 1000000.0f;
        return BackswingDur / DownswingDur;
    }
    return 0.0f;
}