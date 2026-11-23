#include "CricketGameMode.h"
#include "CricketGameState.h"
#include "CricketPlayerController.h"
#include "CricketBall.h"
#include "PitchActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACricketGameMode::ACricketGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f; // Match timer doesn't need frame-perfect updates

    // Default classes
    GameStateClass = ACricketGameState::StaticClass();
    PlayerControllerClass = ACricketPlayerController::StaticClass();
    DefaultPawnClass = nullptr; // We'll spawn custom pawns

    bReplicates = true;
}

void ACricketGameMode::BeginPlay()
{
    Super::BeginPlay();

    CricketGameState = Cast<ACricketGameState>(GameState);
    if (!CricketGameState)
    {
        UE_LOG(LogTemp, Error, TEXT("CricketGameMode: Failed to cast GameState to ACricketGameState"));
    }

    // Find pitch actor
    TArray<AActor*> PitchActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APitchActor::StaticClass(), PitchActors);
    if (PitchActors.Num() > 0)
    {
        PitchActor = Cast<APitchActor>(PitchActors[0]);
    }

    // Initialize match
    SetMatchState(EMatchState::PreMatch);
}

void ACricketGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (MatchState == EMatchState::Innings1 || MatchState == EMatchState::Innings2)
    {
        UpdateMatchTimer(DeltaSeconds);
    }
}

void ACricketGameMode::StartMatch()
{
    if (MatchState != EMatchState::PreMatch)
    {
        UE_LOG(LogTemp, Warning, TEXT("StartMatch called but not in PreMatch state"));
        return;
    }

    StartToss();
}

void ACricketGameMode::StartToss()
{
    SetMatchState(EMatchState::Toss);

    // In a real implementation, this would involve captains and coin toss logic
    // For now, simulate a toss result
    FTimerHandle TossTimer;
    GetWorldTimerManager().SetTimer(TossTimer, [this]()
    {
        // Randomly decide who bats first
        bool bTeamABatsFirst = FMath::RandBool();
        OnTossComplete(bTeamABatsFirst);
    }, 3.0f, false);
}

void ACricketGameMode::OnTossComplete(bool bTeamABatsFirst)
{
    bTeamABattingFirst = bTeamABatsFirst;
    CurrentInnings = 1;
    SetMatchState(EMatchState::Innings1);
    StartInnings(1);
}

void ACricketGameMode::StartInnings(int32 InningsNumber)
{
    CurrentInnings = InningsNumber;
    CurrentOver = 0;
    CurrentBall = 0;
    BallsInCurrentOver = 0;
    SetInningsState(EInningsState::InProgress);

    // Notify game state
    if (CricketGameState)
    {
        CricketGameState->OnInningsStarted(InningsNumber, bTeamABattingFirst);
    }

    // Prepare first delivery
    StartNewDelivery();
}

void ACricketGameMode::EndInnings()
{
    SetInningsState(EInningsState::InningsComplete);

    if (CricketGameState)
    {
        CricketGameState->OnInningsEnded(CurrentInnings);
    }

    if (CurrentInnings == 1)
    {
        SetMatchState(EMatchState::InningsBreak);
        // Schedule second innings start
        FTimerHandle BreakTimer;
        GetWorldTimerManager().SetTimer(BreakTimer, [this]()
        {
            SetMatchState(EMatchState::Innings2);
            StartInnings(2);
        }, 10.0f, false); // 10 second break for demo
    }
    else
    {
        EndMatch();
    }
}

void ACricketGameMode::EndMatch()
{
    SetMatchState(EMatchState::MatchEnd);
    SetInningsState(EInningsState::InningsComplete);

    if (CricketGameState)
    {
        CricketGameState->OnMatchEnded();
    }

    UE_LOG(LogTemp, Log, TEXT("Match ended!"));
}

void ACricketGameMode::PauseMatch()
{
    if (MatchState == EMatchState::Innings1 || MatchState == EMatchState::Innings2)
    {
        SetMatchState(EMatchState::Paused);
        UGameplayStatics::SetGamePaused(GetWorld(), true);
    }
}

void ACricketGameMode::ResumeMatch()
{
    if (MatchState == EMatchState::Paused)
    {
        // Restore previous innings state
        SetMatchState(CurrentInnings == 1 ? EMatchState::Innings1 : EMatchState::Innings2);
        UGameplayStatics::SetGamePaused(GetWorld(), false);
    }
}

void ACricketGameMode::StartNewDelivery()
{
    if (InningsState != EInningsState::InProgress)
    {
        return;
    }

    // Check if over is complete
    if (BallsInCurrentOver >= 6)
    {
        HandleOverComplete();
        return;
    }

    // Check innings end conditions
    CheckInningsEndConditions();
    if (InningsState != EInningsState::InProgress)
    {
        return;
    }

    // Increment ball counter
    CurrentBall++;
    BallsInCurrentOver++;

    UE_LOG(LogTemp, Log, TEXT("Starting delivery %d.%d (Over %d, Ball %d)"), CurrentOver, BallsInCurrentOver, CurrentOver + 1, BallsInCurrentOver);

    // Spawn ball at bowler's end
    // This would be called by the bowler's action in practice
    // For now, we just signal that a new delivery can begin
    if (CricketGameState)
    {
        CricketGameState->OnNewDelivery(CurrentOver, BallsInCurrentOver);
    }
}

void ACricketGameMode::OnBallDelivered(ACricketBall* Ball)
{
    CurrentBallActor = Ball;

    if (CricketGameState)
    {
        CricketGameState->OnBallInPlay(Ball);
    }
}

void ACricketGameMode::OnBallDead(int32 RunsScored, bool bIsWicket, bool bIsExtra)
{
    CurrentBallActor = nullptr;

    // Update score
    if (CricketGameState)
    {
        CricketGameState->AddRuns(RunsScored, bIsExtra);
        if (bIsWicket)
        {
            CricketGameState->AddWicket();
            OnWicketFallen.Broadcast(CricketGameState->GetWicketsDown(), TEXT("Batter")); // Would pass actual batter name
        }
        else if (RunsScored > 0)
        {
            OnRunsScored.Broadcast(RunsScored, TEXT("Batter")); // Would pass actual batter name
        }
    }

    // Check for end conditions
    CheckInningsEndConditions();

    if (InningsState == EInningsState::InProgress)
    {
        // Start next delivery after a brief delay
        FTimerHandle NextBallTimer;
        GetWorldTimerManager().SetTimer(NextBallTimer, [this]()
        {
            StartNewDelivery();
        }, 2.0f, false);
    }
}

void ACricketGameMode::HandleOverComplete()
{
    BallsInCurrentOver = 0;
    CurrentOver++;

    OnOverComplete.Broadcast(CurrentOver);

    if (CricketGameState)
    {
        CricketGameState->OnOverCompleted(CurrentOver);
    }

    // Check if innings is complete (overs reached)
    if (CurrentOver >= MatchConfig.OversPerInnings)
    {
        EndInnings();
        return;
    }

    // Change ends, new bowler, etc.
    StartNewDelivery();
}

void ACricketGameMode::RequestDRSReview(AActor* RequestingPlayer, FGameplayTag ReviewType)
{
    if (!MatchConfig.bAllowDRS)
    {
        return;
    }

    if (MatchState != EMatchState::Innings1 && MatchState != EMatchState::Innings2)
    {
        return;
    }

    // Check if team has reviews left
    // Implementation would check team's remaining reviews

    SetMatchState(EMatchState::DRSReview);

    // Start DRS review process
    // This would involve ball tracking, ultra edge, hot spot simulations
    UE_LOG(LogTemp, Log, TEXT("DRS Review requested: %s"), *ReviewType.ToString());
}

void ACricketGameMode::CompleteDRSReview(bool bOriginalDecisionOverturned)
{
    UE_LOG(LogTemp, Log, TEXT("DRS Review complete. Overturned: %s"), bOriginalDecisionOverturned ? TEXT("Yes") : TEXT("No"));

    // Restore match state
    SetMatchState(CurrentInnings == 1 ? EMatchState::Innings1 : EMatchState::Innings2);
}

void ACricketGameMode::SetMatchState(EMatchState NewState)
{
    if (MatchState != NewState)
    {
        MatchState = NewState;
        OnMatchStateChanged.Broadcast(NewState);

        if (CricketGameState)
        {
            CricketGameState->OnMatchStateChanged(NewState);
        }

        UE_LOG(LogTemp, Log, TEXT("Match state changed to: %s"), *UEnum::GetValueAsString(NewState));
    }
}

void ACricketGameMode::SetInningsState(EInningsState NewState)
{
    if (InningsState != NewState)
    {
        InningsState = NewState;
        OnInningsStateChanged.Broadcast(NewState);

        if (CricketGameState)
        {
            CricketGameState->OnInningsStateChanged(NewState);
        }
    }
}

void ACricketGameMode::UpdateMatchTimer(float DeltaSeconds)
{
    MatchTimer += DeltaSeconds;
}

void ACricketGameMode::CheckInningsEndConditions()
{
    if (!CricketGameState)
    {
        return;
    }

    // All out (10 wickets)
    if (CricketGameState->GetWicketsDown() >= 10)
    {
        EndInnings();
        return;
    }

    // Overs completed
    if (CurrentOver >= MatchConfig.OversPerInnings && BallsInCurrentOver == 0)
    {
        EndInnings();
        return;
    }

    // Target reached (second innings)
    if (CurrentInnings == 2 && CricketGameState->GetTeamARuns() > CricketGameState->GetTeamBRuns())
    {
        // Team A batted first, Team B chasing
        if (bTeamABattingFirst)
        {
            // Team B has surpassed Team A's total
            if (CricketGameState->GetTeamBRuns() > CricketGameState->GetTeamARuns())
            {
                EndInnings(); // Team B wins
            }
        }
        else
        {
            // Team A chasing
            if (CricketGameState->GetTeamARuns() > CricketGameState->GetTeamBRuns())
            {
                EndInnings(); // Team A wins
            }
        }
    }
}

void ACricketGameMode::AdvanceOver()
{
    CurrentOver++;
    BallsInCurrentOver = 0;
}