#include "CricketGameState.h"
#include "Net/UnrealNetwork.h"

ACricketGameState::ACricketGameState()
{
    bReplicates = true;
    TeamAScore.Runs = 0;
    TeamAScore.Wickets = 0;
    TeamAScore.Overs = 0.0f;
    TeamBScore.Runs = 0;
    TeamBScore.Wickets = 0;
    TeamBScore.Overs = 0.0f;
    CurrentStrikerIndex = 0;
    CurrentNonStrikerIndex = 1;
    CurrentBowlerIndex = 0;
    bTeamABatting = true;
    TargetScore = 0;
    PartnershipRuns = 0;
    PartnershipBalls = 0;
    TeamADRSReviewsRemaining = 2;
    TeamBDRSReviewsRemaining = 2;
}

void ACricketGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ACricketGameState, TeamAScore);
    DOREPLIFETIME(ACricketGameState, TeamBScore);
    DOREPLIFETIME(ACricketGameState, TeamABattingOrder);
    DOREPLIFETIME(ACricketGameState, TeamBBattingOrder);
    DOREPLIFETIME(ACricketGameState, TeamABowlingFigures);
    DOREPLIFETIME(ACricketGameState, TeamBBowlingFigures);
    DOREPLIFETIME(ACricketGameState, BallByBallLog);
    DOREPLIFETIME(ACricketGameState, CurrentStrikerIndex);
    DOREPLIFETIME(ACricketGameState, CurrentNonStrikerIndex);
    DOREPLIFETIME(ACricketGameState, CurrentBowlerIndex);
    DOREPLIFETIME(ACricketGameState, bTeamABatting);
    DOREPLIFETIME(ACricketGameState, TargetScore);
    DOREPLIFETIME(ACricketGameState, PartnershipRuns);
    DOREPLIFETIME(ACricketGameState, PartnershipBalls);
    DOREPLIFETIME(ACricketGameState, TeamADRSReviewsRemaining);
    DOREPLIFETIME(ACricketGameState, TeamBDRSReviewsRemaining);
}

void FTeamScore::AddRuns(int32 RunCount, bool bIsExtra)
{
    Runs += RunCount;
    if (bIsExtra)
    {
        Extras += RunCount;
    }
}

void FTeamScore::AddWicket()
{
    Wickets++;
}

void FTeamScore::AddOver(float OversCompleted)
{
    Overs += OversCompleted;
}

void FTeamScore::CalculateRunRate(int32 TotalBallsBowled)
{
    if (TotalBallsBowled > 0)
    {
        RunRate = (float)Runs / (float)TotalBallsBowled * 6.0f;
    }
    else
    {
        RunRate = 0.0f;
    }
}

void ACricketGameState::AddRuns(int32 Runs, bool bIsExtra)
{
    if (bTeamABatting)
    {
        TeamAScore.AddRuns(Runs, bIsExtra);
        TeamAScore.CalculateRunRate(FMath::FloorToInt(TeamAScore.Overs * 6));
        OnScoreUpdated.Broadcast(0);
    }
    else
    {
        TeamBScore.AddRuns(Runs, bIsExtra);
        TeamBScore.CalculateRunRate(FMath::FloorToInt(TeamBScore.Overs * 6));
        OnScoreUpdated.Broadcast(1);
    }

    PartnershipRuns += Runs;
    PartnershipBalls++;
}

void ACricketGameState::AddWicket()
{
    if (bTeamABatting)
    {
        TeamAScore.AddWicket();
        TeamAScore.CalculateRunRate(FMath::FloorToInt(TeamAScore.Overs * 6));

        // Update batter stats
        if (CurrentStrikerIndex < TeamABattingOrder.Num())
        {
            TeamABattingOrder[CurrentStrikerIndex].bIsOut = true;
        }

        OnWicketFallen.Broadcast(TeamAScore.Wickets);
    }
    else
    {
        TeamBScore.AddWicket();
        TeamBScore.CalculateRunRate(FMath::FloorToInt(TeamBScore.Overs * 6));

        if (CurrentStrikerIndex < TeamBBattingOrder.Num())
        {
            TeamBBattingOrder[CurrentStrikerIndex].bIsOut = true;
        }

        OnWicketFallen.Broadcast(TeamBScore.Wickets);
    }

    PartnershipRuns = 0;
    PartnershipBalls = 0;
}

void ACricketGameState::AddExtra(int32 Runs, FGameplayTag ExtraType)
{
    AddRuns(Runs, true);

    // Track specific extra types
    if (bTeamABatting)
    {
        if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Wide")))
        {
            TeamAScore.Wides++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.NoBall")))
        {
            TeamAScore.NoBalls++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Bye")))
        {
            TeamAScore.Byes++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.LegBye")))
        {
            TeamAScore.LegByes++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Penalty")))
        {
            TeamAScore.PenaltyRuns += Runs;
        }
    }
    else
    {
        if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Wide")))
        {
            TeamBScore.Wides++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.NoBall")))
        {
            TeamBScore.NoBalls++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Bye")))
        {
            TeamBScore.Byes++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.LegBye")))
        {
            TeamBScore.LegByes++;
        }
        else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Penalty")))
        {
            TeamBScore.PenaltyRuns += Runs;
        }
    }
}

void ACricketGameState::RecordBall(int32 Runs, bool bIsWicket, bool bIsExtra, FGameplayTag ExtraType, FGameplayTag WicketType, FGameplayTag ShotType)
{
    FBallByBallEntry Entry;
    Entry.OverNumber = FMath::FloorToInt(bTeamABatting ? TeamAScore.Overs : TeamBScore.Overs);
    Entry.BallNumber = (bTeamABatting ? TeamAScore.Overs : TeamBScore.Overs) - Entry.OverNumber > 0.001f ? 6 : 0; // Simplified

    if (bTeamABatting)
    {
        Entry.BowlerName = CurrentBowlerIndex < TeamBBowlingFigures.Num() ? TeamBBowlingFigures[CurrentBowlerIndex].PlayerName : TEXT("Bowler");
        Entry.BatterName = CurrentStrikerIndex < TeamABattingOrder.Num() ? TeamABattingOrder[CurrentStrikerIndex].PlayerName : TEXT("Batter");
        Entry.NonStrikerName = CurrentNonStrikerIndex < TeamABattingOrder.Num() ? TeamABattingOrder[CurrentNonStrikerIndex].PlayerName : TEXT("Non-Striker");
    }
    else
    {
        Entry.BowlerName = CurrentBowlerIndex < TeamABowlingFigures.Num() ? TeamABowlingFigures[CurrentBowlerIndex].PlayerName : TEXT("Bowler");
        Entry.BatterName = CurrentStrikerIndex < TeamBBattingOrder.Num() ? TeamBBattingOrder[CurrentStrikerIndex].PlayerName : TEXT("Batter");
        Entry.NonStrikerName = CurrentNonStrikerIndex < TeamBBattingOrder.Num() ? TeamBBattingOrder[CurrentNonStrikerIndex].PlayerName : TEXT("Non-Striker");
    }

    Entry.RunsScored = Runs;
    Entry.bIsWicket = bIsWicket;
    Entry.bIsExtra = bIsExtra;
    Entry.ExtraType = ExtraType.IsValid() ? ExtraType.ToString() : TEXT("");
    Entry.WicketType = WicketType.IsValid() ? WicketType.ToString() : TEXT("");
    Entry.ShotType = ShotType.IsValid() ? ShotType.ToString() : TEXT("");

    BallByBallLog.Add(Entry);

    // Update bowler stats
    if (bTeamABatting)
    {
        if (CurrentBowlerIndex < TeamBBowlingFigures.Num())
        {
            TeamBBowlingFigures[CurrentBowlerIndex].RunsConceded += Runs;
            if (bIsWicket)
            {
                TeamBBowlingFigures[CurrentBowlerIndex].WicketsTaken++;
            }
            if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Wide")))
            {
                TeamBBowlingFigures[CurrentBowlerIndex].Wides++;
            }
            else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.NoBall")))
            {
                TeamBBowlingFigures[CurrentBowlerIndex].NoBalls++;
            }
        }
    }
    else
    {
        if (CurrentBowlerIndex < TeamABowlingFigures.Num())
        {
            TeamABowlingFigures[CurrentBowlerIndex].RunsConceded += Runs;
            if (bIsWicket)
            {
                TeamABowlingFigures[CurrentBowlerIndex].WicketsTaken++;
            }
            if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.Wide")))
            {
                TeamABowlingFigures[CurrentBowlerIndex].Wides++;
            }
            else if (ExtraType.MatchesTag(FGameplayTag::RequestGameplayTag("Extra.NoBall")))
            {
                TeamABowlingFigures[CurrentBowlerIndex].NoBalls++;
            }
        }
    }
}

void ACricketGameState::RecordDelivery(const FBallByBallEntry& Entry)
{
    BallByBallLog.Add(Entry);
}

float ACricketGameState::GetRequiredRunRate(int32 Target, int32 OversRemaining) const
{
    int32 RunsNeeded = Target - GetCurrentRuns();
    if (RunsNeeded <= 0 || OversRemaining <= 0)
    {
        return 0.0f;
    }
    return (float)RunsNeeded / (float)OversRemaining;
}

void ACricketGameState::SetTeamABattingOrder(const TArray<FBatterStats>& Order)
{
    TeamABattingOrder = Order;
}

void ACricketGameState::SetTeamBBattingOrder(const TArray<FBatterStats>& Order)
{
    TeamBBattingOrder = Order;
}

void ACricketGameState::SetBattingTeam(bool bIsTeamA)
{
    bTeamABatting = bIsTeamA;
    CurrentStrikerIndex = 0;
    CurrentNonStrikerIndex = 1;
    PartnershipRuns = 0;
    PartnershipBalls = 0;
}

void ACricketGameState::SetCurrentBatters(int32 StrikerIndex, int32 NonStrikerIndex)
{
    CurrentStrikerIndex = StrikerIndex;
    CurrentNonStrikerIndex = NonStrikerIndex;
}

void ACricketGameState::SetCurrentBowler(int32 BowlerIndex)
{
    CurrentBowlerIndex = BowlerIndex;
}

void ACricketGameState::OnInningsStarted(int32 InningsNumber, bool bTeamABatsFirst)
{
    bTeamABatting = bTeamABatsFirst;
    CurrentStrikerIndex = 0;
    CurrentNonStrikerIndex = 1;
    PartnershipRuns = 0;
    PartnershipBalls = 0;

    OnInningsStartedEvent.Broadcast(InningsNumber);
}

void ACricketGameState::OnInningsEnded(int32 InningsNumber)
{
    OnInningsEndedEvent.Broadcast(InningsNumber);
}

void ACricketGameState::OnMatchEnded()
{
    bool bTeamAWon = TeamAScore.Runs > TeamBScore.Runs;
    OnMatchEndedEvent.Broadcast(bTeamAWon);
}

void ACricketGameState::OnNewDelivery(int32 Over, int32 Ball)
{
    OnNewDeliveryEvent.Broadcast(Over, Ball);
}

void ACricketGameState::OnBallInPlay(ACricketBall* Ball)
{
    OnBallInPlayEvent.Broadcast(Ball);
}

void ACricketGameState::OnMatchStateChanged(TEnumAsByte<EMatchState> NewState)
{
    OnMatchStateChangedEvent.Broadcast(NewState);
}

void ACricketGameState::OnInningsStateChanged(TEnumAsByte<EInningsState> NewState)
{
    OnInningsStateChangedEvent.Broadcast(NewState);
}