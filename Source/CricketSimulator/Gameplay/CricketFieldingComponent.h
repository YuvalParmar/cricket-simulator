#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CricketFieldingPawn.h"
#include "CricketBall.h"
#include "CricketFieldingComponent.generated.h"

class ACricketFieldingPawn;
class ACricketBall;
class ACricketGameMode;

USTRUCT(BlueprintType)
struct FFieldingAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Speed = 1.0f; // 0-2, movement speed

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Agility = 1.0f; // 0-2, turning, diving

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Reactions = 1.0f; // 0-2, response time

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Catching = 1.0f; // 0-2, catch success rate

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Throwing = 1.0f; // 0-2, throw accuracy/power

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Stamina = 1.0f; // 0-2, fatigue resistance

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Anticipation = 1.0f; // 0-2, reading the shot
};

USTRUCT(BlueprintType)
struct FInterceptionPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    FVector Location;

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    float TimeToReach = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    float BallTimeToArrive = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    bool bCanIntercept = false;

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    bool bRequiresDive = false;

    UPROPERTY(BlueprintReadOnly, Category = "Interception")
    float Confidence = 0.0f; // 0-1
};

USTRUCT(BlueprintType)
struct FThrowResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bSuccessful = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector TargetLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ActualLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float Accuracy = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float Power = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float TimeToTarget = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bDirectHit = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bOverthrow = false;
};

USTRUCT(BlueprintType)
struct FCatchResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bSuccessful = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float CatchQuality = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bDropped = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FGameplayTag DropReason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInterceptionCalculated, const FInterceptionPoint&, Point);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowExecuted, const FThrowResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCatchAttempted, const FCatchResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunOutAttempt, bool, bSuccessful);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UCricketFieldingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCricketFieldingComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Setup
    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetFieldingPawn(ACricketFieldingPawn* Pawn);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetTargetBall(ACricketBall* Ball);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetGameMode(ACricketGameMode* GameMode);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetFieldingPosition(EFieldingPosition Position);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetAttributes(const FFieldingAttributes& NewAttributes);

    // AI Control
    UFUNCTION(BlueprintCallable, Category = "Fielding|AI")
    void EnableAIControl(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Fielding|AI")
    void SetPriorityTarget(const FVector& Location, float Priority);

    // Player Actions
    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void MoveToLocation(const FVector& Location, bool bSprint = false);

    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void Dive(const FVector2D& Direction);

    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void ThrowToLocation(const FVector& TargetLocation, float Power = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void AttemptCatch();

    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void AttemptRunOut(const FVector& StumpLocation);

    UFUNCTION(BlueprintCallable, Category = "Fielding|Actions")
    void AttemptStumping();

    // Analysis
    UFUNCTION(BlueprintCallable, Category = "Fielding|Analysis")
    FInterceptionPoint CalculateInterceptionPoint() const;

    UFUNCTION(BlueprintPure, Category = "Fielding")
    bool CanReachBall(float TimeToBall) const;

    UFUNCTION(BlueprintPure, Category = "Fielding")
    float GetDistanceToBall() const;

    UFUNCTION(BlueprintPure, Category = "Fielding")
    EFieldingPosition GetFieldingPosition() const { return FieldingPosition; }

    UFUNCTION(BlueprintPure, Category = "Fielding")
    EFieldingAction GetCurrentAction() const { return CurrentAction; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInterceptionCalculated OnInterceptionCalculated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnThrowExecuted OnThrowExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCatchAttempted OnCatchAttempted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunOutAttempt OnRunOutAttempt;

protected:
    // References
    UPROPERTY()
    TObjectPtr<ACricketFieldingPawn> FieldingPawn;

    UPROPERTY()
    TObjectPtr<ACricketBall> TargetBall;

    UPROPERTY()
    TObjectPtr<ACricketGameMode> GameMode;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding Setup")
    FFieldingAttributes Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding Setup")
    TObjectPtr<UDataTable> PositionLocationsTable;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    EFieldingPosition FieldingPosition = EFieldingPosition::Cover;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    EFieldingAction CurrentAction = EFieldingAction::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FVector HomePosition;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    bool bAIControlled = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FVector PriorityTargetLocation;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float PriorityTargetWeight = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float CurrentStamina = 100.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float MaxStamina = 100.0f;

    // Movement
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    FVector2D MovementInput = FVector2D::ZeroVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bWantsToSprint = false;

    // Diving
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Diving")
    bool bIsDiving = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Diving")
    FVector DiveDirection;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Diving")
    float DiveTimeRemaining = 0.0f;

    // Throwing
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Throwing")
    bool bIsThrowing = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Throwing")
    FVector ThrowTarget;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Throwing")
    float ThrowPower = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Throwing")
    float ThrowChargeTime = 0.0f;

    // Catching
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Catching")
    bool bCatchAttempted = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Catching")
    float CatchWindowStart = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Catching")
    float CatchWindowDuration = 0.3f;

    // Internal functions
    void UpdateAI(float DeltaTime);
    void UpdateMovement(float DeltaTime);
    void UpdateDive(float DeltaTime);
    void UpdateThrow(float DeltaTime);
    void UpdateCatch(float DeltaTime);
    void UpdateStamina(float DeltaTime);
    FInterceptionPoint CalculateBallInterception() const;
    FVector GetPositionCoordinates(EFieldingPosition Position) const;
    FThrowResult CalculateThrowResult() const;
    FCatchResult CalculateCatchResult() const;
    void SetAction(EFieldingAction NewAction);
    float CalculateReactionTime() const;
    float CalculateMaxRunDistance(float TimeAvailable) const;
};