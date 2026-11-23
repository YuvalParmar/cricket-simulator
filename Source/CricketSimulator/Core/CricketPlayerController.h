#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "CricketPlayerController.generated.h"

class UCricketInputConfig;
class ACricketBattingPawn;
class ACricketBowlingPawn;
class ACricketFieldingPawn;

UENUM(BlueprintType)
enum class ECricketControlMode : uint8
{
    Spectator       UMETA(DisplayName = "Spectator"),
    Batting         UMETA(DisplayName = "Batting"),
    Bowling         UMETA(DisplayName = "Bowling"),
    Fielding        UMETA(DisplayName = "Fielding"),
    Umpire          UMETA(DisplayName = "Umpire"),
    Replay          UMETA(DisplayName = "Replay")
};

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API ACricketPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACricketPlayerController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    // Control mode management
    UFUNCTION(BlueprintCallable, Category = "Control")
    void SetControlMode(ECricketControlMode NewMode);

    UFUNCTION(BlueprintPure, Category = "Control")
    ECricketControlMode GetControlMode() const { return CurrentControlMode; }

    // Batting input
    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnShotTypeSelected(int32 ShotTypeIndex);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnFootworkInput(const FVector2D& Direction, float Magnitude);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnBatAngleInput(const FVector2D& Angle); // X = Face angle, Y = Wrist rotation

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnTimingTriggerPressed();

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnTimingTriggerReleased();

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void OnPowerModulatorPressed(bool bAggressive);

    // Bowling input
    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnRunUpMarkerPlaced(FVector Location);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnRunUpRhythmInput();

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnDeliveryTypeSelected(int32 DeliveryTypeIndex);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnAimInput(const FVector2D& AimOffset);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnReleasePointInput(float Height, float Angle);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnSeamOrientationInput(const FVector& Orientation);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnSpinAxisInput(const FVector& Axis, float RPM);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnEffortInput(float Effort);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnBowlButtonPressed();

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnBowlButtonReleased();

    // Fielding input
    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnMovementInput(const FVector2D& Direction);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnDiveInput(const FVector2D& Direction);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnThrowInput(const FVector& TargetLocation, float Power);

    UFUNCTION(BlueprintCallable, Category = "Fielding")
    void OnSwitchFielderInput(int32 Direction); // +1 next, -1 previous

    // Phone bat input (received from companion app)
    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatPoseUpdate(const FTransform& BatTransform, const FVector& LinearVelocity, const FVector& AngularVelocity);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatSwingPhaseChanged(int32 Phase); // 0=Idle, 1=Backswing, 2=Downswing, 3=FollowThrough

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatContactPredicted(float TimeToContact, float Confidence);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat")
    void OnPhoneBatCalibrationComplete(const FTransform& PhoneToBatTransform);

    // Camera
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void ToggleCameraPerspective();

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraPerspective(bool bFirstPerson);

    // Getters for current controlled pawn
    UFUNCTION(BlueprintPure, Category = "Pawn")
    ACricketBattingPawn* GetBattingPawn() const;

    UFUNCTION(BlueprintPure, Category = "Pawn")
    ACricketBowlingPawn* GetBowlingPawn() const;

    UFUNCTION(BlueprintPure, Category = "Pawn")
    ACricketFieldingPawn* GetFieldingPawn() const;

protected:
    // Input configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UCricketInputConfig> InputConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputMappingContext> BattingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputMappingContext> BowlingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputMappingContext> FieldingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputMappingContext> PhoneBatMappingContext;

    // Current state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Control")
    ECricketControlMode CurrentControlMode = ECricketControlMode::Spectator;

    UPROPERTY(BlueprintReadOnly, Category = "Camera")
    bool bFirstPersonCamera = false;

    // Input action bindings
    void BindBattingActions(UEnhancedInputComponent* EnhancedInput);
    void BindBowlingActions(UEnhancedInputComponent* EnhancedInput);
    void BindFieldingActions(UEnhancedInputComponent* EnhancedInput);
    void BindPhoneBatActions(UEnhancedInputComponent* EnhancedInput);
    void BindCommonActions(UEnhancedInputComponent* EnhancedInput);

    void ApplyMappingContext(UInputMappingContext* Context, bool bAdd = true);

    // Current pawn references
    TWeakObjectPtr<ACricketBattingPawn> BattingPawn;
    TWeakObjectPtr<ACricketBowlingPawn> BowlingPawn;
    TWeakObjectPtr<ACricketFieldingPawn> FieldingPawn;
};