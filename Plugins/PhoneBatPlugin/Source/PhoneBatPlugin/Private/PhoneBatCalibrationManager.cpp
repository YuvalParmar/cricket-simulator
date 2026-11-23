#include "PhoneBatCalibrationManager.h"

UPhoneBatCalibrationManager::UPhoneBatCalibrationManager()
{
    CurrentResult = FPhoneBatCalibrationResult();
}

UPhoneBatCalibrationManager& UPhoneBatCalibrationManager::Get()
{
    static UPhoneBatCalibrationManager* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<UPhoneBatCalibrationManager>();
        Instance->AddToRoot();
    }
    return *Instance;
}

void UPhoneBatCalibrationManager::Initialize()
{
    CurrentResult = FPhoneBatCalibrationResult();
    UE_LOG(LogTemp, Log, TEXT("PhoneBatCalibrationManager initialized"));
}

void UPhoneBatCalibrationManager::Shutdown()
{
    CurrentResult = FPhoneBatCalibrationResult();
    UE_LOG(LogTemp, Log, TEXT("PhoneBatCalibrationManager shutdown"));
}

void UPhoneBatCalibrationManager::ProcessCalibrationPacket(const FPhoneBatCalibrationPayload& Payload)
{
    if (Payload.bComplete && Payload.bIsValid)
    {
        CurrentResult.PhoneToBatTransform = Payload.PhoneToBatTransform;
        CurrentResult.GripOffset = FVector(Payload.PhoneToBatTransform.GetLocation().X,
                                           Payload.PhoneToBatTransform.GetLocation().Y,
                                           Payload.PhoneToBatTransform.GetLocation().Z);
        CurrentResult.GripRotation = Payload.PhoneToBatTransform.GetRotation().Rotator();
        CurrentResult.SweetSpotDistance = Payload.SweetSpotDistance * 0.01f;
        CurrentResult.BatLength = Payload.BatLength * 0.01f;
        CurrentResult.bIsValid = true;
        CurrentResult.QualityScore = Payload.QualityScore;
        CurrentResult.CalibrationTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

        OnCalibrationReceived.Broadcast(CurrentResult);
        UE_LOG(LogTemp, Log, TEXT("PhoneBatCalibration: Applied, quality: %.2f"), CurrentResult.QualityScore);
    }
    else if (Payload.Step >= 0)
    {
        FString Instruction = Payload.Instruction.IsEmpty() ?
            FString::Printf(TEXT("Step %d: Calibration"), Payload.Step) :
            Payload.Instruction;
        OnCalibrationStepChanged.Broadcast(Payload.Step, Instruction);
    }
}

void UPhoneBatCalibrationManager::ComputeTransformFromSamples(const TArray<FQuat>& Quaternions, const TArray<FVector>& Positions)
{
    if (Quaternions.Num() == 0 || Positions.Num() == 0) return;

    FVector AvgPos = FVector::ZeroVector;
    for (const FVector& Pos : Positions)
    {
        AvgPos += Pos;
    }
    AvgPos /= Positions.Num();

    FQuat AvgQuat = FQuat::Identity;
    float Weight = 1.0f / Quaternions.Num();
    for (const FQuat& q : Quaternions)
    {
        AvgQuat = FQuat::Slerp(AvgQuat, q, Weight);
    }

    CurrentResult.PhoneToBatTransform = FTransform(AvgQuat, AvgPos);
}