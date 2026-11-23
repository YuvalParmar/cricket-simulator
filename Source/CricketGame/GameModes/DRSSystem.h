#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBall.h"
#include "DRSSystem.generated.h"

class ACricketBall;
class ACricketGameMode;
class APitchActor;

UENUM(BlueprintType)
enum class EDRSReviewType : uint8
{
    LBW             UMETA(DisplayName = "LBW"),
    CaughtBehind    UMETA(DisplayName = "Caught Behind"),
    Caught          UMETA(DisplayName = "Caught"),
    HitPad          UMETA(DisplayName = "Hit Pad"),
    NoBall          UMETA(DisplayName = "No Ball"),
    Boundary        UMETA(DisplayName = "Boundary Check")
};

UENUM(BlueprintType)
enum class EDRSStage : uint8
{
    Initiated       UMETA(DisplayName = "Initiated"),
    BallTracking    UMETA(DisplayName = "Ball Tracking"),
    UltraEdge       UMETA(DisplayName = "UltraEdge"),
    HotSpot         UMETA(DisplayName = "HotSpot"),
    Decision        UMETA(DisplayName = "Decision"),
    Complete        UMETA(DisplayName = "Complete")
};

UENUM(BlueprintType)
enum class EDRSOutcome : uint8
{
    Out             UMETA(DisplayName = "Out"),
    NotOut          UMETA(DisplayName = "Not Out"),
    UmpiresCall     UMETA(DisplayName = "Umpire's Call"),
    Inconclusive    UMETA(DisplayName = "Inconclusive")
};

USTRUCT(BlueprintType)
struct FBallTrackingData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    TArray<FVector> PredictedPath; // World space positions

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    TArray<FVector> ActualPath; // From physics simulation

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    FVector PredictedImpactPoint; // On pads/stumps

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    FVector PredictedStumpImpact; // Where ball hits stumps

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    bool bPitchesInLine = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    bool bImpactInLine = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    bool bHitsStumps = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    float StumpImpactHeight = 0.0f; // Meters from ground

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    float StumpImpactOffset = 0.0f; // Meters from center

    UPROPERTY(BlueprintReadOnly, Category = "Ball Tracking")
    float Confidence = 0.0f; // 0-1
};

USTRUCT(BlueprintType)
struct FUltraEdgeData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    TArray<float> AudioWaveform; // Microphone input

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    TArray<float> SyncedWaveform; // Synced to video frame

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    float SpikeTime = 0.0f; // Time of spike relative to ball pass

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    float SpikeMagnitude = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    bool bSpikeDetected = false;

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    bool bSpikeAlignedWithBall = false;

    UPROPERTY(BlueprintReadOnly, Category = "UltraEdge")
    float Confidence = 0.0f;
};

USTRUCT(BlueprintType)
struct FHotSpotData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    TArray<FVector2D> HotSpots; // UV coordinates on bat/pad

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    TArray<float> Temperatures; // Simulated temperature rise

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    bool bHotSpotOnBat = false;

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    bool bHotSpotOnPad = false;

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    FVector2D PrimaryHotSpotUV;

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    float PrimaryTemperature = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "HotSpot")
    float Confidence = 0.0f;
};

USTRUCT(BlueprintType)
struct FDRSReviewRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    EDRSReviewType ReviewType = EDRSReviewType::LBW;

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    int32 RequestingTeam = 0; // 0 = Team A, 1 = Team B

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    FString UmpireDecision; // "Out" or "Not Out"

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    ACricketBall* BallActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    FVector BatterPosition;

    UPROPERTY(BlueprintReadWrite, Category = "DRS")
    FVector StumpPosition;
};

USTRUCT(BlueprintType)
struct FDRSResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    EDRSOutcome FinalDecision = EDRSOutcome::Inconclusive;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    bool bOriginalDecisionOverturned = false;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    FBallTrackingData BallTracking;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    FUltraEdgeData UltraEdge;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    FHotSpotData HotSpot;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    FString Reasoning;

    UPROPERTY(BlueprintReadOnly, Category = "DRS")
    float OverallConfidence = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDRSStageChanged, EDRSStage, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDRSCompleted, const FDRSResult&, Result);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UDRSSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRSSystem();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Review management
    UFUNCTION(BlueprintCallable, Category = "DRS")
    void RequestReview(const FDRSReviewRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "DRS")
    void CancelReview();

    UFUNCTION(BlueprintPure, Category = "DRS")
    bool IsReviewActive() const { return bReviewActive; }

    UFUNCTION(BlueprintPure, Category = "DRS")
    EDRSStage GetCurrentStage() const { return CurrentStage; }

    UFUNCTION(BlueprintPure, Category = "DRS")
    const FDRSResult& GetCurrentResult() const { return CurrentResult; }

    // Analysis functions (called during review stages)
    UFUNCTION(BlueprintCallable, Category = "DRS|Analysis")
    void RunBallTracking();

    UFUNCTION(BlueprintCallable, Category = "DRS|Analysis")
    void RunUltraEdge();

    UFUNCTION(BlueprintCallable, Category = "DRS|Analysis")
    void RunHotSpot();

    UFUNCTION(BlueprintCallable, Category = "DRS|Analysis")
    void MakeDecision();

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDRSStageChanged OnDRSStageChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnDRSCompleted OnDRSCompleted;

protected:
    // References
    UPROPERTY()
    TObjectPtr<ACricketGameMode> GameMode;

    UPROPERTY()
    TObjectPtr<APitchActor> PitchActor;

    // Current review state
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    bool bReviewActive = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    EDRSStage CurrentStage = EDRSStage::Initiated;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    FDRSReviewRequest CurrentRequest;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    FDRSResult CurrentResult;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    float StageProgress = 0.0f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "DRS State")
    float StageDuration = 5.0f; // Seconds per stage

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS Config")
    float BallTrackingDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS Config")
    float UltraEdgeDuration = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS Config")
    float HotSpotDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS Config")
    float DecisionDuration = 2.0f;

    // Internal functions
    void AdvanceStage();
    void UpdateStageProgress(float DeltaTime);
    FBallTrackingData SimulateBallTracking(const FDRSReviewRequest& Request) const;
    FUltraEdgeData SimulateUltraEdge(const FDRSReviewRequest& Request) const;
    FHotSpotData SimulateHotSpot(const FDRSReviewRequest& Request) const;
    EDRSOutcome DetermineLBWDecision(const FBallTrackingData& Tracking, const FUltraEdgeData& Edge, const FHotSpotData& HotSpot) const;
    EDRSOutcome DetermineCaughtDecision(const FUltraEdgeData& Edge, const FHotSpotData& HotSpot) const;
    EDRSOutcome DetermineNoBallDecision(const FDRSReviewRequest& Request) const;
    EDRSOutcome DetermineBoundaryDecision(const FDRSReviewRequest& Request) const;
    void BroadcastStageChange(EDRSStage NewStage);
};