#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemInterface.h"
#include "Core/CricketDataAssets.h"
#include "CricketGameplayAbilities.h"
#include "CricketBattingPawn.generated.h"

class UCapsuleComponent;
class UCricketBattingComponent;
class UPhoneBatReceiverComponent;
class UCricketAbilitySystemComponent;
class UCricketAttributeSet;
class UCricketShotAbility;
class UCricketFootworkAbility;
class UCricketGameplayAbility;

UENUM(BlueprintType)
enum class ETimingWindow : uint8
{
    VeryEarly       UMETA(DisplayName = "Very Early"),
    Early           UMETA(DisplayName = "Early"),
    Perfect         UMETA(DisplayName = "Perfect"),
    Late            UMETA(DisplayName = "Late"),
    VeryLate        UMETA(DisplayName = "Very Late"),
    Missed          UMETA(DisplayName = "Missed")
};

USTRUCT(BlueprintType)
struct FShotDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    EShotType ShotType = EShotType::ForwardDefense;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    FGameplayTag ShotTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    FName AnimationMontageName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float MinBatSpeed = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float MaxBatSpeed = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float IdealContactAngle = 0.0f; // Degrees from perpendicular

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float AngleTolerance = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float PowerMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float ControlMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float PlacementBias = 0.0f; // -1 = leg side, 0 = straight, 1 = off side

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    float LoftProbability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    EFootworkType RequiredFootwork = EFootworkType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    FGameplayTagContainer SuitableDeliveries;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
    FGameplayTagContainer UnsuitableDeliveries;
};

USTRUCT(BlueprintType)
struct FShotResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    EShotType ShotType = EShotType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    ETimingWindow Timing = ETimingWindow::Missed;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float BatSpeedAtContact = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ContactAngle = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ContactPoint;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector BallVelocityAtContact;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector BatVelocityAtContact;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ShotPower = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ShotControl = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    int32 PredictedRuns = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bIsWicket = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FGameplayTag WicketType;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float TimingScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float PlacementScore = 0.0f; // 0-1

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float PowerScore = 0.0f; // 0-1
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotPlayed, const FShotResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimingWindowUpdate, ETimingWindow, Window);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatSwingUpdate, float, SwingProgress, FTransform, BatTransform);

UCLASS(Blueprintable)
class CRICKETGAME_API ACricketBattingPawn : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACricketBattingPawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // Shot selection
    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SelectShotType(int32 ShotTypeIndex);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetFootworkInput(const FVector2D& Direction, float Magnitude);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetBatAngleInput(float FaceAngle, float WristRotation);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnSwingCommit();

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnSwingRelease();

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetPowerModifier(bool bAggressive);

    // Phone Bat Integration
    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void UpdatePhoneBatPose(const FTransform& BatTransform, const FVector& LinearVelocity, const FVector& AngularVelocity);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatSwingPhaseChanged(int32 Phase);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatContactPredicted(float TimeToContact, float Confidence);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void SetPhoneBatCalibration(const FTransform& PhoneToBatTransform);

    // Camera
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraPerspective(bool bFirstPerson);

    // GAS Functions
    UFUNCTION(BlueprintCallable, Category = "GAS")
    void InitializeAbilities();

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateShotAbility(EShotType ShotType);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void ActivateFootworkAbility(EFootworkType FootworkType);

    UFUNCTION(BlueprintCallable, Category = "GAS")
    void EndActiveShotAbility();

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
    UFUNCTION(BlueprintPure, Category = "Batting")
    EShotType GetSelectedShotType() const { return SelectedShotType; }

    UFUNCTION(BlueprintPure, Category = "Batting")
    EFootworkType GetCurrentFootwork() const { return CurrentFootwork; }

    UFUNCTION(BlueprintPure, Category = "Batting")
    float GetBatFaceAngle() const { return BatFaceAngle; }

    UFUNCTION(BlueprintPure, Category = "Batting")
    float GetWristRotation() const { return WristRotation; }

    UFUNCTION(BlueprintPure, Category = "Batting")
    bool IsSwinging() const { return bIsSwinging; }

    UFUNCTION(BlueprintPure, Category = "Batting")
    const FShotResult& GetLastShotResult() const { return LastShotResult; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShotPlayed OnShotPlayed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTimingWindowUpdate OnTimingWindowUpdate;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBatSwingUpdate OnBatSwingUpdate;

protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> BatMesh;

    // GAS Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|GAS", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketAttributeSet> AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> ThirdPersonCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> ThirdPersonSpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCricketBattingComponent> BattingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UPhoneBatReceiverComponent> PhoneBatReceiver;

    // Shot configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting Setup")
    TArray<FShotDefinition> ShotDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting Setup")
    TObjectPtr<UDataTable> ShotDefinitionsTable;

    // GAS Abilities
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketShotAbility>> AvailableShotAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TSubclassOf<UCricketFootworkAbility> FootworkAbilityClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS Abilities")
    TArray<TSubclassOf<UCricketGameplayAbility>> DefaultAbilities;

    // GAS State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketShotAbility* ActiveShotAbility = nullptr;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GAS State")
    UCricketFootworkAbility* ActiveFootworkAbility = nullptr;

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
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    EShotType SelectedShotType = EShotType::ForwardDefense;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    EFootworkType CurrentFootwork = EFootworkType::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    float BatFaceAngle = 0.0f; // Degrees, 0 = perpendicular to pitch

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    float WristRotation = 0.0f; // Degrees

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    bool bIsSwinging = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    float SwingProgress = 0.0f; // 0-1

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    float SwingCommitTime = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    bool bPowerModifierActive = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Batting State")
    bool bAggressiveMode = false;

    // Phone bat state
    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    FTransform PhoneBatCalibrationTransform;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    FTransform CurrentPhoneBatTransform;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    FVector PhoneBatLinearVelocity;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    FVector PhoneBatAngularVelocity;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    int32 PhoneBatSwingPhase = 0; // 0=Idle, 1=Backswing, 2=Downswing, 3=FollowThrough

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    float PhoneBatContactPredictionTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    float PhoneBatContactConfidence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    bool bPhoneBatConnected = false;

    UPROPERTY(BlueprintReadOnly, Category = "Phone Bat")
    bool bUsePhoneBat = false;

    // Camera
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Camera")
    bool bFirstPersonCamera = false;

    // Last shot result
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Results")
    FShotResult LastShotResult;

    // Timing window detection
    float CurrentBallArrivalTime = 0.0f;
    float SwingStartTime = 0.0f;

    // Internal functions
    void UpdateBatTransform(float DeltaSeconds);
    void UpdatePhoneBatIntegration(float DeltaSeconds);
    void DetectTimingWindow();
    void ProcessShot(const FShotResult& Result);
    void PlayShotAnimation(EShotType ShotType, float PlayRate);
    void UpdateCameraBlending(float DeltaSeconds);
    void CalculateShotResult(FShotResult& OutResult);
    ETimingWindow DetermineTimingWindow(float SwingTime, float BallArrivalTime) const;
    FVector CalculateBatSweetSpot() const;
    float CalculateBatSpeed() const;

    // GAS Internal
    void InitializeAttributeSet();
    void GrantDefaultAbilities();
    UCricketShotAbility* FindShotAbility(EShotType ShotType) const;
    void OnShotAbilityActivated(UCricketShotAbility* Ability);
    void OnShotAbilityEnded(UCricketShotAbility* Ability);
    void UpdateGASAttributes(float DeltaTime);
};