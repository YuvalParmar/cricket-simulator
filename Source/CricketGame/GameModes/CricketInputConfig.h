#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "CricketInputConfig.generated.h"

UCLASS(BlueprintType, Const)
class CRICKETSIMULATOR_API UCricketInputConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // Common Actions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common")
    TObjectPtr<UInputAction> ToggleCameraAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common")
    TObjectPtr<UInputAction> PauseAction;

    // Batting Actions (Keyboard/Mouse + DualSense + Phone)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Batting")
    TObjectPtr<UInputAction> ShotTypeAction;        // Digital: 1-9 / D-Pad

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Batting")
    TObjectPtr<UInputAction> FootworkAction;        // Axis2D: WASD / Left Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Batting")
    TObjectPtr<UInputAction> BatAngleAction;        // Axis2D: Mouse / Right Stick (X=Face, Y=Wrist)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Batting")
    TObjectPtr<UInputAction> TimingTriggerAction;   // Digital: Space / R2 (pressure sensitive)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Batting")
    TObjectPtr<UInputAction> PowerModAction;        // Digital: Shift / L2

    // Bowling Actions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> RunUpMarkerAction;     // Digital: Click / X button

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> RunUpRhythmAction;     // Digital: Space / R2 (rhythm taps)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> DeliveryTypeAction;    // Digital: 1-6 / D-Pad

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> AimAction;             // Axis2D: Mouse / Right Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> ReleasePointAction;    // Axis2D: Mouse Y / Right Stick Y

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> SeamOrientationAction; // Axis3D: Phone gyro / Right Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> SpinAxisAction;        // Axis3D: Phone gyro / Right Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> EffortAction;          // Axis1D: Scroll / L2/R2 pressure

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bowling")
    TObjectPtr<UInputAction> BowlAction;            // Digital: Click / R2 (hold for run-up, release for delivery)

    // Fielding Actions
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fielding")
    TObjectPtr<UInputAction> MoveAction;            // Axis2D: WASD / Left Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fielding")
    TObjectPtr<UInputAction> DiveAction;            // Digital+Axis2D: Shift+Dir / L1+Dir

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fielding")
    TObjectPtr<UInputAction> ThrowAction;           // Axis3D: Mouse aim + RMB / R2 + Right Stick

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fielding")
    TObjectPtr<UInputAction> SwitchFielderAction;   // Digital: Tab / R1/L1

    // Phone Bat Actions (received via network, not direct input)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PhoneBat")
    TObjectPtr<UInputAction> PhoneBatPoseAction;    // Axis3D: Phone pose (network driven)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PhoneBat")
    TObjectPtr<UInputAction> PhoneBatSwingAction;   // Digital: Swing phase (network driven)

    // Mapping Contexts
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MappingContexts")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MappingContexts")
    TObjectPtr<UInputMappingContext> BattingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MappingContexts")
    TObjectPtr<UInputMappingContext> BowlingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MappingContexts")
    TObjectPtr<UInputMappingContext> FieldingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MappingContexts")
    TObjectPtr<UInputMappingContext> PhoneBatMappingContext;
};