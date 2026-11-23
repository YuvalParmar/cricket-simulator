#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CricketGameState.h"
#include "CricketHUD.generated.h"

class UUserWidget;
class ACricketGameState;
class ACricketBattingPawn;
class ACricketBowlingPawn;

UENUM(BlueprintType)
enum class EHUDMode : uint8
{
    General         UMETA(DisplayName = "General"),
    Batting         UMETA(DisplayName = "Batting"),
    Bowling         UMETA(DisplayName = "Bowling"),
    Fielding        UMETA(DisplayName = "Fielding"),
    DRSReview       UMETA(DisplayName = "DRS Review"),
    Replay          UMETA(DisplayName = "Replay"),
    PauseMenu       UMETA(DisplayName = "Pause Menu")
};

USTRUCT(BlueprintType)
struct FScoreboardData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 TeamARuns = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 TeamAWickets = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    float TeamAOvers = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    float TeamARunRate = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 TeamBRuns = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 TeamBWickets = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    float TeamBOvers = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    float TeamBRunRate = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 CurrentInnings = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    int32 TargetScore = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Scoreboard")
    float RequiredRunRate = 0.0f;
};

USTRUCT(BlueprintType)
struct FBatterDisplayInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    FString Name;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    int32 Runs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    int32 BallsFaced = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    int32 Fours = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    int32 Sixes = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    float StrikeRate = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    bool bIsStriker = false;

    UPROPERTY(BlueprintReadWrite, Category = "Batter")
    bool bIsOut = false;
};

USTRUCT(BlueprintType)
struct FBowlerDisplayInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    FString Name;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    float OversBowled = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    int32 Maidens = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    int32 RunsConceded = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    int32 WicketsTaken = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    float EconomyRate = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowler")
    bool bIsCurrentBowler = false;
};

USTRUCT(BlueprintType)
struct FShotFeedbackData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    FString ShotName;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    FString TimingText; // "Perfect", "Early", "Late", etc.

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    FLinearColor TimingColor;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    int32 RunsScored = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    bool bIsWicket = false;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    FString WicketType;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    float PowerRating = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    float PlacementRating = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    float TimingRating = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Shot Feedback")
    FVector2D PlacementDirection; // For wagon wheel
};

USTRUCT(BlueprintType)
struct FDeliveryFeedbackData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    FString DeliveryType;

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    float ActualSpeed = 0.0f; // km/h

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    float Swing = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    float Spin = 0.0f; // RPM

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    float Accuracy = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    float RhythmScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    bool bNoBall = false;

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    bool bWide = false;

    UPROPERTY(BlueprintReadWrite, Category = "Delivery Feedback")
    FVector PitchLocation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHUDModeChanged, EHUDMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotFeedback, const FShotFeedbackData&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeliveryFeedback, const FDeliveryFeedbackData&, Data);

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API ACricketHUD : public AHUD
{
    GENERATED_BODY()

public:
    ACricketHUD();

    virtual void BeginPlay() override;
    virtual void DrawHUD() override;
    virtual void Tick(float DeltaSeconds) override;

    // HUD Mode management
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void SetHUDMode(EHUDMode NewMode);

    UFUNCTION(BlueprintPure, Category = "HUD")
    EHUDMode GetHUDMode() const { return CurrentMode; }

    // Scoreboard
    UFUNCTION(BlueprintCallable, Category = "HUD|Scoreboard")
    void UpdateScoreboard(const FScoreboardData& Data);

    UFUNCTION(BlueprintPure, Category = "HUD|Scoreboard")
    const FScoreboardData& GetScoreboardData() const { return ScoreboardData; }

    // Batting UI
    UFUNCTION(BlueprintCallable, Category = "HUD|Batting")
    void UpdateBatterInfo(const FBatterDisplayInfo& Striker, const FBatterDisplayInfo& NonStriker);

    UFUNCTION(BlueprintCallable, Category = "HUD|Batting")
    void ShowShotFeedback(const FShotFeedbackData& Data);

    UFUNCTION(BlueprintCallable, Category = "HUD|Batting")
    void UpdateTimingMeter(float Progress, FString WindowText, FLinearColor WindowColor);

    UFUNCTION(BlueprintCallable, Category = "HUD|Batting")
    void UpdateShotSelectionWheel(const TArray<FString>& AvailableShots, int32 SelectedIndex);

    // Bowling UI
    UFUNCTION(BlueprintCallable, Category = "HUD|Bowling")
    void UpdateBowlerInfo(const FBowlerDisplayInfo& Bowler);

    UFUNCTION(BlueprintCallable, Category = "HUD|Bowling")
    void ShowDeliveryFeedback(const FDeliveryFeedbackData& Data);

    UFUNCTION(BlueprintCallable, Category = "HUD|Bowling")
    void UpdateFieldPlacement(const TArray<FVector>& FielderPositions);

    UFUNCTION(BlueprintCallable, Category = "HUD|Bowling")
    void UpdateRunUpProgress(float Progress);

    UFUNCTION(BlueprintCallable, Category = "HUD|Bowling")
    void UpdateEffortMeter(float Effort);

    // Fielding UI
    UFUNCTION(BlueprintCallable, Category = "HUD|Fielding")
    void UpdateFieldingPosition(FString PositionName);

    UFUNCTION(BlueprintCallable, Category = "HUD|Fielding")
    void ShowThrowMeter(float Charge, float Accuracy);

    UFUNCTION(BlueprintCallable, Category = "HUD|Fielding")
    void ShowCatchWindow(float TimeRemaining);

    // DRS UI
    UFUNCTION(BlueprintCallable, Category = "HUD|DRS")
    void ShowDRSReview(FString ReviewType, FString ReviewingTeam);

    UFUNCTION(BlueprintCallable, Category = "HUD|DRS")
    void UpdateDRSProgress(FString Stage, float Progress);

    UFUNCTION(BlueprintCallable, Category = "HUD|DRS")
    void ShowDRSResult(bool bOverturned, FString Reason);

    // Replay UI
    UFUNCTION(BlueprintCallable, Category = "HUD|Replay")
    void ShowReplay(FString ReplayType);

    UFUNCTION(BlueprintCallable, Category = "HUD|Replay")
    void SetReplayCamera(int32 CameraIndex);

    // Phone Bat UI
    UFUNCTION(BlueprintCallable, Category = "HUD|PhoneBat")
    void UpdatePhoneBatConnection(bool bConnected, float Latency);

    UFUNCTION(BlueprintCallable, Category = "HUD|PhoneBat")
    void ShowPhoneBatCalibration();

    UFUNCTION(BlueprintCallable, Category = "HUD|PhoneBat")
    void UpdatePhoneBatSwing(float Progress, int32 Phase);

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnHUDModeChanged OnHUDModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShotFeedback OnShotFeedback;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeliveryFeedback OnDeliveryFeedback;

protected:
    // Widget references
    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> BattingWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> BowlingWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> FieldingWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> DRSWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> ReplayWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> PhoneBatWidget;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "HUD State")
    EHUDMode CurrentMode = EHUDMode::General;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "HUD State")
    FScoreboardData ScoreboardData;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "HUD State")
    FBatterDisplayInfo StrikerInfo;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "HUD State")
    FBatterDisplayInfo NonStrikerInfo;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "HUD State")
    FBowlerDisplayInfo CurrentBowlerInfo;

    // Game state reference
    UPROPERTY()
    ACricketGameState* CricketGameState;

    // Internal functions
    void CreateWidgets();
    void SwitchWidget(EHUDMode Mode);
    void UpdateFromGameState();
    void DrawDebugInfo();
};