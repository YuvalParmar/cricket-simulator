#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystemInterface.h"
#include "CricketDataAssets.h"
#include "CricketBowlingPawn.generated.h"

class UCapsuleComponent;
class UCricketBowlingComponent;
class UArrowComponent;
class UCricketAbilitySystemComponent;
class UCricketAttributeSet;
class UCricketBowlingAbility;
class UCricketRunupRhythmAbility;
class UCricketGameplayAbility;

UENUM(BlueprintType)
enum class EBowlingPhase : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    SettingField    UMETA(DisplayName = "Setting Field"),
    RunUpMarker     UMETA(DisplayName = "Run-Up Marker"),
    RunUp           UMETA(DisplayName = "Run-Up"),
    Release         UMETA(DisplayName = "Release"),
    FollowThrough   UMETA(DisplayName = "Follow Through"),
    Complete        UMETA(DisplayName = "Complete")
};

USTRUCT(BlueprintType)
struct FDeliveryDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    EDeliveryType DeliveryType = EDeliveryType::StockDelivery;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    FGameplayTag DeliveryTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    FName AnimationMontageName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float BaseSpeed = 130.0f; // km/h

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float MaxSpeed = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float SpinRPM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    FVector SpinAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float SwingAmount = 0.0f; // -1 to 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float SeamPosition = 0.0f; // -1 to 1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float ReleaseHeight = 2.2f; // meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float ReleaseAngle = 0.0f; // degrees

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float EffortCost = 0.1f; // Stamina cost per delivery

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
    float Difficulty = 0.5f; // 0-1, affects timing window
};

USTRUCT(BlueprintType)
struct FDeliveryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    EDeliveryType DeliveryType = EDeliveryType::StockDelivery;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ActualSpeed = 0.0f; // km/h

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ActualSpinRPM = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ActualSpinAxis = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ActualSwing = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ReleaseLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ReleaseVelocity;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ReleaseSpin;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector PredictedPitchLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float TimingScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float AccuracyScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bNoBall = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bWide = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeliveryBowled, const FDeliveryResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBowlingPhaseChanged, EBowlingPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRunUpProgress, float, Progress, FVector, CurrentLocation);

UCLASS(Blueprintable)
class CRICKETGAME_API ACricketBowlingPawn : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACricketBowlingPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Delivery setup
    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetRunUpMarker(const FVector& Location);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SelectDeliveryType(int32 DeliveryTypeIndex);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetAimOffset(const FVector2D& Offset);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetReleasePoint(float Height, float Angle);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetSeamOrientation(const FVector& Orientation);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetSpinAxis(const FVector& Axis, float RPM);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetEffort(float Effort);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnRunUpRhythmTap();

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnBowlButtonPressed();

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnBowlButtonReleased();

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void CancelDelivery();

    // GAS Functions
    UFUNCTION(BlueprintCallable, Category = "GAS")
    void InitializeAbilities();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateDeliveryAbility(EDeliveryType DeliveryType);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateRunupRhythmAbility();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void EndActiveDeliveryAbility();

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
    UFUNCTION(BlueprintPure, Category = "Bowling")
    EDeliveryType GetSelectedDeliveryType() const { return SelectedDeliveryType; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    EBowlingPhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    float GetRunUpProgress() const { return RunUpProgress; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    bool IsBowling() const { return CurrentPhase != EBowlingPhase::Idle && CurrentPhase != EBowlingPhase::SettingField; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    const FDeliveryResult& GetLastDeliveryResult() const { return LastDeliveryResult; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeliveryBowled OnDeliveryBowled;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBowlingPhaseChanged OnBowlingPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunUpProgress OnRunUpProgress;

protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> BodyMesh;

    // GAS Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> BowlingCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketBowlingComponent> BowlingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UArrowComponent> RunUpMarker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UArrowComponent> TargetMarker;

    // Delivery configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling Setup")
    TArray<FDeliveryDefinition> DeliveryDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling Setup")
    TObjectPtr<UDataTable> DeliveryDefinitionsTable;

    // GAS Abilities
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketBowlingAbility>> AvailableDeliveryAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TSubclassOf<UCricketRunupRhythmAbility> RunupRhythmAbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketGameplayAbility>> DefaultAbilities;

    // GAS State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketBowlingAbility* ActiveDeliveryAbility = nullptr;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketRunupRhythmAbility* ActiveRunupAbility = nullptr;

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

    // Current state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    EDeliveryType SelectedDeliveryType = EDeliveryType::StockDelivery;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    EBowlingPhase CurrentPhase = EBowlingPhase::SettingField;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    FVector RunUpMarkerLocation;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    FVector2D AimOffset = FVector2D::ZeroVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    float ReleaseHeight = 2.2f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    float ReleaseAngle = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    FVector SeamOrientation = FVector::ForwardVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    FVector SpinAxis = FVector::UpVector;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    float SpinRPM = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    float Effort = 0.5f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    bool bBowlButtonPressed = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Bowling State")
    float BowlButtonPressure = 0.0f;

    // Run-up state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    float RunUpProgress = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    float RunUpDistance = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    float TargetRunUpDistance = 20.0f; // meters

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    int32 RhythmTapCount = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    float RhythmTimingScore = 1.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Run Up")
    float LastRhythmTapTime = 0.0f;

    // Last delivery result
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Results")
    FDeliveryResult LastDeliveryResult;

    // Internal functions
    void UpdateBowlingState(float DeltaSeconds);
    void UpdateRunUp(float DeltaSeconds);
    void UpdateRelease(float DeltaSeconds);
    void ExecuteDelivery();
    void CalculateDeliveryResult(FDeliveryResult& OutResult);
    void PlayDeliveryAnimation(EDeliveryType DeliveryType, float PlayRate);
    void UpdateCamera(float DeltaSeconds);
    void CheckNoBall(const FVector& FrontFootLocation);
    FVector CalculateReleaseLocation() const;
    FVector CalculateReleaseVelocity() const;
    void SetPhase(EBowlingPhase NewPhase);
    float CalculateRhythmScore() const;

    // GAS Internal
    void InitializeAttributeSet();
    void GrantDefaultAbilities();
    UCricketBowlingAbility* FindDeliveryAbility(EDeliveryType DeliveryType) const;
    void OnDeliveryAbilityActivated(UCricketBowlingAbility* Ability);
    void OnDeliveryAbilityEnded(UCricketBowlingAbility* Ability);
    void UpdateGASAttributes(float DeltaTime);
};