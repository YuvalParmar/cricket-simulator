#include "PhoneBatInput.h"
#include "PhoneBatReceiverComponent.h"
#include "CricketBattingPawn.h"
#include "CricketBowlingPawn.h"
#include "CricketFieldingPawn.h"
#include "PhoneBatNetworkProtocol.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

UPhoneBatInput::UPhoneBatInput()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics; // Early tick for input
}

void UPhoneBatInput::InitializeComponent()
{
    Super::InitializeComponent();

    // Create receiver component
    ReceiverComponent = NewObject<UPhoneBatReceiverComponent>(this, TEXT("PhoneBatReceiver"));
    if (ReceiverComponent)
    {
        ReceiverComponent->RegisterComponent();

        // Bind callbacks
        ReceiverComponent->OnSensorFrameReceived.AddDynamic(this, &UPhoneBatInput::OnSensorFrameReceived);
        ReceiverComponent->OnSwingPhaseReceived.AddDynamic(this, &UPhoneBatInput::OnSwingPhaseReceived);
        ReceiverComponent->OnContactPredictionReceived.AddDynamic(this, &UPhoneBatInput::OnContactPredictionReceived);
        ReceiverComponent->OnSwingResultReceived.AddDynamic(this, &UPhoneBatInput::OnSwingResultReceived);
        ReceiverComponent->OnCalibrationDataReceived.AddDynamic(this, &UPhoneBatInput::OnCalibrationDataReceived);
        ReceiverComponent->OnConnectionStateChanged.AddDynamic(this, &UPhoneBatInput::OnConnectionStateChanged);
        ReceiverComponent->OnClockSyncUpdate.AddDynamic(this, &UPhoneBatInput::OnClockSyncUpdate);
    }

    CurrentState.ConnectionState = EPhoneBatConnectionState::Disconnected;
}

void UPhoneBatInput::BeginPlay()
{
    Super::BeginPlay();

    // Auto-start discovery if configured
    // StartDiscovery();
}

void UPhoneBatInput::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateBattingState(DeltaTime);
    UpdateConnectionQuality();
}

void UPhoneBatInput::SetBattingPawn(ACricketBattingPawn* Pawn)
{
    BattingPawn = Pawn;
}

void UPhoneBatInput::SetBowlingPawn(ACricketBowlingPawn* Pawn)
{
    BowlingPawn = Pawn;
}

void UPhoneBatInput::SetFieldingPawn(ACricketFieldingPawn* Pawn)
{
    FieldingPawn = Pawn;
}

FBattingInputState UPhoneBatInput::GetBattingInputState() const
{
    FBattingInputState State;

    if (!IsInputActive() || !CurrentState.bIsCalibrated)
    {
        return State;
    }

    // Bat angle from phone orientation
    FQuat BatRotation = CurrentState.BatTransform.GetRotation();
    FVector BatForward = BatRotation.GetForwardVector();
    FVector BatRight = BatRotation.GetRightVector();
    FVector BatUp = BatRotation.GetUpVector();

    // Face angle: rotation around vertical axis (Yaw)
    State.BatFaceAngle = FMath::Atan2(BatForward.Y, BatForward.X);

    // Wrist rotation: roll around bat shaft
    State.WristRotation = FMath::Atan2(BatRight.Z, BatUp.Z);

    // Bat path direction from velocity
    if (CurrentState.BatVelocity.Size() > 10.0f)
    {
        State.BatPathDirection = CurrentState.BatVelocity.GetSafeNormal2D();
    }

    // Shot type and footwork from selection
    State.SelectedShotType = CurrentState.SelectedShotType;
    State.SelectedFootwork = CurrentState.SelectedFootwork;
    State.PowerModifier = CurrentState.PowerModifier;

    // Timing trigger from swing phase
    State.bTimingTriggerPressed = (CurrentState.SwingPhase == EPhoneBatSwingPhase::Downswing ||
                                    CurrentState.SwingPhase == EPhoneBatSwingPhase::Contact);

    // Timing progress for visual feedback
    State.TimingProgress = CurrentState.SwingProgress;

    // Predicted contact info for early preparation
    State.bContactPredicted = CurrentState.bContactPredicted;
    State.PredictedContactTime = CurrentState.PredictedContactTime;
    State.PredictedContactPoint = CurrentState.PredictedContactPoint;
    State.ContactConfidence = CurrentState.ContactConfidence;

    return State;
}

FBowlingInputState UPhoneBatInput::GetBowlingInputState() const
{
    // Phone not used for bowling in current design
    return FBowlingInputState();
}

FFieldingInputState UPhoneBatInput::GetFieldingInputState() const
{
    // Phone not used for fielding in current design
    return FFieldingInputState();
}

bool UPhoneBatInput::IsInputActive() const
{
    return CurrentState.ConnectionState == EPhoneBatConnectionState::Active &&
           CurrentState.bIsCalibrated;
}

FString UPhoneBatInput::GetInputDeviceName() const
{
    return TEXT("Phone Bat (AR)");
}

void UPhoneBatInput::StartDiscovery(int32 Port)
{
    if (ReceiverComponent)
    {
        CurrentState.ConnectionState = EPhoneBatConnectionState::Discovering;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
        ReceiverComponent->StartListening(Port);
    }
}

void UPhoneBatInput::StopDiscovery()
{
    if (ReceiverComponent)
    {
        ReceiverComponent->StopListening();
        CurrentState.ConnectionState = EPhoneBatConnectionState::Disconnected;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
    }
}

void UPhoneBatInput::StartPairing(const FString& PairingCode)
{
    if (ReceiverComponent && CurrentState.ConnectionState == EPhoneBatConnectionState::Discovering)
    {
        CurrentState.ConnectionState = EPhoneBatConnectionState::Pairing;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
        ReceiverComponent->SendPairingRequest(PairingCode);
    }
}

void UPhoneBatInput::CancelPairing()
{
    if (ReceiverComponent)
    {
        ReceiverComponent->SendDisconnect();
        CurrentState.ConnectionState = EPhoneBatConnectionState::Disconnected;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
    }
}

void UPhoneBatInput::StartCalibration()
{
    if (ReceiverComponent && CurrentState.ConnectionState == EPhoneBatConnectionState::Connected)
    {
        CurrentState.ConnectionState = EPhoneBatConnectionState::Calibrating;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
        ReceiverComponent->StartCalibration(CalibrationSteps);
    }
}

bool UPhoneBatInput::IsCalibrated_Implementation() const
{
    return CurrentState.bIsCalibrated;
}

bool UPhoneBatInput::IsConnected_Implementation() const
{
    return CurrentState.ConnectionState == EPhoneBatConnectionState::Active;
}

void UPhoneBatInput::CompleteCalibrationStep(int32 StepIndex, const FTransform& PhoneToBatTransform)
{
    if (ReceiverComponent && CurrentState.ConnectionState == EPhoneBatConnectionState::Calibrating)
    {
        ReceiverComponent->SendCalibrationStep(StepIndex, PhoneToBatTransform);

        if (StepIndex >= CalibrationSteps - 1)
        {
            // Last step - wait for confirmation from phone
        }
        else
        {
            OnCalibrationProgress.Broadcast(StepIndex + 1);
        }
    }
}

void UPhoneBatInput::SetShotSelection(FGameplayTag ShotType, FGameplayTag Footwork, float PowerModifier)
{
    CurrentState.SelectedShotType = ShotType;
    CurrentState.SelectedFootwork = Footwork;
    CurrentState.PowerModifier = FMath::Clamp(PowerModifier, 0.5f, 2.0f);
}

void UPhoneBatInput::OnSensorFrameReceived(const FPhoneBatSensorFramePayload& Frame)
{
    ProcessSensorFrame(Frame);
}

void UPhoneBatInput::OnSwingPhaseReceived(const FPhoneBatSwingPhasePayload& Phase)
{
    LastSwingPhase = Phase;
    CurrentState.SwingPhase = Phase.CurrentPhase;
    CurrentState.SwingProgress = Phase.PhaseProgress;
    CurrentState.BatSpeed = Phase.BatSpeed;
    CurrentState.MaxBatSpeedThisSwing = Phase.MaxBatSpeed;
    CurrentState.BatVelocity = Phase.BatVelocity;
    CurrentState.BatAngularVelocity = Phase.BatAngularVelocity;

    // Detect swing start
    if (!bSwingStarted && (Phase.CurrentPhase == EPhoneBatSwingPhase::Backswing ||
                           Phase.CurrentPhase == EPhoneBatSwingPhase::Transition))
    {
        StartNewSwing();
    }

    // Detect swing end
    if (bSwingStarted && Phase.CurrentPhase == EPhoneBatSwingPhase::Recovery)
    {
        EndCurrentSwing();
    }

    // Broadcast swing phase change
    OnSwingDetected.Broadcast(Phase.CurrentPhase);
}

void UPhoneBatInput::OnContactPredictionReceived(const FPhoneBatContactPredictPayload& Prediction)
{
    // Convert phone timestamp to server timestamp using clock offset
    uint64 ServerContactTime = Prediction.PredictedContactTime + ClockOffsetUs;

    CurrentState.bContactPredicted = true;
    CurrentState.PredictedContactTime = ServerContactTime;
    CurrentState.PredictedContactPoint = Prediction.PredictedContactPoint;
    CurrentState.PredictedBatVelocityAtContact = Prediction.PredictedBatVelocity;
    CurrentState.PredictedBatOrientationAtContact = Prediction.PredictedBatOrientation;
    CurrentState.ContactConfidence = Prediction.Confidence;

    OnContactPredicted.Broadcast(Prediction);
}

void UPhoneBatInput::OnSwingResultReceived(const FPhoneBatSwingResultPayload& Result)
{
    // Swing completed - result contains full analysis
    // This can be used for post-shot feedback, replay, etc.

    EndCurrentSwing();

    // Reset contact prediction
    CurrentState.bContactPredicted = false;
}

void UPhoneBatInput::OnCalibrationDataReceived(const FPhoneBatCalibrationPayload& Calibration)
{
    if (Calibration.bComplete)
    {
        CurrentState.bIsCalibrated = true;
        CurrentState.CalibrationTransform = Calibration.PhoneToBatTransform;
        CurrentState.ConnectionState = EPhoneBatConnectionState::Active;
        OnConnectionChanged.Broadcast(CurrentState.ConnectionState);
        OnCalibrationComplete.Broadcast(true);
    }
    else
    {
        OnCalibrationProgress.Broadcast(Calibration.Step);
    }
}

void UPhoneBatInput::OnConnectionStateChanged(EPhoneBatConnectionState NewState)
{
    CurrentState.ConnectionState = NewState;
    OnConnectionChanged.Broadcast(NewState);

    if (NewState == EPhoneBatConnectionState::Disconnected ||
        NewState == EPhoneBatConnectionState::Error)
    {
        CurrentState.bIsCalibrated = false;
        bSwingStarted = false;
    }
}

void UPhoneBatInput::OnClockSyncUpdate(int32 ClockOffsetUs, int32 RTTUs)
{
    ClockOffsetUs = ClockOffsetUs;
    CurrentState.LatencyMs = RTTUs / 2000.0f; // Convert microseconds to ms (half RTT)
}

void UPhoneBatInput::UpdateBattingState(float DeltaTime)
{
    if (!IsInputActive() || !bSwingStarted)
    {
        return;
    }

    // Apply latency compensation: extrapolate bat pose forward
    if (bUsePredictiveExtrapolation && RecentFrames.Num() > 0)
    {
        ApplyLatencyCompensation();
    }

    // Update bat transform from latest sensor data
    if (RecentFrames.Num() > 0)
    {
        const FPhoneBatSensorFramePayload& LatestFrame = RecentFrames.Last();
        CurrentState.BatTransform = CalculateWorldBatTransform(
            FTransform(LatestFrame.Orientation, LatestFrame.Gravity * 100.0f) // Position from gravity reference
        );
    }
}

void UPhoneBatInput::ProcessSensorFrame(const FPhoneBatSensorFramePayload& Frame)
{
    // Add to recent frames buffer
    RecentFrames.Add(Frame);

    // Keep buffer size limited (e.g., last 60 frames = 1 second at 60Hz)
    while (RecentFrames.Num() > 60)
    {
        RecentFrames.RemoveAt(0);
    }

    // Add to current swing recording
    if (bSwingStarted)
    {
        CurrentSwingFrames.Add(Frame);
    }

    // Update swing detection using phone's ML results
    // The phone sends swing phase separately, but we can also do local detection as fallback
    UpdateSwingDetection();
}

void UPhoneBatInput::UpdateSwingDetection()
{
    // Local fallback swing detection using accelerometer magnitude
    if (RecentFrames.Num() < 10)
    {
        return;
    }

    // Calculate acceleration magnitude
    float MaxAccel = 0.0f;
    for (int32 i = RecentFrames.Num() - 10; i < RecentFrames.Num(); i++)
    {
        float AccelMag = RecentFrames[i].LinearAcceleration.Size();
        MaxAccel = FMath::Max(MaxAccel, AccelMag);
    }

    // Detect swing start (high acceleration)
    if (!bSwingStarted && MaxAccel > 15.0f) // ~1.5g threshold
    {
        StartNewSwing();
    }

    // Detect contact (sharp deceleration spike)
    if (bSwingStarted && RecentFrames.Num() >= 3)
    {
        int32 LastIdx = RecentFrames.Num() - 1;
        float AccelNow = RecentFrames[LastIdx].LinearAcceleration.Size();
        float AccelPrev = RecentFrames[LastIdx - 1].LinearAcceleration.Size();
        float AccelPrev2 = RecentFrames[LastIdx - 2].LinearAcceleration.Size();

        // Deceleration spike indicates contact
        if (AccelPrev > 20.0f && AccelNow < AccelPrev * 0.3f)
        {
            // Contact detected locally
            PredictContact();
        }
    }
}

void UPhoneBatInput::PredictContact()
{
    if (RecentFrames.Num() < 5)
    {
        return;
    }

    // Simple linear extrapolation for contact prediction
    // In practice, the phone's ML model does this better
    const FPhoneBatSensorFramePayload& Latest = RecentFrames.Last();

    // Estimate time to contact based on bat velocity and distance to crease
    float BatSpeed = Latest.LinearAcceleration.Size(); // Rough approximation
    float DistanceToCrease = 200.0f; // cm - would come from game state

    float TimeToContact = (BatSpeed > 100.0f) ? DistanceToCrease / BatSpeed : 0.5f;
    TimeToContact = FMath::Clamp(TimeToContact, 0.05f, 0.5f); // 50-500ms

    uint64 CurrentTime = FDateTime::UtcNow().GetTicks() / 10; // Microseconds
    uint64 PredictedTime = CurrentTime + static_cast<uint64>(TimeToContact * 1000000.0f);

    // Predict contact point (simplified - would use proper bat trajectory)
    FVector PredictedPoint = Latest.Gravity * 100.0f + Latest.LinearAcceleration.GetSafeNormal() * DistanceToCrease;

    CurrentState.bContactPredicted = true;
    CurrentState.PredictedContactTime = PredictedTime;
    CurrentState.PredictedContactPoint = PredictedPoint;
    CurrentState.PredictedBatVelocityAtContact = Latest.LinearAcceleration;
    CurrentState.PredictedBatOrientationAtContact = Latest.Orientation;
    CurrentState.ContactConfidence = 0.6f; // Lower confidence for local prediction

    FPhoneBatContactPredictPayload Prediction;
    Prediction.Timestamp = CurrentTime;
    Prediction.PredictedContactTime = PredictedTime;
    Prediction.PredictedContactPoint = PredictedPoint;
    Prediction.PredictedBatVelocity = Latest.LinearAcceleration;
    Prediction.PredictedBatOrientation = Latest.Orientation;
    Prediction.Confidence = CurrentState.ContactConfidence;

    OnContactPredicted.Broadcast(Prediction);
}

void UPhoneBatInput::ApplyLatencyCompensation()
{
    // Extrapolate bat pose forward by estimated latency
    float ExtrapolationTime = ExtrapolationTimeMs / 1000.0f; // Convert to seconds

    if (RecentFrames.Num() >= 2)
    {
        const FPhoneBatSensorFramePayload& Current = RecentFrames.Last();
        const FPhoneBatSensorFramePayload& Previous = RecentFrames[RecentFrames.Num() - 2];

        // Calculate angular velocity from orientation change
        FQuat DeltaRotation = Current.Orientation * Previous.Orientation.Inverse();
        FVector Axis;
        float Angle;
        DeltaRotation.ToAxisAndAngle(Axis, Angle);
        FVector AngularVelocity = Axis * (Angle / (ExtrapolationTimeMs / 1000.0f));

        // Extrapolate orientation
        FQuat ExtrapolatedOrientation = FQuat(AngularVelocity * ExtrapolationTime) * Current.Orientation;

        // Extrapolate position using linear velocity
        FVector LinearVelocity = (Current.Gravity - Previous.Gravity) / (ExtrapolationTimeMs / 1000.0f) * 100.0f;
        FVector ExtrapolatedPosition = Current.Gravity * 100.0f + LinearVelocity * ExtrapolationTime;

        // Update bat transform with extrapolated values
        CurrentState.BatTransform = FTransform(ExtrapolatedOrientation, ExtrapolatedPosition);
    }
}

FTransform UPhoneBatInput::CalculateWorldBatTransform(const FTransform& PhoneTransform) const
{
    if (!CurrentState.bIsCalibrated)
    {
        return PhoneTransform;
    }

    // Apply calibration transform: Phone -> Bat
    FTransform BatTransform = CurrentState.CalibrationTransform * PhoneTransform;

    // Apply coordinate space conversion if needed (phone space -> world space)
    // This depends on how the phone is held and AR session orientation

    return BatTransform;
}

void UPhoneBatInput::StartNewSwing()
{
    bSwingStarted = true;
    SwingStartTime = FDateTime::UtcNow().GetTicks() / 10;
    CurrentSwingFrames.Empty();
    CurrentState.MaxBatSpeedThisSwing = 0.0f;
    CurrentState.bContactPredicted = false;
}

void UPhoneBatInput::EndCurrentSwing()
{
    bSwingStarted = false;
    CurrentState.SwingPhase = EPhoneBatSwingPhase::Idle;
    CurrentState.SwingProgress = 0.0f;
    CurrentState.BatSpeed = 0.0f;
    CurrentState.BatVelocity = FVector::ZeroVector;
    CurrentState.BatAngularVelocity = FVector::ZeroVector;
}

void UPhoneBatInput::UpdateConnectionQuality()
{
    if (ReceiverComponent)
    {
        CurrentState.LatencyMs = ReceiverComponent->GetAverageLatencyMs();
        CurrentState.JitterMs = ReceiverComponent->GetJitterMs();
        CurrentState.PacketLoss = ReceiverComponent->GetPacketLossRate();
    }
}

// Static variable for clock offset
int32 UPhoneBatInput::ClockOffsetUs = 0;