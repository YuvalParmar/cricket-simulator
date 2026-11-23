#pragma once

#include "CoreMinimal.h"
#include "Input/IBatInputInterface.h"
#include "PhoneBatReceiverComponent.h"
#include "PhoneBatNetworkProtocol.h"
#include "PhoneBatInput.generated.h"

class UPhoneBatReceiverComponent;
class ACricketBattingPawn;

USTRUCT(BlueprintType)
struct FPhoneBatInputState
{
    GENERATED_BODY()

    // Current bat pose from phone
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FTransform BatTransform;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FVector BatVelocity;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FVector BatAngularVelocity;

    // Swing state
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    EPhoneBatSwingPhase SwingPhase = EPhoneBatSwingPhase::Idle;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float SwingProgress = 0.0f; // 0-1 within phase

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float BatSpeed = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float MaxBatSpeedThisSwing = 0.0f;

    // Contact prediction
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    bool bContactPredicted = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    int64 PredictedContactTime = 0; // Microseconds (server clock)

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FVector PredictedContactPoint;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FVector PredictedBatVelocityAtContact;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FQuat PredictedBatOrientationAtContact;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float ContactConfidence = 0.0f;

    // Connection state
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    EPhoneBatConnectionState ConnectionState = EPhoneBatConnectionState::Disconnected;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float LatencyMs = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float JitterMs = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float PacketLoss = 0.0f;

    // Calibration
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    bool bIsCalibrated = false;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FTransform CalibrationTransform;

    // Shot selection (from phone UI or auto-detected)
    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FGameplayTag SelectedShotType;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    FGameplayTag SelectedFootwork;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat Input")
    float PowerModifier = 1.0f; // 0.5 (defensive) to 2.0 (aggressive)
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CRICKETSIMULATOR_API UPhoneBatInput : public UActorComponent, public IBatInputInterface
{
    GENERATED_BODY()

public:
    UPhoneBatInput();

    virtual void InitializeComponent() override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // IBatInputInterface implementation
    virtual FBattingInputState GetBattingInputState() const override;
    virtual FBowlingInputState GetBowlingInputState() const override;
    virtual FFieldingInputState GetFieldingInputState() const override;

    virtual void SetBattingPawn(ACricketBattingPawn* Pawn) override;
    virtual void SetBowlingPawn(ACricketBowlingPawn* Pawn) override;
    virtual void SetFieldingPawn(ACricketFieldingPawn* Pawn) override;

    virtual bool IsInputActive() const override;
    virtual FString GetInputDeviceName() const override;

    // PhoneBat specific functions
    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void StartDiscovery(int32 Port = 7777);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void StopDiscovery();

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void StartPairing(const FString& PairingCode);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void CancelPairing();

    // IBatInputInterface implementation - BlueprintNativeEvent functions
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PhoneBat Input")
    void StartCalibration();

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void CompleteCalibrationStep(int32 StepIndex, const FTransform& PhoneToBatTransform);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat Input")
    void SetShotSelection(FGameplayTag ShotType, FGameplayTag Footwork, float PowerModifier);

    UFUNCTION(BlueprintPure, Category = "PhoneBat Input")
    FPhoneBatInputState GetPhoneBatState() const { return CurrentState; }

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "PhoneBat Input")
    bool IsCalibrated() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "PhoneBat Input")
    bool IsConnected() const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Events")
    FOnPhoneBatConnectionChanged OnConnectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Events")
    FOnPhoneBatSwingDetected OnSwingDetected;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Events")
    FOnPhoneBatContactPredicted OnContactPredicted;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Events")
    FOnPhoneBatCalibrationProgress OnCalibrationProgress;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat Events")
    FOnPhoneBatCalibrationComplete OnCalibrationComplete;

protected:
    // References
    UPROPERTY()
    TObjectPtr<UPhoneBatReceiverComponent> ReceiverComponent;

    UPROPERTY()
    TObjectPtr<ACricketBattingPawn> BattingPawn;

    UPROPERTY()
    TObjectPtr<ACricketBowlingPawn> BowlingPawn;

    UPROPERTY()
    TObjectPtr<ACricketFieldingPawn> FieldingPawn;

    // State
    UPROPERTY()
    FPhoneBatInputState CurrentState;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat Config")
    float MaxPredictionTimeMs = 100.0f; // How far ahead to predict contact

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat Config")
    float MinContactConfidence = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat Config")
    bool bUsePredictiveExtrapolation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat Config")
    float ExtrapolationTimeMs = 16.0f; // One frame at 60Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhoneBat Config")
    int32 CalibrationSteps = 3;

    // Internal state
    TArray<FPhoneBatSensorFramePayload> RecentFrames;
    FPhoneBatSwingPhasePayload LastSwingPhase;
    bool bSwingStarted = false;
    uint64 SwingStartTime = 0;
    TArray<FPhoneBatSensorFramePayload> CurrentSwingFrames;

    // Callbacks from receiver component
    void OnSensorFrameReceived(const FPhoneBatSensorFramePayload& Frame);
    void OnSwingPhaseReceived(const FPhoneBatSwingPhasePayload& Phase);
    void OnContactPredictionReceived(const FPhoneBatContactPredictPayload& Prediction);
    void OnSwingResultReceived(const FPhoneBatSwingResultPayload& Result);
    void OnCalibrationDataReceived(const FPhoneBatCalibrationPayload& Calibration);
    void OnConnectionStateChanged(EPhoneBatConnectionState NewState);
    void OnClockSyncUpdate(int32 ClockOffsetUs, int32 RTTUs);

    // Internal functions
    void UpdateBattingState(float DeltaTime);
    void ProcessSensorFrame(const FPhoneBatSensorFramePayload& Frame);
    void UpdateSwingDetection();
    void PredictContact();
    void ApplyLatencyCompensation();
    FTransform CalculateWorldBatTransform(const FTransform& PhoneTransform) const;
    void StartNewSwing();
    void EndCurrentSwing();
    void UpdateConnectionQuality();

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatConnectionChanged, EPhoneBatConnectionState, NewState);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatSwingDetected, EPhoneBatSwingPhase, Phase);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatContactPredicted, const FPhoneBatContactPredictPayload&, Prediction);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatCalibrationProgress, int32, StepIndex);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhoneBatCalibrationComplete, bool, bSuccess);
};