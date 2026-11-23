#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CricketBowlingPawn.h"
#include "CricketBall.h"
#include "CricketBowlingComponent.generated.h"

class ACricketBowlingPawn;
class ACricketBall;
class APitchActor;

USTRUCT(BlueprintType)
struct FBowlingAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Pace = 1.0f; // 0-2, raw speed

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Accuracy = 1.0f; // 0-2, line and length consistency

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Swing = 1.0f; // 0-2, conventional swing ability

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Seam = 1.0f; // 0-2, seam movement

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Spin = 1.0f; // 0-2, spin RPM and control

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Variations = 1.0f; // 0-2, variety of deliveries

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Stamina = 1.0f; // 0-2, recovery between balls/overs

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Intelligence = 1.0f; // 0-2, reading batter, setting traps
};

USTRUCT(BlueprintType)
struct FDeliverySetup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    EBowlingDeliveryType DeliveryType = EBowlingDeliveryType::Fast;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    FVector TargetPitchLocation; // Where on pitch to land

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    float Effort = 0.5f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    FVector SeamOrientation = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    FVector SpinAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    float SpinRPM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    float ReleaseHeight = 2.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
    float ReleaseAngle = 0.0f;
};

USTRUCT(BlueprintType)
struct FDeliveryAnalysis
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    FVector PredictedPitchLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    FVector PredictedBatterContactPoint;

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    float PredictedSpeed = 0.0f; // km/h

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    float PredictedSwing = 0.0f; // meters deviation

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    float PredictedSpinDeviation = 0.0f; // meters deviation

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    float PredictedBounceHeight = 0.0f; // meters

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    float PredictedBounceDeviation = 0.0f; // meters sideways

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    bool bNoBallRisk = false;

    UPROPERTY(BlueprintReadOnly, Category = "Analysis")
    bool bWideRisk = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeliveryPrepared, const FDeliveryAnalysis&, Analysis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeliveryBowled, const FDeliveryResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRunUpProgress, float, Progress);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UCricketBowlingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCricketBowlingComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Delivery preparation
    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void PrepareDelivery(const FDeliverySetup& Setup);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetBowlingPawn(ACricketBowlingPawn* Pawn);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetPitchActor(APitchActor* Pitch);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void UpdateRunUpProgress(float Progress);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void OnRhythmTap(float TimingScore);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetEffort(float Effort);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetSeamOrientation(const FVector& Orientation);

    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void SetSpinAxis(const FVector& Axis, float RPM);

    // Ball release
    UFUNCTION(BlueprintCallable, Category = "Bowling")
    void ReleaseBall(const FVector& ReleaseLocation, const FVector& ReleaseVelocity, const FVector& ReleaseSpin);

    // Analysis
    UFUNCTION(BlueprintCallable, Category = "Bowling")
    FDeliveryAnalysis AnalyzeCurrentDelivery() const;

    UFUNCTION(BlueprintPure, Category = "Bowling")
    const FDeliverySetup& GetCurrentSetup() const { return CurrentSetup; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    float GetRhythmScore() const { return RhythmScore; }

    UFUNCTION(BlueprintPure, Category = "Bowling")
    float GetStamina() const { return CurrentStamina; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeliveryPrepared OnDeliveryPrepared;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDeliveryBowled OnDeliveryBowled;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRunUpProgress OnRunUpProgress;

protected:
    // References
    UPROPERTY()
    TObjectPtr<ACricketBowlingPawn> BowlingPawn;

    UPROPERTY()
    TObjectPtr<APitchActor> PitchActor;

    UPROPERTY()
    TObjectPtr<ACricketBall> CurrentBall;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling Setup")
    FBowlingAttributes Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling Setup")
    TArray<FDeliveryDefinition> DeliveryDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling Setup")
    TObjectPtr<UDataTable> DeliveryDefinitionsTable;

    // Current state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FDeliverySetup CurrentSetup;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    EBowlingPhase CurrentPhase = EBowlingPhase::SettingField;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float RunUpProgress = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float RhythmScore = 1.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float CurrentStamina = 100.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float MaxStamina = 100.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    int32 BallsInCurrentOver = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    int32 OversBowled = 0;

    // Last delivery result
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Results")
    FDeliveryResult LastDeliveryResult;

    // Internal functions
    void UpdateStamina(float DeltaTime);
    FDeliveryResult ExecuteDeliveryInternal();
    FVector CalculateReleaseVelocity() const;
    FVector CalculateReleaseSpin() const;
    FVector CalculateReleaseLocation() const;
    void ApplyAttributesToDelivery(FDeliveryResult& Result) const;
    void SimulateBallTrajectory(const FDeliverySetup& Setup, FDeliveryAnalysis& OutAnalysis) const;
    const FDeliveryDefinition* GetDeliveryDefinition(EBowlingDeliveryType Type) const;
    float CalculateSwingAmount(const FDeliverySetup& Setup) const;
    float CalculateSeamDeviation(const FDeliverySetup& Setup) const;
    float CalculateSpinDeviation(const FDeliverySetup& Setup) const;
    bool CheckNoBall(const FVector& FrontFootLocation) const;
    bool CheckWide(const FVector& PitchLocation, bool bRightHandedBatter) const;
    void UpdateBallPhysics(float DeltaTime);
};