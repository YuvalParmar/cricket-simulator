#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "BallPhysics/CricketBall.h"
#include "GameModes/CricketDataAssets.h"
#include "GameModes/CricketEnums.h"
#include "CricketGameplayAbilities.generated.h"

// Forward declarations - break circular dependencies
class ACricketBattingPawn;
class ACricketBowlingPawn;
class ACricketFieldingPawn;
class UPitchActor;
class UCricketFieldingComponent;

// ============================================================================
// CRICKET ENUMS (shared across gameplay)
// ============================================================================


// ============================================================================
// GAMEPLAY TAGS
// ============================================================================

// Shot Types
// GameplayTags.Cricket.Shot.ForwardDefense
// GameplayTags.Cricket.Shot.CoverDrive
// GameplayTags.Cricket.Shot.PullShot
// GameplayTags.Cricket.Shot.SweepShot
// GameplayTags.Cricket.Shot.LoftedDrive
// GameplayTags.Cricket.Shot.RampShot
// etc.

// Delivery Types
// GameplayTags.Cricket.Delivery.InSwinger
// GameplayTags.Cricket.Delivery.OutSwinger
// GameplayTags.Cricket.Delivery.Yorker
// GameplayTags.Cricket.Delivery.OffBreak
// GameplayTags.Cricket.Delivery.LegBreak
// GameplayTags.Cricket.Delivery.Googly
// etc.

// Fielding Actions
// GameplayTags.Cricket.Fielding.Dive
// GameplayTags.Cricket.Fielding.Catch
// GameplayTags.Cricket.Fielding.Throw
// GameplayTags.Cricket.Fielding.RunOut
// GameplayTags.Cricket.Fielding.Stumping

// States
// GameplayTags.Cricket.State.Batting
// GameplayTags.Cricket.State.Bowling
// GameplayTags.Cricket.State.Fielding
// GameplayTags.Cricket.State.BallInPlay
// GameplayTags.Cricket.State.BallDead
// GameplayTags.Cricket.State.DRSReview

// Events
// GameplayTags.Cricket.Event.BallBowled
// GameplayTags.Cricket.Event.BallHitBat
// GameplayTags.Cricket.Event.BallHitPad
// GameplayTags.Cricket.Event.RunsScored
// GameplayTags.Cricket.Event.WicketFallen
// GameplayTags.Cricket.Event.Boundary4
// GameplayTags.Cricket.Event.Boundary6

// ============================================================================
// BASE CRICKET ABILITY
// ============================================================================

// Input state structs for abilities (prefixed to avoid conflicts with IBatInputInterface)
USTRUCT(BlueprintType)
struct FShotInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float FootworkX = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float FootworkY = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float BatAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float WristRotation = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float TimingInput = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    bool bTimingTriggered = false;

    UPROPERTY(BlueprintReadWrite, Category = "Shot Input")
    float InputTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FCricketBowlingInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float RunupRhythm = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float AimX = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float AimY = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float SeamAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float SpinAxis = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    float Effort = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    int32 CurrentStep = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Bowling Input")
    bool bReleaseTriggered = false;
};

USTRUCT(BlueprintType)
struct FCricketFieldingInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    FVector2D MoveInput = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    bool bDiveTriggered = false;

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    FVector DiveDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    bool bThrowTriggered = false;

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    FVector ThrowTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Fielding Input")
    float ThrowPower = 0.0f;
};

UCLASS(Abstract, Blueprintable)
class CRICKETGAME_API UCricketGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketGameplayAbility();

    // Cricket-specific ability activation
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

    // Called when ability activates
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    // Called when ability ends
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

    // Cricket-specific tags
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    FGameplayTag CricketAbilityTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    FGameplayTagContainer RequiredStateTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    FGameplayTagContainer BlockedByTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    FGameplayTagContainer CricketActivationOwnedTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    float AbilityCooldown = 0.0f;

    // Input binding for this ability
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket")
    FName InputActionName;

protected:
    // Helper to get cricket pawn
    ACricketBattingPawn* GetBattingPawn(const FGameplayAbilityActorInfo* ActorInfo) const;
    ACricketBowlingPawn* GetBowlingPawn(const FGameplayAbilityActorInfo* ActorInfo) const;
    ACricketFieldingPawn* GetFieldingPawn(const FGameplayAbilityActorInfo* ActorInfo) const;
};

// ============================================================================
// BATTING ABILITIES
// ============================================================================

// Base class for all shot abilities
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketShotAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketShotAbility();

    // Shot configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Config")
    EShotType ShotType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Config")
    FGameplayTag ShotGameplayTag;

    // Footwork requirement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    bool bRequiresFrontFoot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    bool bRequiresBackFoot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    FVector2D FootworkOffset = FVector2D::ZeroVector;

    // Bat angle configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat Angle")
    float BatFaceAngle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat Angle")
    float WristRotation = 0.0f;

    // Timing windows (relative to perfect contact)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float PerfectWindowMs = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float GoodWindowMs = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float EarlyWindowMs = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float LateWindowMs = 100.0f;

    // Outcome modifiers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float PowerMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float ControlMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float PlacementSpreadDegrees = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float EdgeProbability = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float MissProbability = 0.1f;

    // Animation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* ShotMontage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    float MontagePlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FName StartSectionName;

    // Suitable deliveries
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suitability")
    TArray<TEnumAsByte<EDeliveryType>> SuitableDeliveries;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suitability")
    TArray<TEnumAsByte<EDeliveryType>> UnsuitableDeliveries;

    // Ability flow
    virtual void OnShotTriggered(const FGameplayEventData* EventData);
    virtual void OnContactPredicted(float TimeToContact, const FVector& PredictedContactPoint);
    virtual void OnContactMade(const FVector& ContactPoint, const FVector& BallVelocity, float TimingQuality);
    virtual void OnShotComplete(EShotOutcome Outcome, const FVector& BallDirection, float Power, float Placement);

    // Input handling
    UFUNCTION(BlueprintCallable, Category = "Shot Ability")
    void SetShotInput(float FootworkX, float FootworkY, float BatAngle, float InWristRotation, float TimingInput);

protected:
    // Current shot input state
    UPROPERTY()
    FShotInputState CurrentShotInput;
};

// Specific shot abilities
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketDefensiveShotAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketDefensiveShotAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketDriveAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketDriveAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketHorizontalBatAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketHorizontalBatAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketSweepAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketSweepAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketLoftedShotAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketLoftedShotAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketSpecialShotAbility : public UCricketShotAbility
{
    GENERATED_BODY()
public:
    UCricketSpecialShotAbility();
};

// Footwork ability (can be used independently or as part of shot)
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketFootworkAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketFootworkAbility();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    EFootworkType FootworkType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    FVector2D TargetOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    float MovementSpeed = 3.0f;

    UFUNCTION(BlueprintCallable, Category = "Footwork")
    void SetFootworkDirection(const FVector2D& Direction);
};

// ============================================================================
// BOWLING ABILITIES
// ============================================================================

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketBowlingAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketBowlingAbility();

    // Delivery type
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    TEnumAsByte<EDeliveryType> DeliveryType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    FGameplayTag DeliveryGameplayTag;

    // Run-up
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    float RunupSpeed = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    int32 RunupSteps = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    float RhythmTolerance = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    UCurveFloat* RunupSpeedCurve;

    // Release parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release")
    float ReleaseHeight = 2.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release")
    float ReleaseAngle = -5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release")
    float WristPosition = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Release")
    float SeamOrientation = 0.0f;

    // Physics
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float PaceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SwingAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SeamMovement = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SpinRPM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector SpinAxis = FVector(0.0f, 0.0f, 1.0f);

    // Control
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float ControlDifficulty = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float StaminaCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float AccuracyRadius = 0.3f;

    // Visual/Audio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* TrailEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* ReleaseSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* FlightSound;

    // Ability flow
    virtual void OnRunupStarted();
    virtual void OnRunupStep(int32 StepNumber, float TimingQuality);
    virtual void OnRunupCompleted(float RhythmQuality);
    virtual void OnReleasePoint(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, float Effort);
    virtual void OnBallReleased(ACricketBall* Ball);

    // Input handling
    UFUNCTION(BlueprintCallable, Category = "Bowling Ability")
    void SetBowlingInput(float RunupRhythm, float AimX, float AimY, float SeamAngle, float InSpinAxis, float Effort);

protected:
    UPROPERTY()
    FCricketBowlingInputState CurrentBowlingInput;
};

// Specific delivery abilities
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketPaceDeliveryAbility : public UCricketBowlingAbility
{
    GENERATED_BODY()
public:
    UCricketPaceDeliveryAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketSwingDeliveryAbility : public UCricketBowlingAbility
{
    GENERATED_BODY()
public:
    UCricketSwingDeliveryAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketSpinDeliveryAbility : public UCricketBowlingAbility
{
    GENERATED_BODY()
public:
    UCricketSpinDeliveryAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketVariationDeliveryAbility : public UCricketBowlingAbility
{
    GENERATED_BODY()
public:
    UCricketVariationDeliveryAbility();
};

// Run-up rhythm ability
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketRunupRhythmAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketRunupRhythmAbility();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
    int32 TotalSteps = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
    float StepInterval = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
    float PerfectWindow = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm")
    float GoodWindow = 0.1f;

    UFUNCTION(BlueprintCallable, Category = "Rhythm")
    void OnStepInput(float Timing);

    UFUNCTION(BlueprintCallable, Category = "Rhythm")
    float GetRhythmQuality() const;

protected:
    UPROPERTY()
    TArray<float> StepTimings;

    UPROPERTY()
    int32 CurrentStep = 0;
};

// ============================================================================
// FIELDING ABILITIES
// ============================================================================

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketFieldingAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketFieldingAbility();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    TEnumAsByte<EFieldingActionType> ActionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    FGameplayTag ActionGameplayTag;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MoveSpeed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float Acceleration = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float TurnRate = 360.0f;

    // Interception
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interception")
    float PredictionTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interception")
    float InterceptionRadius = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interception")
    float ReactionTime = 0.2f;

    // Dive
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dive")
    float DiveReach = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dive")
    float DiveCooldown = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dive")
    UAnimMontage* DiveMontage;

    // Throw
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float MaxThrowSpeed = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float ThrowAccuracyBase = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float ThrowChargeTime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    UAnimMontage* ThrowMontage;

    // Catch
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catch")
    float CatchRadius = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catch")
    float CatchProbabilityBase = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catch")
    UAnimMontage* CatchMontage;

    // Ability flow
    virtual void OnBallInPlay(ACricketBall* Ball, const FVector& PredictedLanding);
    virtual void OnInterceptionPointCalculated(const FVector& InterceptionPoint, float TimeToArrive);
    virtual void OnMovementStarted(const FVector& TargetLocation);
    virtual void OnDiveTriggered(const FVector& DiveDirection);
    virtual void OnThrowTriggered(const FVector& TargetLocation, float Power);
    virtual void OnCatchAttempt(const FVector& BallLocation, const FVector& BallVelocity);
    virtual void OnFieldingActionComplete(EFieldingOutcome Outcome);

    // Input handling
    UFUNCTION(BlueprintCallable, Category = "Fielding Ability")
    void SetFieldingInput(const FVector2D& MoveInput, bool bDive, bool bThrow, const FVector& ThrowTarget, float ThrowPower);

    UFUNCTION(BlueprintPure, Category = "Fielding Ability")
    EFieldingActionType GetFieldingActionType() const { return ActionType; }

    UFUNCTION(BlueprintCallable, Category = "Fielding Ability")
    void SetEffectivenessModifier(float Modifier);

protected:
    UPROPERTY(BlueprintReadOnly, Category = "Fielding Ability")
    float EffectivenessModifier = 1.0f;

    UPROPERTY()
    FCricketFieldingInputState CurrentFieldingInput;
};

// Specific fielding abilities
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketInterceptionAbility : public UCricketFieldingAbility
{
    GENERATED_BODY()
public:
    UCricketInterceptionAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketDiveAbility : public UCricketFieldingAbility
{
    GENERATED_BODY()
public:
    UCricketDiveAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketThrowAbility : public UCricketFieldingAbility
{
    GENERATED_BODY()
public:
    UCricketThrowAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketCatchAbility : public UCricketFieldingAbility
{
    GENERATED_BODY()
public:
    UCricketCatchAbility();
};

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketRunOutAbility : public UCricketFieldingAbility
{
    GENERATED_BODY()
public:
    UCricketRunOutAbility();
};

// Wicketkeeping specific abilities
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketWicketkeepingAbility : public UCricketGameplayAbility
{
    GENERATED_BODY()

public:
    UCricketWicketkeepingAbility();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    TEnumAsByte<EWicketkeeperStance> Stance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    float StanceHeight = 1.0f; // 0=standing, 1=crouching

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    float CollectionRadius = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    float StumpingSpeed = 0.15f; // Time to break stumps

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    float StumpingAccuracy = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    float ByesPredictionRadius = 1.0f;

    // Ability flow
    virtual void OnStanceChanged(EWicketkeeperStance NewStance);
    virtual void OnBallApproaching(const FVector& BallLocation, const FVector& BallVelocity, float TimeToArrive);
    virtual void OnCollectionAttempt(const FVector& BallLocation);
    virtual void OnStumpingAttempt(const FVector& BallLocation, bool bBatterGrounded);
    virtual void OnByesConceded(int32 Runs);

    UFUNCTION(BlueprintCallable, Category = "Wicketkeeping")
    void SetWicketkeepingInput(float InStanceHeight, bool bMoveToBall, bool bStumpingAttempt);
};

// ============================================================================
// GAMEPLAY EFFECTS
// ============================================================================

// Stamina cost effect
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketStaminaCostEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCricketStaminaCostEffect();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
    float StaminaCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
    bool bScaleWithEffort = true;
};

// Pressure effect (affects timing windows)
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketPressureEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCricketPressureEffect();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure")
    float PressureLevel = 0.0f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure")
    float TimingWindowReduction = 0.0f; // Percentage

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure")
    float ControlReduction = 0.0f; // Percentage
};

// Form/Confidence effect
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketFormEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCricketFormEffect();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    float FormLevel = 0.0f; // -1 to 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    float TimingWindowBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    float PowerBonus = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Form")
    float ControlBonus = 0.0f;
};

// Fatigue effect
UCLASS(Blueprintable)
class CRICKETGAME_API UCricketFatigueEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UCricketFatigueEffect();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fatigue")
    float FatigueLevel = 0.0f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fatigue")
    float PaceReduction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fatigue")
    float AccuracyReduction = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fatigue")
    float ReactionTimeIncrease = 0.0f;
};

// ============================================================================
// ATTRIBUTE ENUMS (moved before ASC to avoid circular dependency)
// ============================================================================

UENUM(BlueprintType)
enum class EBattingAttribute : uint8
{
    Technique,
    Power,
    Timing,
    Footwork,
    Temperament,
    Concentration
};

UENUM(BlueprintType)
enum class EBowlingAttribute : uint8
{
    Pace,
    Swing,
    Seam,
    Spin,
    Control,
    Stamina,
    Variations
};

UENUM(BlueprintType)
enum class EFieldingAttribute : uint8
{
    Agility,
    Reflexes,
    Throwing,
    Catching,
    Anticipation
};

// ============================================================================
// ABILITY SYSTEM COMPONENT EXTENSION
// ============================================================================

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETGAME_API UCricketAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UCricketAbilitySystemComponent();

    // Cricket-specific attribute access
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetBattingAttribute(EBattingAttribute Attribute) const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetBowlingAttribute(EBowlingAttribute Attribute) const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetFieldingAttribute(EFieldingAttribute Attribute) const;

    // Stamina management
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetCurrentStamina() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetMaxStamina() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void ConsumeStamina(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void RegenerateStamina(float DeltaTime);

    // Pressure/form
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetPressureLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void SetPressureLevel(float Level);

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetFormLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void UpdateForm(float PerformanceDelta);

    // Fatigue
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    float GetFatigueLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void AddFatigue(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void RecoverFatigue(float DeltaTime);

    // Ability granting
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void GrantBattingAbilities(const TArray<TSubclassOf<UCricketShotAbility>>& Abilities);

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void GrantBowlingAbilities(const TArray<TSubclassOf<UCricketBowlingAbility>>& Abilities);

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    void GrantFieldingAbilities(const TArray<TSubclassOf<UCricketFieldingAbility>>& Abilities);

    // Active ability queries
    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    UCricketShotAbility* GetActiveShotAbility() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    UCricketBowlingAbility* GetActiveBowlingAbility() const;

    UFUNCTION(BlueprintCallable, Category = "Cricket ASC")
    UCricketFieldingAbility* GetActiveFieldingAbility() const;

protected:
    // Attributes (would be defined in AttributeSet)
    UPROPERTY()
    class UCricketAttributeSet* CricketAttributes;
};

// ============================================================================
// ATTRIBUTE SET
// ============================================================================

UCLASS(Blueprintable)
class CRICKETGAME_API UCricketAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UCricketAttributeSet();

    // Batting Attributes
    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Technique)
    FGameplayAttributeData Technique;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Technique)

    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Power)
    FGameplayAttributeData Power;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Power)

    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Timing)
    FGameplayAttributeData Timing;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Timing)

    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Footwork)
    FGameplayAttributeData Footwork;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Footwork)

    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Temperament)
    FGameplayAttributeData Temperament;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Temperament)

    UPROPERTY(BlueprintReadOnly, Category = "Batting", ReplicatedUsing = OnRep_Concentration)
    FGameplayAttributeData Concentration;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Concentration)

    // Bowling Attributes
    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingPace)
    FGameplayAttributeData BowlingPace;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingPace)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingSwing)
    FGameplayAttributeData BowlingSwing;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingSwing)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingSeam)
    FGameplayAttributeData BowlingSeam;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingSeam)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingSpin)
    FGameplayAttributeData BowlingSpin;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingSpin)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingControl)
    FGameplayAttributeData BowlingControl;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingControl)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingStamina)
    FGameplayAttributeData BowlingStamina;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Bowling", ReplicatedUsing = OnRep_BowlingVariations)
    FGameplayAttributeData BowlingVariations;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, BowlingVariations)

    // Fielding Attributes
    UPROPERTY(BlueprintReadOnly, Category = "Fielding", ReplicatedUsing = OnRep_Agility)
    FGameplayAttributeData Agility;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Agility)

    UPROPERTY(BlueprintReadOnly, Category = "Fielding", ReplicatedUsing = OnRep_Reflexes)
    FGameplayAttributeData Reflexes;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Reflexes)

    UPROPERTY(BlueprintReadOnly, Category = "Fielding", ReplicatedUsing = OnRep_Throwing)
    FGameplayAttributeData Throwing;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Throwing)

    UPROPERTY(BlueprintReadOnly, Category = "Fielding", ReplicatedUsing = OnRep_Catching)
    FGameplayAttributeData Catching;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Catching)

    UPROPERTY(BlueprintReadOnly, Category = "Fielding", ReplicatedUsing = OnRep_Anticipation)
    FGameplayAttributeData Anticipation;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, Anticipation)

    // Dynamic Attributes
    UPROPERTY(BlueprintReadOnly, Category = "Dynamic", ReplicatedUsing = OnRep_CurrentStamina)
    FGameplayAttributeData CurrentStamina;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, CurrentStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Dynamic", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, MaxStamina)

    UPROPERTY(BlueprintReadOnly, Category = "Dynamic", ReplicatedUsing = OnRep_PressureLevel)
    FGameplayAttributeData PressureLevel;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, PressureLevel)

    UPROPERTY(BlueprintReadOnly, Category = "Dynamic", ReplicatedUsing = OnRep_FormLevel)
    FGameplayAttributeData FormLevel;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, FormLevel)

    UPROPERTY(BlueprintReadOnly, Category = "Dynamic", ReplicatedUsing = OnRep_FatigueLevel)
    FGameplayAttributeData FatigueLevel;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, FatigueLevel)

    // Wicketkeeping
    UPROPERTY(BlueprintReadOnly, Category = "Wicketkeeping", ReplicatedUsing = OnRep_WKKeeping)
    FGameplayAttributeData WKKeeping;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, WKKeeping)

    UPROPERTY(BlueprintReadOnly, Category = "Wicketkeeping", ReplicatedUsing = OnRep_WKStumping)
    FGameplayAttributeData WKStumping;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, WKStumping)

    UPROPERTY(BlueprintReadOnly, Category = "Wicketkeeping", ReplicatedUsing = OnRep_WKReactions)
    FGameplayAttributeData WKReactions;
    ATTRIBUTE_ACCESSORS(UCricketAttributeSet, WKReactions)

protected:
    // Replication callbacks
    UFUNCTION()
    virtual void OnRep_Technique(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Power(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Timing(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Footwork(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Temperament(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Concentration(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_BowlingPace(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingSwing(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingSeam(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingSpin(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingControl(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingStamina(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_BowlingVariations(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_Agility(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Reflexes(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Throwing(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Catching(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_Anticipation(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_CurrentStamina(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_PressureLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_FormLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_FatigueLevel(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_WKKeeping(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_WKStumping(const FGameplayAttributeData& OldValue);
    UFUNCTION()
    virtual void OnRep_WKReactions(const FGameplayAttributeData& OldValue);
};