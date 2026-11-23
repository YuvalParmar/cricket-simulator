#include "CricketHUD.h"
#include "Blueprint/UserWidget.h"
#include "CricketGameState.h"
#include "CricketGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"

ACricketHUD::ACricketHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACricketHUD::BeginPlay()
{
    Super::BeginPlay();

    // Get game state reference
    CricketGameState = Cast<ACricketGameState>(UGameplayStatics::GetGameState(GetWorld()));

    CreateWidgets();

    // Set initial mode
    SetHUDMode(EHUDMode::General);
}

void ACricketHUD::CreateWidgets()
{
    // Widgets would be created from Blueprint classes
    // For now, we'll create placeholder logic
    // In practice, these would be set via Blueprint or loaded from asset paths
}

void ACricketHUD::DrawHUD()
{
    Super::DrawHUD();

    // Debug drawing fallback if widgets not available
    DrawDebugInfo();
}

void ACricketHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateFromGameState();
}

void ACricketHUD::SetHUDMode(EHUDMode NewMode)
{
    if (CurrentMode != NewMode)
    {
        CurrentMode = NewMode;
        SwitchWidget(NewMode);
        OnHUDModeChanged.Broadcast(NewMode);
    }
}

void ACricketHUD::SwitchWidget(EHUDMode Mode)
{
    // Hide all widgets
    if (MainHUDWidget) MainHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (BattingWidget) BattingWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (BowlingWidget) BowlingWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (FieldingWidget) FieldingWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (DRSWidget) DRSWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (ReplayWidget) ReplayWidget->SetVisibility(ESlateVisibility::Collapsed);
    if (PhoneBatWidget) PhoneBatWidget->SetVisibility(ESlateVisibility::Collapsed);

    // Show relevant widget
    switch (Mode)
    {
        case EHUDMode::General:
            if (MainHUDWidget) MainHUDWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::Batting:
            if (BattingWidget) BattingWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::Bowling:
            if (BowlingWidget) BowlingWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::Fielding:
            if (FieldingWidget) FieldingWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::DRSReview:
            if (DRSWidget) DRSWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::Replay:
            if (ReplayWidget) ReplayWidget->SetVisibility(ESlateVisibility::Visible);
            break;
        case EHUDMode::PauseMenu:
            // Pause menu handled separately
            break;
    }
}

void ACricketHUD::UpdateScoreboard(const FScoreboardData& Data)
{
    ScoreboardData = Data;

    if (MainHUDWidget)
    {
        // Update widget - would call Blueprint function
        // MainHUDWidget->CallFunctionByNameWithArguments(TEXT("UpdateScoreboard"), ...);
    }
}

void ACricketHUD::UpdateBatterInfo(const FBatterDisplayInfo& Striker, const FBatterDisplayInfo& NonStriker)
{
    StrikerInfo = Striker;
    NonStrikerInfo = NonStriker;

    if (BattingWidget)
    {
        // Update widget
    }
}

void ACricketHUD::ShowShotFeedback(const FShotFeedbackData& Data)
{
    OnShotFeedback.Broadcast(Data);

    if (BattingWidget)
    {
        // Show shot feedback animation
    }
}

void ACricketHUD::UpdateTimingMeter(float Progress, FString WindowText, FLinearColor WindowColor)
{
    if (BattingWidget)
    {
        // Update timing meter UI
    }
}

void ACricketHUD::UpdateShotSelectionWheel(const TArray<FString>& AvailableShots, int32 SelectedIndex)
{
    if (BattingWidget)
    {
        // Update shot selection wheel
    }
}

void ACricketHUD::UpdateBowlerInfo(const FBowlerDisplayInfo& Bowler)
{
    CurrentBowlerInfo = Bowler;

    if (BowlingWidget)
    {
        // Update bowler info
    }
}

void ACricketHUD::ShowDeliveryFeedback(const FDeliveryFeedbackData& Data)
{
    OnDeliveryFeedback.Broadcast(Data);

    if (BowlingWidget)
    {
        // Show delivery feedback
    }
}

void ACricketHUD::UpdateFieldPlacement(const TArray<FVector>& FielderPositions)
{
    if (BowlingWidget)
    {
        // Update field placement overlay
    }
}

void ACricketHUD::UpdateRunUpProgress(float Progress)
{
    if (BowlingWidget)
    {
        // Update run-up progress bar
    }
}

void ACricketHUD::UpdateEffortMeter(float Effort)
{
    if (BowlingWidget)
    {
        // Update effort meter
    }
}

void ACricketHUD::UpdateFieldingPosition(FString PositionName)
{
    if (FieldingWidget)
    {
        // Update fielding position display
    }
}

void ACricketHUD::ShowThrowMeter(float Charge, float Accuracy)
{
    if (FieldingWidget)
    {
        // Show throw power/accuracy meter
    }
}

void ACricketHUD::ShowCatchWindow(float TimeRemaining)
{
    if (FieldingWidget)
    {
        // Show catch timing window
    }
}

void ACricketHUD::ShowDRSReview(FString ReviewType, FString ReviewingTeam)
{
    SetHUDMode(EHUDMode::DRSReview);

    if (DRSWidget)
    {
        // Show DRS review UI with type and team
    }
}

void ACricketHUD::UpdateDRSProgress(FString Stage, float Progress)
{
    if (DRSWidget)
    {
        // Update DRS progress (ball tracking, ultra edge, hotspot)
    }
}

void ACricketHUD::ShowDRSResult(bool bOverturned, FString Reason)
{
    if (DRSWidget)
    {
        // Show DRS result animation
    }
}

void ACricketHUD::ShowReplay(FString ReplayType)
{
    SetHUDMode(EHUDMode::Replay);

    if (ReplayWidget)
    {
        // Show replay with multi-cam angles
    }
}

void ACricketHUD::SetReplayCamera(int32 CameraIndex)
{
    if (ReplayWidget)
    {
        // Switch replay camera angle
    }
}

void ACricketHUD::UpdatePhoneBatConnection(bool bConnected, float Latency)
{
    if (PhoneBatWidget)
    {
        // Update connection status
    }
}

void ACricketHUD::ShowPhoneBatCalibration()
{
    if (PhoneBatWidget)
    {
        // Show calibration UI
    }
}

void ACricketHUD::UpdatePhoneBatSwing(float Progress, int32 Phase)
{
    if (PhoneBatWidget)
    {
        // Update swing visualization
    }
}

void ACricketHUD::UpdateFromGameState()
{
    if (!CricketGameState)
    {
        return;
    }

    // Update scoreboard
    FScoreboardData Data;
    Data.TeamARuns = CricketGameState->GetTeamARuns();
    Data.TeamAWickets = CricketGameState->GetTeamAWickets();
    Data.TeamAOvers = CricketGameState->GetTeamAOvers();
    Data.TeamARunRate = CricketGameState->GetTeamARunRate();
    Data.TeamBRuns = CricketGameState->GetTeamBRuns();
    Data.TeamBWickets = CricketGameState->GetTeamBWickets();
    Data.TeamBOvers = CricketGameState->GetTeamBOvers();
    Data.TeamBRunRate = CricketGameState->GetTeamBRunRate();
    Data.CurrentInnings = 1; // Would get from game mode
    Data.TargetScore = CricketGameState->TargetScore;

    if (Data.TargetScore > 0)
    {
        int32 OversRemaining = 20 - FMath::FloorToInt(Data.TeamBOvers); // Example
        Data.RequiredRunRate = CricketGameState->GetRequiredRunRate(Data.TargetScore, OversRemaining);
    }

    UpdateScoreboard(Data);

    // Update batter info
    const TArray<FBatterStats>& BattingOrder = CricketGameState->GetCurrentBattingOrder();
    if (BattingOrder.Num() > CricketGameState->CurrentStrikerIndex)
    {
        const FBatterStats& StrikerStats = BattingOrder[CricketGameState->CurrentStrikerIndex];
        FBatterDisplayInfo StrikerInfo;
        StrikerInfo.Name = StrikerStats.PlayerName;
        StrikerInfo.Runs = StrikerStats.Runs;
        StrikerInfo.BallsFaced = StrikerStats.BallsFaced;
        StrikerInfo.Fours = StrikerStats.Fours;
        StrikerInfo.Sixes = StrikerStats.Sixes;
        StrikerInfo.StrikeRate = StrikerStats.StrikeRate;
        StrikerInfo.bIsStriker = true;
        StrikerInfo.bIsOut = StrikerStats.bIsOut;

        if (BattingOrder.Num() > CricketGameState->CurrentNonStrikerIndex)
        {
            const FBatterStats& NonStrikerStats = BattingOrder[CricketGameState->CurrentNonStrikerIndex];
            FBatterDisplayInfo NonStrikerInfo;
            NonStrikerInfo.Name = NonStrikerStats.PlayerName;
            NonStrikerInfo.Runs = NonStrikerStats.Runs;
            NonStrikerInfo.BallsFaced = NonStrikerStats.BallsFaced;
            NonStrikerInfo.Fours = NonStrikerStats.Fours;
            NonStrikerInfo.Sixes = NonStrikerStats.Sixes;
            NonStrikerInfo.StrikeRate = NonStrikerStats.StrikeRate;
            NonStrikerInfo.bIsStriker = false;
            NonStrikerInfo.bIsOut = NonStrikerStats.bIsOut;

            UpdateBatterInfo(StrikerInfo, NonStrikerInfo);
        }
    }

    // Update bowler info
    const TArray<FBowlerStats>& BowlingFigures = CricketGameState->GetCurrentBowlingFigures();
    if (BowlingFigures.Num() > CricketGameState->CurrentBowlerIndex)
    {
        const FBowlerStats& BowlerStats = BowlingFigures[CricketGameState->CurrentBowlerIndex];
        FBowlerDisplayInfo BowlerInfo;
        BowlerInfo.Name = BowlerStats.PlayerName;
        BowlerInfo.OversBowled = BowlerStats.OversBowled;
        BowlerInfo.Maidens = BowlerStats.Maidens;
        BowlerInfo.RunsConceded = BowlerStats.RunsConceded;
        BowlerInfo.WicketsTaken = BowlerStats.WicketsTaken;
        BowlerInfo.EconomyRate = BowlerStats.EconomyRate;
        BowlerInfo.bIsCurrentBowler = true;

        UpdateBowlerInfo(BowlerInfo);
    }
}

void ACricketHUD::DrawDebugInfo()
{
    if (!Canvas)
    {
        return;
    }

    // Draw basic debug info if widgets not available
    FString DebugText = FString::Printf(TEXT("Cricket Simulator - Mode: %s"), *UEnum::GetValueAsString(CurrentMode));
    DrawText(Canvas, FColor::White, DebugText, 10.0f, 10.0f, Canvas->DefaultFont, 1.0f, false);

    if (CricketGameState)
    {
        FString ScoreText = FString::Printf(TEXT("%d/%d (%.1f ov) - Target: %d"),
            CricketGameState->GetCurrentRuns(),
            CricketGameState->GetWicketsDown(),
            CricketGameState->GetCurrentOvers(),
            CricketGameState->TargetScore);
        DrawText(Canvas, FColor::Yellow, ScoreText, 10.0f, 35.0f, Canvas->DefaultFont, 1.0f, false);
    }
}