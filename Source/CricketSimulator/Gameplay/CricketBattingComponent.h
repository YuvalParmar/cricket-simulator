#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CricketBattingPawn.h"
#include "CricketBall.h"
#include "CricketBattingComponent.generated.h"

class ACricketBattingPawn;
class ACricketBall;

USTRUCT(BlueprintType)
struct FBattingAttributes
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Timing = 1.0f; // 0-2, timing window size multiplier

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Power = 1.0f; // 0-2, boundary hitting ability

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Technique = 1.0f; // 0-2, control vs aggression

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Footwork = 1.0f; // 0-2, reach and balance

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Temperament = 1.0f; // 0-2, pressure handling

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float ShotSelection = 1.0f; // 0-2, choosing right shot

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
    float Aggression = 1.0f; // 0-2, natural aggression
};

USTRUCT(BlueprintType)
struct FBatPhysicsState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FTransform BatTransform;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector LinearVelocity;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector AngularVelocity;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector SweetSpotLocation; // World space

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector SweetSpotNormal; // Face direction

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float BatMass = 1.2f; // kg

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float BatLength = 0.965f; // meters (max legal)

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float BatWidth = 0.108f; // meters (max legal)

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float SweetSpotRadius = 0.04f; // 4cm
};

USTRUCT(BlueprintType)
struct FContactResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bContact = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ContactPoint;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector ContactNormal; // Ball to bat

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float PenetrationDepth = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float RelativeSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bSweetSpot = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    bool bEdge = false;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector BallVelocityBefore;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    FVector BatVelocityAtContact;

    UPROPERTY(BlueprintReadOnly, Category = "Result")
    float ContactAngle = 0.0f; // Degrees from perpendicular
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotExecuted, const FShotResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimingWindowChanged, ETimingWindow, Window);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBatSwingProgress, float, Progress, const FTransform&, BatTransform);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UCricketBattingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCricketBattingComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Shot execution
    UFUNCTION(BlueprintCallable, Category = "Batting")
    void ExecuteShot(EBattingShotType ShotType, const FTransform& BatTransform, const FVector& BatVelocity, float SwingTime, float BallArrivalTime);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetBattingPawn(ACricketBattingPawn* Pawn);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetTargetBall(ACricketBall* Ball);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void UpdateBatPhysicsState(const FBatPhysicsState& NewState);

    UFUNCTION(BlueprintCallable, Category = "Batting")
    void SetBattingAttributes(const FBattingAttributes& NewAttributes);

    // Prediction
    UFUNCTION(BlueprintPure, Category = "Batting")
    float PredictBallArrivalTime() const;

    UFUNCTION(BlueprintPure, Category = "Batting")
    FVector PredictBallContactPoint() const;

    UFUNCTION(BlueprintPure, Category = "Batting")
    ETimingWindow GetCurrentTimingWindow() const { return CurrentTimingWindow; }

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnShotExecuted OnShotExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTimingWindowChanged OnTimingWindowChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBatSwingProgress OnBatSwingProgress;

protected:
    // References
    UPROPERTY()
    TObjectPtr<ACricketBattingPawn> BattingPawn;

    UPROPERTY()
    TObjectPtr<ACricketBall> TargetBall;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting Setup")
    FBattingAttributes Attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting Setup")
    TArray<FShotDefinition> ShotDefinitions;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FBatPhysicsState BatPhysicsState;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    EBattingShotType PendingShotType = EBattingShotType::None;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    bool bShotPending = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float PendingSwingTime = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    ETimingWindow CurrentTimingWindow = ETimingWindow::Missed;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float CurrentBallArrivalTime = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FShotResult LastShotResult;

    // Timing window thresholds (as fraction of ideal contact time)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing Windows")
    float VeryEarlyThreshold = 0.15f; // 15% before ideal

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing Windows")
    float EarlyThreshold = 0.05f; // 5% before ideal

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing Windows")
    float PerfectThreshold = 0.02f; // ±2% around ideal

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing Windows")
    float LateThreshold = 0.05f; // 5% after ideal

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing Windows")
    float VeryLateThreshold = 0.15f; // 15% after ideal

    // Internal functions
    void UpdateTimingWindow(float DeltaTime);
    ETimingWindow DetermineTimingWindow(float SwingTime, float BallArrivalTime) const;
    void CalculateShotResult(const FShotDefinition& ShotDef, const FContactResult& Contact, FShotResult& OutResult);
    FContactResult DetectBatBallContact(const FTransform& BatTransform, const FVector& BatVelocity) const;
    FVector CalculateSweetSpotWorldLocation(const FTransform& BatTransform) const;
    FVector CalculateSweetSpotNormal(const FTransform& BatTransform) const;
    float CalculateBatSpeedAtContact(const FVector& BatVelocity, const FVector& ContactPoint) const;
    float CalculateContactAngle(const FVector& BallVelocity, const FVector& BatNormal) const;
    int32 PredictRuns(const FShotResult& Result) const;
    bool CheckWicket(const FShotResult& Result, FGameplayTag& OutWicketType) const;
    const FShotDefinition* GetShotDefinition(EBattingShotType ShotType) const;
    void ApplyAttributesToShot(FShotResult& Result) const;
};