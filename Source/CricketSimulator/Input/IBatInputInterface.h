#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IBatInputInterface.generated.h"

class UInputAction;
class UInputMappingContext;

UENUM(BlueprintType)
enum class EInputDeviceType : uint8
{
    KeyboardMouse   UMETA(DisplayName = "Keyboard & Mouse"),
    DualSense       UMETA(DisplayName = "PS5 DualSense"),
    PhoneBat        UMETA(DisplayName = "Phone Bat (AR)"),
    XboxController  UMETA(DisplayName = "Xbox Controller"),
    GenericGamepad  UMETA(DisplayName = "Generic Gamepad")
};

USTRUCT(BlueprintType)
struct FBattingInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    int32 SelectedShotType = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector2D FootworkDirection = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float FootworkMagnitude = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float BatFaceAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float WristRotation = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bTimingTriggerPressed = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float TimingTriggerPressure = 0.0f; // 0-1 for analog triggers

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bPowerModifierActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bAggressiveMode = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FTransform PhoneBatTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector PhoneBatLinearVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector PhoneBatAngularVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    int32 PhoneBatSwingPhase = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float PhoneBatContactPrediction = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float PhoneBatConfidence = 0.0f;
};

USTRUCT(BlueprintType)
struct FBowlingInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector RunUpMarkerLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bRunUpRhythmActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float RunUpRhythmTiming = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    int32 SelectedDeliveryType = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector2D AimOffset = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float ReleaseHeight = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float ReleaseAngle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector SeamOrientation = FVector::ForwardVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector SpinAxis = FVector::UpVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float SpinRPM = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float Effort = 0.5f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bBowlButtonPressed = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float BowlButtonPressure = 0.0f;
};

USTRUCT(BlueprintType)
struct FFieldingInputState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector2D MovementDirection = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    bool bDivePressed = false;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector2D DiveDirection = FVector2D::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    FVector ThrowTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    float ThrowPower = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Input")
    int32 SwitchFielderDirection = 0;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UBatInputInterface : public UInterface
{
    GENERATED_BODY()
};

class CRICKETSIMULATOR_API IBatInputInterface
{
    GENERATED_BODY()

public:
    // Device identification
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Input")
    EInputDeviceType GetDeviceType() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Input")
    FString GetDeviceName() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Input")
    bool IsConnected() const;

    // Batting input
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Batting")
    void GetBattingInputState(FBattingInputState& OutState) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Batting")
    void SetBattingInputState(const FBattingInputState& InState);

    // Bowling input
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Bowling")
    void GetBowlingInputState(FBowlingInputState& OutState) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Bowling")
    void SetBowlingInputState(const FBowlingInputState& InState);

    // Fielding input
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fielding")
    void GetFieldingInputState(FFieldingInputState& OutState) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Fielding")
    void SetFieldingInputState(const FFieldingInputState& InState);

    // Haptic feedback
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Haptics")
    void TriggerHapticFeedback(FGameplayTag FeedbackType, float Intensity = 1.0f);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Haptics")
    void TriggerAdaptiveTriggerResistance(FGameplayTag TriggerType, float Resistance = 1.0f);

    // Calibration
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Calibration")
    void StartCalibration();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Calibration")
    bool IsCalibrated() const;

    // Mapping contexts
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mapping")
    UInputMappingContext* GetBattingMappingContext() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mapping")
    UInputMappingContext* GetBowlingMappingContext() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Mapping")
    UInputMappingContext* GetFieldingMappingContext() const;
};