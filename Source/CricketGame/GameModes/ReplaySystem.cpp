#include "ReplaySystem.h"
#include "CricketBall.h"
#include "CricketGameMode.h"
#include "CricketGameState.h"
#include "PitchActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

UReplaySystem::UReplaySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics; // After physics for accurate positions
}

void UReplaySystem::BeginPlay()
{
    Super::BeginPlay();

    // Find ball and pitch actors
    TArray<AActor*> BallActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACricketBall::StaticClass(), BallActors);
    if (BallActors.Num() > 0)
    {
        TrackedBall = Cast<ACricketBall>(BallActors[0]);
    }

    TArray<AActor*> PitchActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APitchActor::StaticClass(), PitchActors);
    if (PitchActors.Num() > 0)
    {
        PitchActor = Cast<APitchActor>(PitchActors[0]);
    }

    // Get camera manager
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC)
    {
        CameraManager = PC->PlayerCameraManager;
    }

    SetupDefaultCameraPositions();

    // Auto-start recording if configured
    if (bAutoRecord)
    {
        StartRecording(0);
    }
}

void UReplaySystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsRecording)
    {
        UpdateRecording(DeltaTime);
    }
    else if (PlaybackState != EReplayPlaybackState::Stopped)
    {
        UpdatePlayback(DeltaTime);
        UpdateCamera(DeltaTime);
    }
}

void UReplaySystem::StartRecording(int32 MatchId)
{
    if (bIsRecording)
    {
        UE_LOG(LogTemp, Warning, TEXT("Already recording"));
        return;
    }

    CurrentSession = FReplaySession();
    CurrentSession.SessionId = FGuid::NewGuid().ToString();
    CurrentSession.RecordedAt = FDateTime::UtcNow();
    CurrentSession.MatchId = MatchId;
    CurrentSession.FrameRate = RecordFrameRate;

    RecordingStartTime = GetWorld()->GetTimeSeconds();
    LastFrameRecordTime = 0.0f;
    bIsRecording = true;

    UE_LOG(LogTemp, Log, TEXT("Replay recording started: %s"), *CurrentSession.SessionId);
}

void UReplaySystem::StopRecording()
{
    if (!bIsRecording)
    {
        return;
    }

    bIsRecording = false;
    CurrentSession.TotalDuration = GetWorld()->GetTimeSeconds() - RecordingStartTime;
    CurrentSession.TotalFrames = CurrentSession.Frames.Num();

    UE_LOG(LogTemp, Log, TEXT("Replay recording stopped. Duration: %.2fs, Frames: %d"),
        CurrentSession.TotalDuration, CurrentSession.TotalFrames);
}

void UReplaySystem::PauseRecording()
{
    bIsRecording = false;
}

void UReplaySystem::ResumeRecording()
{
    if (!bIsRecording && CurrentSession.Frames.Num() > 0)
    {
        bIsRecording = true;
        LastFrameRecordTime = GetWorld()->GetTimeSeconds() - RecordingStartTime;
    }
}

void UReplaySystem::RecordEvent(const FReplayEvent& Event)
{
    if (!bIsRecording)
    {
        return;
    }

    CurrentSession.KeyEvents.Add(Event);

    // Also add to current frame if recording
    if (CurrentSession.Frames.Num() > 0)
    {
        CurrentSession.Frames.Last().EventsAtThisFrame.Add(Event);
    }
}

void UReplaySystem::RecordFrame()
{
    if (!bIsRecording)
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds() - RecordingStartTime;

    // Check if enough time has passed for a keyframe
    if (CurrentTime - LastFrameRecordTime < KeyframeInterval)
    {
        return;
    }

    // Check frame limit
    if (CurrentSession.Frames.Num() >= MaxFrames)
    {
        UE_LOG(LogTemp, Warning, TEXT("Max frames reached, stopping recording"));
        StopRecording();
        return;
    }

    FReplayFrame Frame = CaptureCurrentFrame();
    Frame.Timestamp = CurrentTime;
    Frame.FrameNumber = CurrentSession.Frames.Num();

    CurrentSession.Frames.Add(Frame);
    LastFrameRecordTime = CurrentTime;
}

void UReplaySystem::UpdateRecording(float DeltaTime)
{
    RecordFrame();
}

void UReplaySystem::LoadReplay(const FReplaySession& Session)
{
    StopRecording();

    CurrentSession = Session;
    CurrentPlaybackTime = 0.0f;
    CurrentFrameIndex = 0;
    PlaybackState = EReplayPlaybackState::Stopped;
    PlaybackSpeed = 1.0f;

    // Apply first frame
    if (CurrentSession.Frames.Num() > 0)
    {
        ApplyFrame(CurrentSession.Frames[0]);
    }

    BroadcastStateChange(EReplayPlaybackState::Stopped);
    UE_LOG(LogTemp, Log, TEXT("Replay loaded: %s (%.2fs, %d frames)"),
        *CurrentSession.SessionId, CurrentSession.TotalDuration, CurrentSession.TotalFrames);
}

void UReplaySystem::LoadReplayFromFile(const FString& FilePath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load replay file: %s"), *FilePath);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse replay JSON: %s"), *FilePath);
        return;
    }

    // Deserialize session (simplified - would need full deserialization)
    FReplaySession Session;
    Session.SessionId = JsonObject->GetStringField(TEXT("SessionId"));
    Session.TotalDuration = JsonObject->GetNumberField(TEXT("TotalDuration"));
    Session.TotalFrames = JsonObject->GetIntegerField(TEXT("TotalFrames"));
    Session.FrameRate = JsonObject->GetNumberField(TEXT("FrameRate"));
    Session.MatchInfo = JsonObject->GetStringField(TEXT("MatchInfo"));

    // Would deserialize frames and events here
    // For now, just load basic info
    LoadReplay(Session);
}

void UReplaySystem::Play()
{
    if (CurrentSession.Frames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No replay loaded"));
        return;
    }

    PlaybackState = EReplayPlaybackState::Playing;
    BroadcastStateChange(PlaybackState);
}

void UReplaySystem::Pause()
{
    if (PlaybackState == EReplayPlaybackState::Playing)
    {
        PlaybackState = EReplayPlaybackState::Paused;
        BroadcastStateChange(PlaybackState);
    }
}

void UReplaySystem::Stop()
{
    PlaybackState = EReplayPlaybackState::Stopped;
    CurrentPlaybackTime = 0.0f;
    CurrentFrameIndex = 0;

    if (CurrentSession.Frames.Num() > 0)
    {
        ApplyFrame(CurrentSession.Frames[0]);
    }

    BroadcastStateChange(PlaybackState);
    OnTimeChanged.Broadcast(CurrentPlaybackTime, CurrentSession.TotalDuration);
}

void UReplaySystem::SetPlaybackSpeed(float Speed)
{
    PlaybackSpeed = FMath::Clamp(Speed, -4.0f, 4.0f);
}

void UReplaySystem::SeekToTime(float Time)
{
    Time = FMath::Clamp(Time, 0.0f, CurrentSession.TotalDuration);
    CurrentPlaybackTime = Time;

    // Find frame index
    CurrentFrameIndex = 0;
    for (int32 i = 0; i < CurrentSession.Frames.Num(); i++)
    {
        if (CurrentSession.Frames[i].Timestamp <= Time)
        {
            CurrentFrameIndex = i;
        }
        else
        {
            break;
        }
    }

    // Apply interpolated frame
    if (CurrentFrameIndex < CurrentSession.Frames.Num() - 1)
    {
        const FReplayFrame& FrameA = CurrentSession.Frames[CurrentFrameIndex];
        const FReplayFrame& FrameB = CurrentSession.Frames[CurrentFrameIndex + 1];

        float FrameDuration = FrameB.Timestamp - FrameA.Timestamp;
        if (FrameDuration > 0.0f)
        {
            float Alpha = (Time - FrameA.Timestamp) / FrameDuration;
            InterpolateFrame(CurrentFrameIndex, Alpha);
        }
        else
        {
            ApplyFrame(FrameA);
        }
    }
    else if (CurrentFrameIndex < CurrentSession.Frames.Num())
    {
        ApplyFrame(CurrentSession.Frames[CurrentFrameIndex]);
    }

    OnTimeChanged.Broadcast(CurrentPlaybackTime, CurrentSession.TotalDuration);
}

void UReplaySystem::StepFrame(int32 Direction)
{
    if (CurrentSession.Frames.Num() == 0)
    {
        return;
    }

    PlaybackState = EReplayPlaybackState::FrameStep;

    int32 NewIndex = CurrentFrameIndex + Direction;
    NewIndex = FMath::Clamp(NewIndex, 0, CurrentSession.Frames.Num() - 1);

    if (NewIndex != CurrentFrameIndex)
    {
        CurrentFrameIndex = NewIndex;
        CurrentPlaybackTime = CurrentSession.Frames[CurrentFrameIndex].Timestamp;
        ApplyFrame(CurrentSession.Frames[CurrentFrameIndex]);
        OnTimeChanged.Broadcast(CurrentPlaybackTime, CurrentSession.TotalDuration);
    }

    BroadcastStateChange(PlaybackState);
}

void UReplaySystem::JumpToEvent(int32 EventIndex)
{
    if (EventIndex < 0 || EventIndex >= CurrentSession.KeyEvents.Num())
    {
        return;
    }

    const FReplayEvent& Event = CurrentSession.KeyEvents[EventIndex];
    SeekToTime(Event.Timestamp - 1.0f); // Start 1 second before event
    Play();
}

void UReplaySystem::SetCameraMode(EReplayCameraMode Mode)
{
    if (CurrentCameraMode != Mode)
    {
        CurrentCameraMode = Mode;
        CameraTransitionTime = 0.0f;
        BroadcastCameraChange(Mode);
    }
}

void UReplaySystem::SetCameraTarget(AActor* Target)
{
    CameraTarget = Target;
}

void UReplaySystem::AddCameraPosition(const FTransform& Transform, float Duration)
{
    CameraPositions.Add(Transform);
    // Duration could be used for timed camera sequences
}

FHawkeyeData UReplaySystem::GenerateHawkeyeData(const FReplayEvent& BallEvent) const
{
    return SimulateHawkeye(BallEvent);
}

FWagonWheelData UReplaySystem::GenerateWagonWheelData(int32 Innings) const
{
    FWagonWheelData Data;

    // Collect all scoring shots from events
    for (const FReplayEvent& Event : CurrentSession.KeyEvents)
    {
        if (Event.Innings != Innings)
        {
            continue;
        }

        // Check if event is a scoring shot
        if (Event.EventType == EReplayEventType::RunsScored ||
            Event.EventType == EReplayEventType::Boundary4 ||
            Event.EventType == EReplayEventType::Boundary6)
        {
            // Extract shot direction from metadata
            FString DirectionStr;
            if (Event.Metadata.Contains(TEXT("ShotDirection")))
            {
                DirectionStr = Event.Metadata[TEXT("ShotDirection")];
                // Parse "X,Y" format
                TArray<FString> Parts;
                DirectionStr.ParseIntoArray(Parts, TEXT(","), true);
                if (Parts.Num() == 2)
                {
                    FVector2D Direction(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
                    Data.ShotDirections.Add(Direction);
                }
            }

            // Runs scored
            int32 Runs = 0;
            if (Event.Metadata.Contains(TEXT("Runs")))
            {
                Runs = FCString::Atoi(*Event.Metadata[TEXT("Runs")]);
            }
            Data.RunsScored.Add(Runs);

            // Shot type
            FString ShotType = Event.Metadata.Contains(TEXT("ShotType")) ? Event.Metadata[TEXT("ShotType")] : TEXT("Unknown");
            Data.ShotTypes.Add(ShotType);

            // Color coding
            FLinearColor Color = FLinearColor::White;
            if (Runs == 6) Color = FLinearColor::Red;
            else if (Runs == 4) Color = FLinearColor::Yellow;
            else if (Runs >= 2) Color = FLinearColor::Green;
            else if (Runs == 1) Color = FLinearColor::Blue;
            Data.ShotColors.Add(Color);

            // Distribution
            Data.RunsDistribution.FindOrAdd(Runs)++;
            Data.ShotTypeDistribution.FindOrAdd(ShotType)++;
        }
    }

    return Data;
}

FPitchMapData UReplaySystem::GeneratePitchMapData(int32 Innings) const
{
    FPitchMapData Data;

    for (const FReplayEvent& Event : CurrentSession.KeyEvents)
    {
        if (Event.Innings != Innings)
        {
            continue;
        }

        // Ball bounce events
        if (Event.EventType == EReplayEventType::BallBounced)
        {
            Data.BounceLocations.Add(Event.Location);
        }

        // Ball impact events (bat/pad)
        if (Event.EventType == EReplayEventType::BallHitBat ||
            Event.EventType == EReplayEventType::BallHitPad)
        {
            Data.ImpactLocations.Add(Event.Location);
        }

        // Delivery type
        int32 DeliveryType = 0;
        if (Event.Metadata.Contains(TEXT("DeliveryType")))
        {
            DeliveryType = FCString::Atoi(*Event.Metadata[TEXT("DeliveryType")]);
        }
        Data.DeliveryTypes.Add(DeliveryType);

        // Swing amount
        float Swing = 0.0f;
        if (Event.Metadata.Contains(TEXT("Swing")))
        {
            Swing = FCString::Atof(*Event.Metadata[TEXT("Swing")]);
        }
        Data.SwingAmounts.Add(Swing);

        // Spin amount
        float Spin = 0.0f;
        if (Event.Metadata.Contains(TEXT("Spin")))
        {
            Spin = FCString::Atof(*Event.Metadata[TEXT("Spin")]);
        }
        Data.SpinAmounts.Add(Spin);

        // Color by outcome
        FLinearColor Color = FLinearColor::White;
        if (Event.EventType == EReplayEventType::WicketFallen) Color = FLinearColor::Red;
        else if (Event.EventType == EReplayEventType::RunsScored) Color = FLinearColor::Green;
        else if (Event.EventType == EReplayEventType::Boundary4 || Event.EventType == EReplayEventType::Boundary6) Color = FLinearColor::Yellow;
        else if (Event.EventType == EReplayEventType::BallHitPad) Color = FLinearColor::Orange;
        Data.BallColors.Add(Color);
    }

    return Data;
}

bool UReplaySystem::SaveReplayToFile(const FString& FilePath)
{
    // Serialize session to JSON
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

    JsonObject->SetStringField(TEXT("SessionId"), CurrentSession.SessionId);
    JsonObject->SetStringField(TEXT("RecordedAt"), CurrentSession.RecordedAt.ToIso8601());
    JsonObject->SetNumberField(TEXT("MatchId"), CurrentSession.MatchId);
    JsonObject->SetNumberField(TEXT("TotalDuration"), CurrentSession.TotalDuration);
    JsonObject->SetNumberField(TEXT("TotalFrames"), CurrentSession.TotalFrames);
    JsonObject->SetNumberField(TEXT("FrameRate"), CurrentSession.FrameRate);
    JsonObject->SetStringField(TEXT("MatchInfo"), CurrentSession.MatchInfo);

    // Serialize key events
    TArray<TSharedPtr<FJsonValue>> EventsArray;
    for (const FReplayEvent& Event : CurrentSession.KeyEvents)
    {
        TSharedPtr<FJsonObject> EventObj = MakeShareable(new FJsonObject());
        EventObj->SetNumberField(TEXT("EventType"), static_cast<int32>(Event.EventType));
        EventObj->SetNumberField(TEXT("Timestamp"), Event.Timestamp);
        EventObj->SetNumberField(TEXT("Duration"), Event.Duration);
        EventObj->SetStringField(TEXT("Description"), Event.Description);
        EventObj->SetStringField(TEXT("Location"), Event.Location.ToString());
        EventObj->SetNumberField(TEXT("BallNumber"), Event.BallNumber);
        EventObj->SetNumberField(TEXT("OverNumber"), Event.OverNumber);
        EventObj->SetNumberField(TEXT("Innings"), Event.Innings);

        // Metadata
        TSharedPtr<FJsonObject> MetaObj = MakeShareable(new FJsonObject());
        for (const auto& Pair : Event.Metadata)
        {
            MetaObj->SetStringField(Pair.Key, Pair.Value);
        }
        EventObj->SetObjectField(TEXT("Metadata"), MetaObj);

        // Involved players
        TArray<TSharedPtr<FJsonValue>> PlayersArray;
        for (const FString& Player : Event.InvolvedPlayers)
        {
            PlayersArray.Add(MakeShareable(new FJsonValueString(Player)));
        }
        EventObj->SetArrayField(TEXT("InvolvedPlayers"), PlayersArray);

        EventsArray.Add(MakeShareable(new FJsonValueObject(EventObj)));
    }
    JsonObject->SetArrayField(TEXT("KeyEvents"), EventsArray);

    // Note: Frames are not saved in JSON due to size - would use binary format

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath));
    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

void UReplaySystem::ExportHighlightReel(const TArray<int32>& EventIndices, const FString& OutputPath)
{
    // Create a new session with only the selected events
    FReplaySession HighlightSession = CurrentSession;
    HighlightSession.SessionId = FGuid::NewGuid().ToString() + TEXT("_highlight");
    HighlightSession.KeyEvents.Empty();
    HighlightSession.Frames.Empty();

    for (int32 Index : EventIndices)
    {
        if (Index >= 0 && Index < CurrentSession.KeyEvents.Num())
        {
            HighlightSession.KeyEvents.Add(CurrentSession.KeyEvents[Index]);
        }
    }

    // Save
    SaveReplayToFile(OutputPath);
}

void UReplaySystem::UpdatePlayback(float DeltaTime)
{
    if (PlaybackState != EReplayPlaybackState::Playing)
    {
        return;
    }

    CurrentPlaybackTime += DeltaTime * PlaybackSpeed;

    // Clamp to duration
    if (CurrentPlaybackTime >= CurrentSession.TotalDuration)
    {
        CurrentPlaybackTime = CurrentSession.TotalDuration;
        Pause();
        return;
    }
    else if (CurrentPlaybackTime <= 0.0f)
    {
        CurrentPlaybackTime = 0.0f;
        if (PlaybackSpeed < 0.0f)
        {
            Pause();
        }
        return;
    }

    // Find current frame
    while (CurrentFrameIndex < CurrentSession.Frames.Num() - 1 &&
           CurrentSession.Frames[CurrentFrameIndex + 1].Timestamp <= CurrentPlaybackTime)
    {
        CurrentFrameIndex++;
    }

    // Interpolate between frames
    if (CurrentFrameIndex < CurrentSession.Frames.Num() - 1)
    {
        const FReplayFrame& FrameA = CurrentSession.Frames[CurrentFrameIndex];
        const FReplayFrame& FrameB = CurrentSession.Frames[CurrentFrameIndex + 1];

        float FrameDuration = FrameB.Timestamp - FrameA.Timestamp;
        if (FrameDuration > 0.0f)
        {
            float Alpha = (CurrentPlaybackTime - FrameA.Timestamp) / FrameDuration;
            InterpolateFrame(CurrentFrameIndex, Alpha);
        }
    }
    else if (CurrentFrameIndex < CurrentSession.Frames.Num())
    {
        ApplyFrame(CurrentSession.Frames[CurrentFrameIndex]);
    }

    // Trigger events at this time
    TriggerEventsAtTime(CurrentPlaybackTime);

    OnTimeChanged.Broadcast(CurrentPlaybackTime, CurrentSession.TotalDuration);
}

void UReplaySystem::UpdateCamera(float DeltaTime)
{
    if (!CameraManager)
    {
        return;
    }

    FTransform CameraTransform = GetCameraTransformForMode(CurrentCameraMode);

    if (CameraTransitionTime < 1.0f)
    {
        // Smooth transition between camera modes
        CameraTransitionTime += DeltaTime * 2.0f; // 0.5s transition
        CameraTransitionTime = FMath::Clamp(CameraTransitionTime, 0.0f, 1.0f);

        // Would blend from previous camera transform
    }

    CameraManager->SetCameraLocation(CameraTransform.GetLocation());
    CameraManager->SetCameraRotation(CameraTransform.GetRotation().Rotator());
}

void UReplaySystem::InterpolateFrame(int32 FrameIndex, float Alpha)
{
    if (FrameIndex >= CurrentSession.Frames.Num() - 1)
    {
        ApplyFrame(CurrentSession.Frames[FrameIndex]);
        return;
    }

    const FReplayFrame& FrameA = CurrentSession.Frames[FrameIndex];
    const FReplayFrame& FrameB = CurrentSession.Frames[FrameIndex + 1];

    FReplayFrame InterpolatedFrame;
    InterpolatedFrame.Timestamp = FMath::Lerp(FrameA.Timestamp, FrameB.Timestamp, Alpha);

    // Interpolate ball
    InterpolatedFrame.BallTransform = FMath::Lerp(FrameA.BallTransform, FrameB.BallTransform, Alpha);
    InterpolatedFrame.BallVelocity = FMath::Lerp(FrameA.BallVelocity, FrameB.BallVelocity, Alpha);
    InterpolatedFrame.BallSpin = FMath::Lerp(FrameA.BallSpin, FrameB.BallSpin, Alpha);

    // Interpolate players
    for (const auto& Pair : FrameA.PlayerTransforms)
    {
        const FString& PlayerId = Pair.Key;
        if (FrameB.PlayerTransforms.Contains(PlayerId))
        {
            InterpolatedFrame.PlayerTransforms.Add(PlayerId,
                FMath::Lerp(Pair.Value, FrameB.PlayerTransforms[PlayerId], Alpha));
            InterpolatedFrame.PlayerAnimations.Add(PlayerId, FrameA.PlayerAnimations[PlayerId]);
        }
    }

    ApplyFrame(InterpolatedFrame);
}

void UReplaySystem::ApplyFrame(const FReplayFrame& Frame)
{
    // Apply ball transform
    if (TrackedBall)
    {
        TrackedBall->SetActorTransform(Frame.BallTransform);
        TrackedBall->SetPhysicsLinearVelocity(Frame.BallVelocity);
        TrackedBall->SetPhysicsAngularVelocityInDegrees(Frame.BallSpin);
    }

    // Apply player transforms
    // Would iterate through all player pawns and set their transforms
    // This is simplified - in practice would map PlayerID to actual pawns
}

FTransform UReplaySystem::GetCameraTransformForMode(EReplayCameraMode Mode)
{
    FTransform Result = FTransform::Identity;

    switch (Mode)
    {
        case EReplayCameraMode::Broadcast:
        {
            // Cycle through broadcast cameras or use current camera index
            if (CameraPositions.IsValidIndex(CurrentCameraIndex))
            {
                Result = CameraPositions[CurrentCameraIndex];
            }
            else
            {
                // Default broadcast position
                Result.SetLocation(FVector(-3000.0f, 0.0f, 1500.0f));
                Result.SetRotation(FRotator(-20.0f, 0.0f, 0.0f).Quaternion());
            }
            break;
        }

        case EReplayCameraMode::BatterView:
        {
            // Camera at batter's eye level
            if (TrackedBall)
            {
                // Find striker
                // For now, position behind stumps looking at bowler
                Result.SetLocation(FVector(-100.0f, 0.0f, 160.0f));
                Result.SetRotation(FRotator(-10.0f, 180.0f, 0.0f).Quaternion());
            }
            break;
        }

        case EReplayCameraMode::BowlerView:
        {
            // Camera behind bowler
            Result.SetLocation(FVector(3000.0f, 0.0f, 200.0f));
            Result.SetRotation(FRotator(-5.0f, 0.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::BallTracking:
        {
            // Follow ball with smooth camera
            if (TrackedBall)
            {
                FVector BallLoc = TrackedBall->GetActorLocation();
                Result.SetLocation(BallLoc + FVector(-500.0f, 0.0f, 300.0f));
                Result.SetRotation((BallLoc - Result.GetLocation()).Rotation().Quaternion());
            }
            break;
        }

        case EReplayCameraMode::Hawkeye:
        {
            // Hawkeye visualization camera
            Result.SetLocation(FVector(0.0f, -2000.0f, 1000.0f));
            Result.SetRotation(FRotator(-30.0f, 90.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::UltraEdge:
        {
            // Side-on view for ultra edge
            Result.SetLocation(FVector(-500.0f, -1000.0f, 100.0f));
            Result.SetRotation(FRotator(0.0f, 90.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::HotSpot:
        {
            // Thermal camera view
            Result.SetLocation(FVector(-200.0f, 0.0f, 200.0f));
            Result.SetRotation(FRotator(-15.0f, 180.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::WagonWheel:
        {
            // Top-down view
            Result.SetLocation(FVector(0.0f, 0.0f, 5000.0f));
            Result.SetRotation(FRotator(-90.0f, 0.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::PitchMap:
        {
            // Top-down pitch view
            Result.SetLocation(FVector(1000.0f, 0.0f, 3000.0f));
            Result.SetRotation(FRotator(-60.0f, 180.0f, 0.0f).Quaternion());
            break;
        }

        case EReplayCameraMode::FieldingView:
        {
            // Follow ball from fielding perspective
            if (TrackedBall)
            {
                FVector BallLoc = TrackedBall->GetActorLocation();
                Result.SetLocation(BallLoc + FVector(-200.0f, 500.0f, 200.0f));
                Result.SetRotation((BallLoc - Result.GetLocation()).Rotation().Quaternion());
            }
            break;
        }

        case EReplayCameraMode::FreeCam:
        {
            // Controlled by player input
            break;
        }
    }

    return Result;
}

void UReplaySystem::SetupDefaultCameraPositions()
{
    CameraPositions.Empty();

    // Main broadcast camera
    CameraPositions.Add(FTransform(FRotator(-20.0f, 0.0f, 0.0f).Quaternion(), FVector(-3000.0f, 0.0f, 1500.0f)));

    // Wide angle
    CameraPositions.Add(FTransform(FRotator(-15.0f, 0.0f, 0.0f).Quaternion(), FVector(-4000.0f, 0.0f, 2000.0f)));

    // Close up batter
    CameraPositions.Add(FTransform(FRotator(-10.0f, 180.0f, 0.0f).Quaternion(), FVector(-100.0f, 0.0f, 160.0f)));

    // Bowler's end
    CameraPositions.Add(FTransform(FRotator(-5.0f, 0.0f, 0.0f).Quaternion(), FVector(3000.0f, 0.0f, 200.0f)));

    // Square leg
    CameraPositions.Add(FTransform(FRotator(-15.0f, 90.0f, 0.0f).Quaternion(), FVector(0.0f, -3000.0f, 1000.0f)));

    // Point
    CameraPositions.Add(FTransform(FRotator(-15.0f, -90.0f, 0.0f).Quaternion(), FVector(0.0f, 3000.0f, 1000.0f)));

    // High behind (hawk-eye style)
    CameraPositions.Add(FTransform(FRotator(-45.0f, 0.0f, 0.0f).Quaternion(), FVector(-1000.0f, 0.0f, 3000.0f)));

    // Stump cam
    CameraPositions.Add(FTransform(FRotator(0.0f, 180.0f, 0.0f).Quaternion(), FVector(0.0f, 0.0f, 50.0f)));
}

FHawkeyeData UReplaySystem::SimulateHawkeye(const FReplayEvent& BallEvent) const
{
    FHawkeyeData Result;

    if (!TrackedBall || !PitchActor)
    {
        return Result;
    }

    // Get ball physics state at event time
    // This would come from the replay frame data
    FBallPhysicsState Physics = TrackedBall->GetPhysicsState();

    // Predict trajectory forward
    float TimeStep = 0.01f;
    int32 MaxSteps = 500;
    FVector Pos = Physics.Position;
    FVector Vel = Physics.Velocity;
    FVector Spin = Physics.AngularVelocity;

    Result.ActualPath = TrackedBall->PredictTrajectory(0.05f, 100, true);

    for (int32 i = 0; i < MaxSteps; i++)
    {
        Result.PredictedPath.Add(Pos);

        FVector Accel = FVector(0.0f, 0.0f, -980.0f);

        // Magnus
        FVector Magnus = FVector::CrossProduct(Spin, Vel) * 1.2e-4f;
        Accel += Magnus / 0.156f;

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

        // Bounce
        if (Pos.Z <= 0.0f && Vel.Z < 0.0f)
        {
            if (Result.PredictedBouncePoint.IsNearlyZero())
            {
                Result.PredictedBouncePoint = Pos;
            }

            Pos.Z = 0.0f;
            Vel.Z = -Vel.Z * 0.6f;
            Vel.X *= 0.9f;
            Vel.Y *= 0.9f;

            float SpinDev = FVector::CrossProduct(Spin, FVector::UpVector).Size() * 0.3f;
            Vel.Y += SpinDev * 100.0f;
        }

        // Check stumps
        if (Pos.X > 2000.0f)
        {
            Result.PredictedStumpImpact = Pos;
            break;
        }
    }

    // Find pitch point
    for (int32 i = 1; i < Result.PredictedPath.Num(); i++)
    {
        if (Result.PredictedPath[i-1].Z > 10.0f && Result.PredictedPath[i].Z <= 10.0f)
        {
            Result.PredictedBouncePoint = Result.PredictedPath[i];
            break;
        }
    }

    // LBW analysis
    Result.bPitchesInLine = FMath::Abs(Result.PredictedBouncePoint.Y) < 0.7f;
    Result.bImpactInLine = FMath::Abs(Physics.Position.Y) < 0.3f;
    Result.bHitsStumps = FMath::Abs(Result.PredictedStumpImpact.Y) < 0.2f &&
                         Result.PredictedStumpImpact.Z < 0.7f;
    Result.StumpImpactHeight = Result.PredictedStumpImpact.Z;
    Result.StumpImpactOffset = Result.PredictedStumpImpact.Y;

    // Deviation analysis
    Result.SwingDeviation = 0.0f; // Would calculate from seam position
    Result.SeamDeviation = FMath::FRandRange(-5.0f, 5.0f);
    Result.SpinDeviation = FMath::FRandRange(-10.0f, 10.0f);

    Result.Confidence = 0.9f;

    return Result;
}

void UReplaySystem::BroadcastStateChange(EReplayPlaybackState NewState)
{
    PlaybackState = NewState;
    OnPlaybackStateChanged.Broadcast(NewState);
}

void UReplaySystem::BroadcastCameraChange(EReplayCameraMode NewMode)
{
    CurrentCameraMode = NewMode;
    OnCameraModeChanged.Broadcast(NewMode);
}

void UReplaySystem::TriggerEventsAtTime(float Time)
{
    for (const FReplayEvent& Event : CurrentSession.KeyEvents)
    {
        if (FMath::Abs(Event.Timestamp - Time) < 0.1f) // Within 100ms
        {
            OnEventTriggered.Broadcast(Event);
        }
    }
}

FReplayFrame UReplaySystem::CaptureCurrentFrame()
{
    FReplayFrame Frame;

    if (TrackedBall)
    {
        Frame.BallTransform = TrackedBall->GetActorTransform();
        Frame.BallVelocity = TrackedBall->GetVelocity();
        Frame.BallSpin = TrackedBall->GetPhysicsAngularVelocityInDegrees();
    }

    // Capture all player transforms
    if (bRecordAllPlayers)
    {
        // Would iterate through all player pawns
        // For now, placeholder
    }

    return Frame;
}