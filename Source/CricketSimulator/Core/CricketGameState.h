#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTags.h"
#include "CricketGameState.generated.h"

class ACricketPlayerState;
class ACricketBall;

USTRUCT(BlueprintType)
struct FTeamScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 Runs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 Wickets = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    float Overs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    float RunRate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 Extras = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 Wides = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 NoBalls = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 Byes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 LegByes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Score")
    int32 PenaltyRuns = 0;

    void AddRuns(int32 RunCount, bool bIsExtra = false);
    void AddWicket();
    void AddOver(float OversCompleted);
    void CalculateRunRate(int32 TotalBallsBowled);
};

USTRUCT(BlueprintType)
struct FBatterStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Runs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 BallsFaced = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Fours = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Sixes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float StrikeRate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    bool bIsOut = false;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    FString DismissalType;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    FString BowlerName;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    FString FielderName;
};

USTRUCT(BlueprintType)
struct FBowlerStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    FString PlayerName;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float OversBowled = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Maidens = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 RunsConceded = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 WicketsTaken = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float EconomyRate = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 Wides = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 NoBalls = 0;
};

USTRUCT(BlueprintType)
struct FBallByBallEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    int32 OverNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    int32 BallNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString BowlerName;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString BatterName;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString NonStrikerName;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    int32 RunsScored = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    bool bIsWicket = false;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    bool bIsExtra = false;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString ExtraType;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString WicketType;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FString ShotType;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FVector BallPitchLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
    FVector BallContactLocation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScoreUpdated, int32, TeamIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWicketFallen, int32, WicketsDown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverCompleted, int32, OverNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInningsStarted, int32, InningsNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInningsEnded, int32, InningsNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchEnded, bool, bTeamAWon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNewDelivery, int32, Over, int32, Ball);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallInPlay, ACricketBall*, Ball);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, TEnumAsByte<EMatchState>, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInningsStateChanged, TEnumAsByte<EInningsState>, NewState);

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API ACricketGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ACricketGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Score management
    UFUNCTION(BlueprintCallable, Category = "Score")
    void AddRuns(int32 Runs, bool bIsExtra = false);

    UFUNCTION(BlueprintCallable, Category = "Score")
    void AddWicket();

    UFUNCTION(BlueprintCallable, Category = "Score")
    void AddExtra(int32 Runs, FGameplayTag ExtraType);

    UFUNCTION(BlueprintCallable, Category = "Score")
    void RecordBall(int32 Runs, bool bIsWicket, bool bIsExtra, FGameplayTag ExtraType, FGameplayTag WicketType, FGameplayTag ShotType);

    UFUNCTION(BlueprintCallable, Category = "Score")
    void RecordDelivery(const FBallByBallEntry& Entry);

    // Getters
    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetTeamARuns() const { return TeamAScore.Runs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetTeamBRuns() const { return TeamBScore.Runs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetTeamAWickets() const { return TeamAScore.Wickets; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetTeamBWickets() const { return TeamBScore.Wickets; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetTeamAOvers() const { return TeamAScore.Overs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetTeamBOvers() const { return TeamBScore.Overs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetTeamARunRate() const { return TeamAScore.RunRate; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetTeamBRunRate() const { return TeamBScore.RunRate; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetWicketsDown() const { return bTeamABatting ? TeamAScore.Wickets : TeamBScore.Wickets; }

    UFUNCTION(BlueprintPure, Category = "Score")
    int32 GetCurrentRuns() const { return bTeamABatting ? TeamAScore.Runs : TeamBScore.Runs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetCurrentOvers() const { return bTeamABatting ? TeamAScore.Overs : TeamBScore.Overs; }

    UFUNCTION(BlueprintPure, Category = "Score")
    float GetRequiredRunRate(int32 Target, int32 OversRemaining) const;

    UFUNCTION(BlueprintPure, Category = "Score")
    const TArray<FBallByBallEntry>& GetBallByBallLog() const { return BallByBallLog; }

    UFUNCTION(BlueprintPure, Category = "Score")
    const TArray<FBatterStats>& GetCurrentBattingOrder() const { return bTeamABatting ? TeamABattingOrder : TeamBBattingOrder; }

    UFUNCTION(BlueprintPure, Category = "Score")
    const TArray<FBowlerStats>& GetCurrentBowlingFigures() const { return bTeamABatting ? TeamBBowlingFigures : TeamABowlingFigures; }

    // Batting order management
    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetTeamABattingOrder(const TArray<FBatterStats>& Order);

    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetTeamBBattingOrder(const TArray<FBatterStats>& Order);

    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetBattingTeam(bool bIsTeamA);

    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetCurrentBatters(int32 StrikerIndex, int32 NonStrikerIndex);

    UFUNCTION(BlueprintCallable, Category = "Setup")
    void SetCurrentBowler(int32 BowlerIndex);

    // Match events
    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnInningsStarted(int32 InningsNumber, bool bTeamABatsFirst);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnInningsEnded(int32 InningsNumber);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnMatchEnded();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnNewDelivery(int32 Over, int32 Ball);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnBallInPlay(ACricketBall* Ball);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnMatchStateChanged(TEnumAsByte<EMatchState> NewState);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnInningsStateChanged(TEnumAsByte<EInningsState> NewState);

    // Current match state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentStrikerIndex = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentNonStrikerIndex = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentBowlerIndex = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    bool bTeamABatting = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 TargetScore = 0;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnScoreUpdated OnScoreUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWicketFallen OnWicketFallen;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOverCompleted OnOverCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInningsStarted OnInningsStartedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInningsEnded OnInningsEndedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMatchEnded OnMatchEndedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNewDelivery OnNewDeliveryEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBallInPlay OnBallInPlayEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMatchStateChanged OnMatchStateChangedEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInningsStateChanged OnInningsStateChangedEvent;

protected:
    // Score tracking
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
    FTeamScore TeamAScore;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
    FTeamScore TeamBScore;

    // Batting/Bowling stats
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    TArray<FBatterStats> TeamABattingOrder;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    TArray<FBatterStats> TeamBBattingOrder;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    TArray<FBowlerStats> TeamABowlingFigures;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    TArray<FBowlerStats> TeamBBowlingFigures;

    // Ball-by-ball log
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scoreboard")
    TArray<FBallByBallEntry> BallByBallLog;

    // Current partnership
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Partnership")
    int32 PartnershipRuns = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Partnership")
    int32 PartnershipBalls = 0;

    // DRS
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS")
    int32 TeamADRSReviewsRemaining = 2;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS")
    int32 TeamBDRSReviewsRemaining = 2;
};