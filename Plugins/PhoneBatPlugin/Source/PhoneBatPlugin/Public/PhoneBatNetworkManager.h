#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PhoneBatNetworkProtocol.h"
#include "PhoneBatNetworkManager.generated.h"

USTRUCT(BlueprintType)
struct FPhoneBatConnectionStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    float LatencyMS = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    float PacketLossPercent = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    int64 LastPacketTimeUs = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    int32 PacketsPerSecond = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    int32 TotalPacketsReceived = 0;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    FString IPAddress = TEXT("192.168.1.100");

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    int32 Port = 7777;

    UPROPERTY(BlueprintReadWrite, Category = "PhoneBat|Network")
    bool bIsConnected = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensorFrameReceived, const FPhoneBatSensorFramePayload&, Frame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingPhaseReceived, const FPhoneBatSwingPhasePayload&, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContactPredictionReceived, const FPhoneBatContactPredictPayload&, Prediction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwingResultReceived, const FPhoneBatSwingResultPayload&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationReceived, const FPhoneBatCalibrationPayload&, Calibration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClockSyncUpdate, int64, ClockOffsetUs, int32, RTTUs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConnectionStatusChanged, bool, bConnected);

UCLASS(BlueprintType)
class PHONEBATPLUGIN_API UPhoneBatNetworkManager : public UObject
{
    GENERATED_BODY()

public:
    UPhoneBatNetworkManager();

    // Singleton access
    static UPhoneBatNetworkManager& Get();

    // Initialization
    void Initialize();
    void Shutdown();

    // Network management
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Network")
    bool StartListening(int32 Port = 7777);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Network")
    void StopListening();

    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Network")
    void SendClockSyncRequest();

    // Connection info
    UFUNCTION(BlueprintPure, Category = "PhoneBat|Network")
    FPhoneBatConnectionStats GetConnectionStats() const { return ConnectionStats; }

    UFUNCTION(BlueprintPure, Category = "PhoneBat|Network")
    bool IsConnected() const { return ConnectionStats.bIsConnected; }

    // Manual packet injection (for testing)
    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Network")
    void InjectSensorFrame(const FPhoneBatSensorFramePayload& Payload);

    UFUNCTION(BlueprintCallable, Category = "PhoneBat|Network")
    void InjectSwingPhase(const FPhoneBatSwingPhasePayload& Payload);

    // Events
    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSensorFrameReceived OnSensorFrameReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSwingPhaseReceived OnSwingPhaseReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnContactPredictionReceived OnContactPredictionReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnSwingResultReceived OnSwingResultReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnCalibrationReceived OnCalibrationReceived;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnClockSyncUpdate OnClockSyncUpdate;

    UPROPERTY(BlueprintAssignable, Category = "PhoneBat|Events")
    FOnConnectionStatusChanged OnConnectionStatusChanged;

protected:
    // State
    UPROPERTY()
    FPhoneBatConnectionStats ConnectionStats;

    bool bIsListening = false;
    int32 ListenPort = 7777;

    // Network
    TSharedPtr<FSocket> ListenSocket;
    FRunnableThread* ReceiveThread = nullptr;
    bool bStopReceiveThread = false;

    float LastPacketTimeSeconds = 0.0f;
    TArray<float> LatencyHistory;
    float AverageLatencyMs = 0.0f;
    int64 ClockOffsetUs = 0;

    uint32 PacketSequence = 0;
    int64 LastPacketTimestampUs = 0;

    // Internal receive thread
    class FPhoneBatReceiveRunnable : public FNonAbandonableTask
    {
    public:
        FPhoneBatReceiveRunnable(UPhoneBatNetworkManager* InManager) : Manager(InManager) {}
        void DoWork();
        FORCEINLINE TStatId GetStatId() const { RETURN_STAT_ID(STAT_PhoneBatReceive); }

    private:
        UPhoneBatNetworkManager* Manager;
    };

    friend class FDynamicThreadAffinityGroup<FPlatformAffinity::GetNumHyperThreads()>;
};