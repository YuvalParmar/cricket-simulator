#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBall.h"
#include "ReplaySystem.generated.h"

class ACricketBall;
class APitchActor;
class APlayerCameraManager;

UENUM(BlueprintType)
enum class EReplayCameraMode : uint8
{
    Broadcast       UMETA(DisplayName = "Broadcast"),
    BatterView      UMETA(DisplayName = "Batter View"),
    BowlerView      UMETA(DisplayName = "Bowler View"),
    BallTracking    UMETA(DisplayName = "Ball Tracking"),
    Hawkeye         UMETA(DisplayName = "Hawkeye"),
    UltraEdge       UMETA(DisplayName = "UltraEdge"),
    HotSpot         UMETA(DisplayName = "HotSpot"),
    WagonWheel      UMETA(DisplayName = "Wagon Wheel"),
    PitchMap        UMETA(DisplayName = "Pitch Map"),
    FieldingView    UMETA(DisplayName = "Fielding View"),
    FreeCam         UMETA(DisplayName = "Free Cam")
};

UENUM(BlueprintType)
enum class EReplayPlaybackState : uint8
{
    Stopped         UMETA(DisplayName = "Stopped"),
    Playing         UMETA(DisplayName = "Playing"),
    Paused          UMETA(DisplayName = "Paused"),
    Scrubbing       UMETA(DisplayName = "Scrubbing"),
    FrameStep       UMETA(DisplayName = "Frame Step")
};

UENUM(BlueprintType)
enum class EReplayEventType : uint8
{
    BallBowled      UMETA(DisplayName = "Ball Bowled"),
    BallBounced     UMETA(DisplayName = "Ball Bounced"),
    BallHitBat      UMETA(DisplayName = "Ball Hit Bat"),
    BallHitPad      UMETA(DisplayName = "Ball Hit Pad"),
    BallHitStumps   UMETA(DisplayName = "Ball Hit Stumps"),
    RunsScored      UMETA(DisplayName = "Runs Scored"),
    WicketFallen    UMETA(DisplayName = "Wicket Fallen"),
    CatchTaken      UMETA(DisplayName = "Catch Taken"),
    RunOut          UMETA(DisplayName = "Run Out"),
    Stumping        UMETA(DisplayName = "Stumping"),
    Wide            UMETA(DisplayName = "Wide"),
    NoBall          UMETA(DisplayName = "No Ball"),
    Bye             UMETA(DisplayName = "Bye"),
    LegBye          UMETA(DisplayName = "Leg Bye"),
    Boundary4       UMETA(DisplayName = "Boundary 4"),
    Boundary6       UMETA(DisplayName = "Boundary 6"),
    DRSReview       UMETA(DisplayName = "DRS Review"),
    InningsBreak    UMETA(DisplayName = "Innings Break"),
    MatchEnd        UMETA(DisplayName = "Match End")
};

USTRUCT(BlueprintType)
struct FReplayEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    EReplayEventType EventType = EReplayEventType::BallBowled;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    float Timestamp = 0.0f; // Seconds from match start

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    float Duration = 5.0f; // Suggested replay duration

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FString Description;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FVector Location; // World location of event

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TArray<FString> InvolvedPlayers;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TMap<FString, FString> Metadata; // Flexible key-value data

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 BallNumber = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 OverNumber = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 Innings = 1;
};

USTRUCT(BlueprintType)
struct FReplayFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FTransform BallTransform;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FVector BallVelocity;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FVector BallSpin;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TMap<FString, FTransform> PlayerTransforms; // PlayerID -> Transform

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TMap<FString, FString> PlayerAnimations; // PlayerID -> Animation name

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TArray<FReplayEvent> EventsAtThisFrame;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 FrameNumber = 0;
};

USTRUCT(BlueprintType)
struct FReplaySession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FString SessionId;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FDateTime RecordedAt;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 MatchId = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    float TotalDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    int32 TotalFrames = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    float FrameRate = 60.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TArray<FReplayEvent> KeyEvents;

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    TArray<FReplayFrame> Frames; // Only keyframes stored, interpolated at playback

    UPROPERTY(BlueprintReadWrite, Category = "Replay")
    FString MatchInfo; // JSON with teams, venue, result, etc.
};

USTRUCT(BlueprintType)
struct FHawkeyeData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    TArray<FVector> PredictedPath; // World space positions

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    TArray<FVector> ActualPath; // From recorded physics

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    FVector PredictedBouncePoint;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    FVector PredictedImpactPoint; // On pads/stumps

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    FVector PredictedStumpImpact;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    bool bPitchesInLine = false;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    bool bImpactInLine = false;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    bool bHitsStumps = false;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float StumpImpactHeight = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float StumpImpactOffset = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float SwingDeviation = 0.0f; // cm

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float SeamDeviation = 0.0f; // cm

    UPROPERTY(BlueprintReadWrite, Category = "Hawkeye")
    float SpinDeviation = 0.0f; // cm
};

USTRUCT(BlueprintType)
struct FWagonWheelData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TArray<FVector2D> ShotDirections; // Normalized field coordinates

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TArray<int32> RunsScored; // Per shot

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TArray<FString> ShotTypes; // Per shot

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TArray<FLinearColor> ShotColors; // Per shot (color coding)

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TMap<int32, int32> RunsDistribution; // Runs -> Count

    UPROPERTY(BlueprintReadWrite, Category = "Wagon Wheel")
    TMap<FString, int32> ShotTypeDistribution; // ShotType -> Count
};

USTRUCT(BlueprintType)
struct FPitchMapData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<FVector> BounceLocations; // Where balls pitched

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<FVector> ImpactLocations; // Where balls hit bat/pad

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<int32> DeliveryTypes; // Per ball

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<float> SwingAmounts; // Per ball

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<float> SpinAmounts; // Per ball

    UPROPERTY(BlueprintReadWrite, Category = "Pitch Map")
    TArray<FLinearColor> BallColors; // Color by outcome
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplayStateChanged, EReplayPlaybackState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplayCameraChanged, EReplayCameraMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReplayTimeChanged, float, CurrentTime, float, TotalDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplayEventTriggered, const FReplayEvent&, Event);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UReplaySystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UReplaySystem();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Recording
    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void StartRecording(int32 MatchId);

    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void StopRecording();

    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void PauseRecording();

    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void ResumeRecording();

    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void RecordEvent(const FReplayEvent& Event);

    UFUNCTION(BlueprintCallable, Category = "Replay|Recording")
    void RecordFrame();

    // Playback
    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void LoadReplay(const FReplaySession& Session);

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void LoadReplayFromFile(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void Pause();

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void SetPlaybackSpeed(float Speed);

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void SeekToTime(float Time);

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void StepFrame(int32 Direction = 1); // +1 forward, -1 backward

    UFUNCTION(BlueprintCallable, Category = "Replay|Playback")
    void JumpToEvent(int32 EventIndex);

    // Camera
    UFUNCTION(BlueprintCallable, Category = "Replay|Camera")
    void SetCameraMode(EReplayCameraMode Mode);

    UFUNCTION(BlueprintCallable, Category = "Replay|Camera")
    void SetCameraTarget(AActor* Target);

    UFUNCTION(BlueprintCallable, Category = "Replay|Camera")
    void AddCameraPosition(const FTransform& Transform, float Duration);

    // Analysis
    UFUNCTION(BlueprintCallable, Category = "Replay|Analysis")
    FHawkeyeData GenerateHawkeyeData(const FReplayEvent& BallEvent) const;

    UFUNCTION(BlueprintCallable, Category = "Replay|Analysis")
    FWagonWheelData GenerateWagonWheelData(int32 Innings = 1) const;

    UFUNCTION(BlueprintCallable, Category = "Replay|Analysis")
    FPitchMapData GeneratePitchMapData(int32 Innings = 1) const;

    // Save/Export
    UFUNCTION(BlueprintCallable, Category = "Replay|Export")
    bool SaveReplayToFile(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Replay|Export")
    void ExportHighlightReel(const TArray<int32>& EventIndices, const FString& OutputPath);

    // Getters
    UFUNCTION(BlueprintPure, Category = "Replay")
    EReplayPlaybackState GetPlaybackState() const { return PlaybackState; }

    UFUNCTION(BlueprintPure, Category = "Replay")
    EReplayCameraMode GetCameraMode() const { return CurrentCameraMode; }

    UFUNCTION(BlueprintPure, Category = "Replay")
    float GetCurrentTime() const { return CurrentPlaybackTime; }

    UFUNCTION(BlueprintPure, Category = "Replay")
    float GetTotalDuration() const { return CurrentSession.TotalDuration; }

    UFUNCTION(BlueprintPure, Category = "Replay")
    const FReplaySession& GetCurrentSession() const { return CurrentSession; }

    UFUNCTION(BlueprintPure, Category = "Replay")
    const TArray<FReplayEvent>& GetKeyEvents() const { return CurrentSession.KeyEvents; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Replay Events")
    FOnReplayStateChanged OnPlaybackStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Replay Events")
    FOnReplayCameraChanged OnCameraModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Replay Events")
    FOnReplayTimeChanged OnTimeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Replay Events")
    FOnReplayEventTriggered OnEventTriggered;

protected:
    // References
    UPROPERTY()
    TObjectPtr<ACricketBall> TrackedBall;

    UPROPERTY()
    TObjectPtr<APitchActor> PitchActor;

    UPROPERTY()
    TObjectPtr<APlayerCameraManager> CameraManager;

    // Recording state
    UPROPERTY()
    bool bIsRecording = false;

    UPROPERTY()
    float RecordingStartTime = 0.0f;

    UPROPERTY()
    float LastFrameRecordTime = 0.0f;

    UPROPERTY()
    FReplaySession CurrentSession;

    // Playback state
    UPROPERTY()
    EReplayPlaybackState PlaybackState = EReplayPlaybackState::Stopped;

    UPROPERTY()
    EReplayCameraMode CurrentCameraMode = EReplayCameraMode::Broadcast;

    UPROPERTY()
    float CurrentPlaybackTime = 0.0f;

    UPROPERTY()
    float PlaybackSpeed = 1.0f;

    UPROPERTY()
    int32 CurrentFrameIndex = 0;

    UPROPERTY()
    AActor* CameraTarget = nullptr;

    // Camera positions for broadcast mode
    UPROPERTY()
    TArray<FTransform> CameraPositions;

    UPROPERTY()
    int32 CurrentCameraIndex = 0;

    UPROPERTY()
    float CameraTransitionTime = 0.0f;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay Config")
    float RecordFrameRate = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay Config")
    float KeyframeInterval = 1.0f; // Seconds between keyframes

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay Config")
    int32 MaxFrames = 36000; // 10 minutes at 60fps

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay Config")
    bool bAutoRecord = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay Config")
    bool bRecordAllPlayers = true;

    // Internal functions
    void UpdateRecording(float DeltaTime);
    void UpdatePlayback(float DeltaTime);
    void UpdateCamera(float DeltaTime);
    void InterpolateFrame(int32 FrameIndex, float Alpha);
    void ApplyFrame(const FReplayFrame& Frame);
    void TriggerEventsAtTime(float Time);
    FReplayFrame CaptureCurrentFrame();
    FTransform GetCameraTransformForMode(EReplayCameraMode Mode);
    void SetupDefaultCameraPositions();
    FHawkeyeData SimulateHawkeye(const FReplayEvent& BallEvent) const;
    void BroadcastStateChange(EReplayPlaybackState NewState);
    void BroadcastCameraChange(EReplayCameraMode NewMode);
};