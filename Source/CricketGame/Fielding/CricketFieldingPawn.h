#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "AbilitySystemInterface.h"
#include "CricketDataAssets.h"
#include "CricketFieldingComponent.h"
#include "CricketGameplayAbilities.h"
#include "CricketFieldingPawn.generated.h"

class UCapsuleComponent;
class UCricketFieldingComponent;
class UCricketAbilitySystemComponent;
class UCricketAttributeSet;
class UCricketFieldingAbility;
class UCricketGameplayAbility;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFieldingActionChanged, EFieldingAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCatchAttempt, bool, bSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowCompleted, const FThrowResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBallIntercepted, FVector, Location, float, TimeToReach);

UCLASS(Blueprintable)
class CRICKETGAME_API ACricketFieldingPawn : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACricketFieldingPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Positioning
    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetFieldingPosition(EFieldingPosition Position);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void MoveToPosition(const FVector& TargetLocation, bool bSprint = false);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void ReturnToPosition();

    // Actions
    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void SetMovementInput(const FVector2D& Direction);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnDive(const FVector2D& Direction);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnThrow(const FVector& TargetLocation, float Power);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void AttemptCatch();

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void AttemptRunOut(const FVector& StumpLocation);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void AttemptStumping();

    // AI Control (when not player-controlled)
    UFUNCTION(BlueprintCallable, Category = "Fielding|AI")
    void EnableAIControl(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Fielding|AI")
    void SetBallTarget(AActor* BallActor);

    UFUNCTION(BlueprintCallable, Category = "Fielding|AI")
    void SetPriorityTarget(const FVector& Location, float Priority);

    // GAS Functions
    UFUNCTION(BlueprintCallable, Category = "GAS")
    void InitializeAbilities();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateFieldingAbility(EFieldingActionType ActionType);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateWicketkeepingAbility();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void EndActiveFieldingAbility();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ApplyStaminaCost(float Cost);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void RegenerateStamina(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void SetPressureLevel(float Level);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void UpdateForm(float PerformanceDelta);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void AddFatigue(float Amount);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void RecoverFatigue(float DeltaTime);

    UFUNCTION(BlueprintPure, Category = "GAS")
    UCricketAbilitySystemComponent* GetCricketASC() const { return AbilitySystemComponent; }

    UFUNCTION(BlueprintPure, Category = "GAS")
    UCricketAttributeSet* GetCricketAttributeSet() const { return AttributeSet; }

    // Getters
    UFUNCTION(BlueprintPure, Category = "Fielding")
    EFieldingPosition GetFieldingPosition() const { return FieldingPosition; }

    UFUNCTION(BlueprintPure, Category = "Fielding")
    EFieldingAction GetCurrentAction() const { return CurrentAction; }

    UFUNCTION(BlueprintPure, Category = "Fielding")
    const FFieldingAttributes& GetAttributes() const { return Attributes; }

    UFUNCTION(BlueprintPure, Category = "Fielding")
    bool IsAIControlled() const { return bAIControlled; }

    UFUNCTION(BlueprintPure, Category = "Fielding")
    float GetDistanceToBall() const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFieldingActionChanged OnFieldingActionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCatchAttempt OnCatchAttempt;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnThrowCompleted OnThrowCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBallIntercepted OnBallIntercepted;

protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FieldingCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraSpringArm;

    // GAS Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketFieldingComponent> FieldingComponent;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding Setup")
    FFieldingAttributes Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding Setup")
    TObjectPtr<UDataTable> PositionLocationsTable; // Maps positions to coordinates

    // GAS Abilities
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketFieldingAbility>> AvailableFieldingAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TSubclassOf<UCricketWicketkeepingAbility> WicketkeepingAbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketGameplayAbility>> DefaultAbilities;

    // GAS State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketFieldingAbility* ActiveFieldingAbility = nullptr;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketWicketkeepingAbility* ActiveWicketkeepingAbility = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "GAS State")
    float CurrentStamina = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "GAS State")
    float MaxStamina = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "GAS State")
    float PressureLevel = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "GAS State")
    float FormLevel = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "GAS State")
    float FatigueLevel = 0.0f;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    EFieldingPosition FieldingPosition = EFieldingPosition::Cover;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    EFieldingAction CurrentAction = EFieldingAction::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    FVector HomePosition;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    bool bAIControlled = true;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    AActor* BallTarget = nullptr;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    FVector PriorityTargetLocation;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Fielding State")
    float PriorityTargetWeight = 0.0f;

    // Movement
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    FVector2D MovementInput = FVector2D::ZeroVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bWantsToSprint = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    bool bIsDiving = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
    FVector DiveDirection;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Movement")
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
    void UpdateFieldingAI(float DeltaSeconds);
    void UpdateMovement(float DeltaSeconds);
    void UpdateDive(float DeltaSeconds);
    void UpdateThrow(float DeltaSeconds);
    void UpdateCatch(float DeltaSeconds);
    void CalculateInterceptionPoint();
    FVector GetPositionCoordinates(EFieldingPosition Position) const;
    void SetAction(EFieldingAction NewAction);
    bool CanReachBall(float TimeToBall) const;
    FThrowResult CalculateThrowResult() const;
    void PlayFieldingAnimation(FName AnimationName, float PlayRate);

    // GAS Internal
    void InitializeAttributeSet();
    void GrantDefaultAbilities();
    UCricketFieldingAbility* FindFieldingAbility(EFieldingActionType ActionType) const;
    void OnFieldingAbilityActivated(UCricketFieldingAbility* Ability);
    void OnFieldingAbilityEnded(UCricketFieldingAbility* Ability);
    void OnFieldingAbilityComplete(UCricketFieldingAbility* Ability, EFieldingOutcome Outcome);
    void UpdateGASAttributes(float DeltaTime);
};