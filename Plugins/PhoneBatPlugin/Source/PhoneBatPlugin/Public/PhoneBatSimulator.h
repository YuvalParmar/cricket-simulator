#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkManager.h"
#include "PhoneBatSensorFusion.h"
#include "PhoneBatCalibrationManager.h"
#include "PhoneBatMotionInterpreter.h"
#include "PhoneBatSimulator.generated.h"

USTRUCT(BlueprintType)
struct FBattedBallResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    bool bContact = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector ContactPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector BallVelocityBefore = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector BatVelocityAtContact = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector BallVelocityAfter = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float ContactAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    bool bSweetSpot = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    bool bEdge = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float TimingQuality = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float PowerRating = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float ControlRating = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    int32 PredictedRuns = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FString ShotType;

    // Sub-frame contact data
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|SubFrame")
    float SubFrameContactTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|SubFrame")
    FVector SubFrameContactPoint;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|SubFrame")
    FVector SubFrameBatVelocity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|SubFrame")
    FVector SubFrameBallVelocity;

    // Latency instrumentation
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    int64 ContactDetectionTimeUs = 0;
};

USTRUCT(BlueprintType)
struct FBState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FTransform BatTransform;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector LinearVelocity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector AngularVelocity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector SweetSpotLocation;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    FVector SweetSpotNormal;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float BatMass = 1.2f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float BatLength = 0.965f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float BatWidth = 0.108f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat")
    float SweetSpotRadius = 0.04f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    float EstimatedLatencyMs = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Latency")
    int64 LastPacketTimeUs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattedBall, const FBattedBallResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBatStateUpdated, const FBState&, State);

class UPhoneBatNetworkManager;
class UPhoneBatSensorFusion;
class UPhoneBatCalibrationManager;
class UPhoneBatMotionInterpreter;

UCLASS(BlueprintType)
class PHONEBATPLUGIN_API UPhoneBatSimulator : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatSimulator();

    // Singleton access
    static UPhoneBatSimulator& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Update simulation (call from game thread)
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Simulation")
    void Update(float DeltaTime);

    // Set target ball for collision detection
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Simulation")
    void SetTargetBall(class AActor* BallActor, float BallRadius = 0.036f);

    // Apply calibration
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Simulation")
    void ApplyCalibration(const FPhoneBatCalibrationResult& Calibration);

    // Manual bat state update (for testing without phone)
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Simulation")
    void SetBatState(const FBState& NewState);

    // Getters
    UFUNCTION(BlueprintPure, Category = "PhoneBat|Simulation")
    const FBState& GetBatState() const { return BatState; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Simulation")
    bool IsSimulating() const { return bIsSimulating; }

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    float CoefficientOfRestitution = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    float SweetSpotCORBoost = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    float EdgeCORPenalty = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    float MaxLatencyCompensationMs = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    bool bEnableSubFramePrediction = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat|Simulation")
    float SubFrameTimeStep = 0.001f;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnBattedBall OnBattedBall;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnBatStateUpdated OnBatStateUpdated;

protected:
    // State
    UPROPERTY()
    FBState BatState;

    UPROPERTY()
    bool bIsSimulating = false;

    UPROPERTY()
    bool bCalibrationApplied = false;

    // Ball reference
    TWeakObjectPtr<AActor> TargetBall;
    float BallRadius = 0.036f;

    // Physics
    FVector BallPosition;
    FVector BallVelocity;
    FVector PreviousBallPosition;
    FVector PreviousBallVelocity;

    // Latency compensation
    TArray<FBState> StateHistory;
    static const int32 MAX_HISTORY_SIZE = 30;
    float DeltaTime = 0.016f;

    // Network integration
    UPhoneBatNetworkManager* NetworkManager;
    UPhoneBatSensorFusion* SensorFusion;
    UPhoneBatCalibrationManager* CalibrationManager;
    UPhoneBatMotionInterpreter* MotionInterpreter;

    // Internal functions
    void UpdateFromPhoneData();
    void UpdateBatTransform(float DeltaTime);
    void PredictStateWithLatency(float LatencyMs);
    FBattedBallResult DetectBallContact(float DeltaTime);
    FBattedBallResult DetectBallContactSubFrame(float DeltaTime);
    FVector CalculateSweetSpotLocation() const;
    FVector CalculateSweetSpotNormal() const;
    FVector CalculateBatVelocityAtPoint(const FVector& Point) const;
    void ApplyBallPhysics(FBattedBallResult& Result);

    // Network callbacks
    void OnSensorFrameReceived(const FPhoneBatSensorFramePayload& Frame);
    void OnSwingPhaseReceived(const FPhoneBatSwingPhasePayload& Phase);
    void OnContactPredictionReceived(const FPhoneBatContactPredictPayload& Prediction);
    void OnSwingResultReceived(const FPhoneBatSwingResultPayload& Result);
    void OnCalibrationReceived(const FPhoneBatCalibrationPayload& Calibration);
    void OnClockSyncUpdate(int64 ClockOffsetUs, int32 RTTUs);
};