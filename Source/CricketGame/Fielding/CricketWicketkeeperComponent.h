#pragma once

#include "CoreMinimal.h"
#include "Core/CricketDataAssets.h"
#include "Gameplay/CricketFieldingComponent.h"
#include "CricketWicketkeeperComponent.generated.h"

class ACricketFieldingPawn;
class ACricketBall;
class ACricketGameMode;

USTRUCT(BlueprintType)
struct FWicketkeeperAttributes : public FFieldingAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeper")
    float StandingUp = 1.0f; // 0-2, ability to stand up to spinners

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeper")
    float StumpingSpeed = 1.0f; // 0-2, speed of stumping action

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeper")
    float Collection = 1.0f; // 0-2, clean collection of edges/misses

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeper")
    float Concentration = 1.0f; // 0-2, maintaining focus over long periods
};

USTRUCT(BlueprintType)
struct FStumpingResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bSuccessful = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float SpeedScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bBallCollectedCleanly = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FGameplayTag FailureReason;
};

USTRUCT(BlueprintType)
struct FByesResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 Byes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bWicketkeeperError = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector BallDeflection;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStumpingAttempt, const FStumpingResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnByesConceded, const FByesResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCollection, bool, bClean);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UCricketWicketkeeperComponent : public UCricketFieldingComponent
{
    GENERATED_BODY()

public:
    UCricketWicketkeeperComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Wicketkeeper specific setup
    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper")
    void SetWicketkeeperPawn(ACricketFieldingPawn* Pawn);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper")
    void SetStance(EWicketkeeperStance Stance);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper")
    void SetWicketkeeperAttributes(const FWicketkeeperAttributes& NewAttributes);

    // Actions
    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void CollectBall(const FVector& BallLocation, const FVector& BallVelocity);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void AttemptStumpingAt(const FVector& StumpLocation);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void AttemptRunOutStriker(const FVector& StumpLocation);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void AttemptRunOutNonStriker(const FVector& StumpLocation);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void MoveDownLegSide(float Distance);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Actions")
    void MoveDownOffSide(float Distance);

    // Analysis
    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Analysis")
    float CalculateStumpingChance() const;

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeper|Analysis")
    FByesResult PredictByes(const FVector& BallLocation, const FVector& BallVelocity) const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Wicketkeeper Events")
    FOnStumpingAttempt OnStumpingAttempt;

    UPROPERTY(BlueprintAssignable, Category = "Wicketkeeper Events")
    FOnByesConceded OnByesConceded;

    UPROPERTY(BlueprintAssignable, Category = "Wicketkeeper Events")
    FOnCollection OnCollection;

protected:
    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    EWicketkeeperStance CurrentStance = EWicketkeeperStance::Crouching;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    float StanceDistance = 200.0f; // cm from stumps

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    bool bBallCollected = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    FVector LastCollectionPoint;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    int32 ByesConceded = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    int32 Stumpings = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    int32 Catches = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Wicketkeeper State")
    int32 RunOuts = 0;

    // Wicketkeeper attributes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeper Setup")
    FWicketkeeperAttributes WKAttributes;

    // Internal functions
    void UpdateWicketkeeperAI(float DeltaTime);
    void UpdateStance(float DeltaTime);
    FStumpingResult ExecuteStumping(const FVector& StumpLocation);
    FByesResult CalculateByes(const FVector& BallLocation, const FVector& BallVelocity) const;
    bool IsBallCollectable(const FVector& BallLocation, const FVector& BallVelocity) const;
    FVector GetStumpLocation(bool bStrikerEnd) const;
    void ProcessMissedBall(const FVector& BallLocation, const FVector& BallVelocity);
    float CalculateReactionTime() const override;
};