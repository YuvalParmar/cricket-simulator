#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTags.h"
#include "CricketGameMode.generated.h"

class ACricketGameState;
class ACricketPlayerController;
class ACricketBall;
class APitchActor;

UENUM(BlueprintType)
enum class EMatchState : uint8
{
    PreMatch        UMETA(DisplayName = "Pre-Match"),
    Toss            UMETA(DisplayName = "Toss"),
    Innings1        UMETA(DisplayName = "First Innings"),
    InningsBreak    UMETA(DisplayName = "Innings Break"),
    Innings2        UMETA(DisplayName = "Second Innings"),
    MatchEnd        UMETA(DisplayName = "Match End"),
    Paused          UMETA(DisplayName = "Paused"),
    DRSReview       UMETA(DisplayName = "DRS Review")
};

UENUM(BlueprintType)
enum class EInningsState : uint8
{
    NotStarted      UMETA(DisplayName = "Not Started"),
    InProgress      UMETA(DisplayName = "In Progress"),
    OverComplete    UMETA(DisplayName = "Over Complete"),
    InningsComplete UMETA(DisplayName = "Innings Complete"),
    Declared        UMETA(DisplayName = "Declared"),
    FollowOn        UMETA(DisplayName = "Follow On")
};

USTRUCT(BlueprintType)
struct FMatchConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    int32 OversPerInnings = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    int32 MaxPlayersPerTeam = 11;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    bool bAllowDRS = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    int32 DRSReviewsPerInnings = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    FString PitchType = "Standard";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    FString WeatherCondition = "Clear";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    float MatchStartTimeOfDay = 14.0f; // 2 PM

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match")
    bool bUseDuckworthLewis = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, EMatchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInningsStateChanged, EInningsState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWicketFallen, int32, WicketsDown, FString, BatterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunsScored, int32, Runs, FString, BatterName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOverComplete, int32, OverNumber);

UCLASS(Blueprintable)
class CRICKETGAME_API ACricketGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACricketGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Match state management
    UFUNCTION(BlueprintCallable, Category = "Match")
    void StartMatch();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void StartToss();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void OnTossComplete(bool bTeamABatsFirst);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void StartInnings(int32 InningsNumber);

    UFUNCTION(BlueprintCallable, Category = "Match")
    void EndInnings();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void EndMatch();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void PauseMatch();

    UFUNCTION(BlueprintCallable, Category = "Match")
    void ResumeMatch();

    // Ball delivery cycle
    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void StartNewDelivery();

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void OnBallDelivered(ACricketBall* Ball);

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void OnBallDead(int32 RunsScored, bool bIsWicket, bool bIsExtra);

    UFUNCTION(BlueprintCallable, Category = "Gameplay")
    void HandleOverComplete();

    // DRS System
    UFUNCTION(BlueprintCallable, Category = "DRS")
    void RequestDRSReview(AActor* RequestingPlayer, FGameplayTag ReviewType);

    UFUNCTION(BlueprintCallable, Category = "DRS")
    void CompleteDRSReview(bool bOriginalDecisionOverturned);

    // Getters
    UFUNCTION(BlueprintPure, Category = "Match")
    EMatchState GetMatchState() const { return MatchState; }

    UFUNCTION(BlueprintPure, Category = "Match")
    EInningsState GetInningsState() const { return InningsState; }

    UFUNCTION(BlueprintPure, Category = "Match")
    int32 GetCurrentInnings() const { return CurrentInnings; }

    UFUNCTION(BlueprintPure, Category = "Match")
    int32 GetCurrentOver() const { return CurrentOver; }

    UFUNCTION(BlueprintPure, Category = "Match")
    int32 GetCurrentBall() const { return CurrentBall; }

    UFUNCTION(BlueprintPure, Category = "Match")
    const FMatchConfig& GetMatchConfig() const { return MatchConfig; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMatchStateChanged OnMatchStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInningsStateChanged OnInningsStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWicketFallen OnWicketFallen;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunsScored OnRunsScored;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOverComplete OnOverComplete;

protected:
    virtual void SetMatchState(EMatchState NewState);
    virtual void SetInningsState(EInningsState NewState);

    void UpdateMatchTimer(float DeltaSeconds);
    void CheckInningsEndConditions();
    void AdvanceOver();

    // Configuration
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Match")
    FMatchConfig MatchConfig;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    EMatchState MatchState = EMatchState::PreMatch;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    EInningsState InningsState = EInningsState::NotStarted;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentInnings = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentOver = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 CurrentBall = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 BallsInCurrentOver = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    bool bTeamABattingFirst = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    float MatchTimer = 0.0f;

    // References
    UPROPERTY()
    ACricketGameState* CricketGameState;

    UPROPERTY()
    APitchActor* PitchActor;

    UPROPERTY()
    ACricketBall* CurrentBallActor;
};