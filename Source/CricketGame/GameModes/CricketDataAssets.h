#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTags.h"
#include "CricketDataAssets.generated.h"

class UCurveFloat;
class UCurveVector;

// ============================================================================
// ENUMS FOR DATA TABLES
// ============================================================================

UENUM(BlueprintType)
enum class ECricketBallType : uint8
{
    RedBall         UMETA(DisplayName = "Red Ball (Test)"),
    WhiteBall       UMETA(DisplayName = "White Ball (ODI/T20)"),
    PinkBall        UMETA(DisplayName = "Pink Ball (Day/Night)"),
    TrainingBall    UMETA(DisplayName = "Training Ball")
};

UENUM(BlueprintType)
enum class EPitchType : uint8
{
    Standard          UMETA(DisplayName = "Standard"),
    GreenTop          UMETA(DisplayName = "Green Top (Seamer Friendly)"),
    DryDusty          UMETA(DisplayName = "Dry Dusty (Spinner Friendly)"),
    FlatBatting       UMETA(DisplayName = "Flat Batting Track"),
    Synthetic         UMETA(DisplayName = "Synthetic"),
    Indoor            UMETA(DisplayName = "Indoor")
};

UENUM(BlueprintType)
enum class EShotType : uint8
{
    None                UMETA(DisplayName = "None"),
    // Defensive
    ForwardDefense      UMETA(DisplayName = "Forward Defense"),
    BackwardDefense     UMETA(DisplayName = "Backward Defense"),
    Leave               UMETA(DisplayName = "Leave"),

    // Drives
    StraightDrive       UMETA(DisplayName = "Straight Drive"),
    CoverDrive          UMETA(DisplayName = "Cover Drive"),
    OffDrive            UMETA(DisplayName = "Off Drive"),
    OnDrive             UMETA(DisplayName = "On Drive"),
    SquareDrive         UMETA(DisplayName = "Square Drive"),

    // Cuts & Late Cuts
    LateCut             UMETA(DisplayName = "Late Cut"),
    UpperCut            UMETA(DisplayName = "Upper Cut"),
    ChineseCut          UMETA(DisplayName = "Chinese Cut (Edge)"),

    // Horizontal Bat
    PullShot            UMETA(DisplayName = "Pull Shot"),
    HookShot            UMETA(DisplayName = "Hook Shot"),
    CutShot             UMETA(DisplayName = "Cut Shot"),

    // Sweeps
    SweepShot           UMETA(DisplayName = "Sweep Shot"),
    ReverseSweep        UMETA(DisplayName = "Reverse Sweep"),
    PaddleSweep         UMETA(DisplayName = "Paddle Sweep"),

    // Lofted / Aerial
    LoftedDrive         UMETA(DisplayName = "Lofted Drive"),
    LoftedOnDrive       UMETA(DisplayName = "Lofted On Drive"),
    SlogSweep           UMETA(DisplayName = "Slog Sweep"),
    RampShot            UMETA(DisplayName = "Ramp Shot"),
    HelicopterShot      UMETA(DisplayName = "Helicopter Shot"),

    // Special
    Glance              UMETA(DisplayName = "Glance / Deflection"),
    Flick               UMETA(DisplayName = "Flick"),
    DriveOffPads        UMETA(DisplayName = "Drive Off Pads")
};

UENUM(BlueprintType)
enum class EDistrictType : uint8
{
    FastBowler      UMETA(DisplayName = "Fast Bowler"),
    MediumPacer     UMETA(DisplayName = "Medium Pacer"),
    SwingBowler     UMETA(DisplayName = "Swing Bowler"),
    SeamBowler      UMETA(DisplayName = "Seam Bowler"),
    OffSpinner      UMETA(DisplayName = "Off Spinner"),
    LegSpinner      UMETA(DisplayName = "Leg Spinner"),
    LeftArmOrthodox UMETA(DisplayName = "Left Arm Orthodox"),
    LeftArmChinaman UMETA(DisplayName = "Left Arm Chinaman"),
    PartTime        UMETA(DisplayName = "Part Time")
};

UENUM(BlueprintType)
enum class EDeliveryType : uint8
{
    None                UMETA(DisplayName = "None"),
    // Pace
    StockDelivery       UMETA(DisplayName = "Stock Ball"),
    InSwinger           UMETA(DisplayName = "Inswinger"),
    OutSwinger          UMETA(DisplayName = "Outswinger"),
    ReverseSwing        UMETA(DisplayName = "Reverse Swing"),
    Cutter              UMETA(DisplayName = "Cutter"),
    Yorker              UMETA(DisplayName = "Yorker"),
    Bouncer             UMETA(DisplayName = "Bouncer"),
    ShortBall           UMETA(DisplayName = "Short Ball"),
    SlowerBall          UMETA(DisplayName = "Slower Ball"),
    KnuckleBall         UMETA(DisplayName = "Knuckle Ball"),

    // Spin
    OffBreak            UMETA(DisplayName = "Off Break"),
    LegBreak            UMETA(DisplayName = "Leg Break"),
    Googly              UMETA(DisplayName = "Googly"),
    Doosra              UMETA(DisplayName = "Doosra"),
    TopSpinner          UMETA(DisplayName = "Top Spinner"),
    Flipper             UMETA(DisplayName = "Flipper"),
    Slider              UMETA(DisplayName = "Slider"),
    ArmBall             UMETA(DisplayName = "Arm Ball"),
    CarromBall          UMETA(DisplayName = "Carrom Ball"),

    // Special
    WideDeliberate      UMETA(DisplayName = "Deliberate Wide"),
    Beamer              UMETA(DisplayName = "Beamer (No Ball)")
};

UENUM(BlueprintType)
enum class EFieldingPosition : uint8
{
    // Close Catching
    Slip1           UMETA(DisplayName = "1st Slip"),
    Slip2           UMETA(DisplayName = "2nd Slip"),
    Slip3           UMETA(DisplayName = "3rd Slip"),
    Slip4           UMETA(DisplayName = "4th Slip"),
    Gully           UMETA(DisplayName = "Gully"),
    LegSlip         UMETA(DisplayName = "Leg Slip"),
    LegGully        UMETA(DisplayName = "Leg Gully"),
    ShortLeg        UMETA(DisplayName = "Short Leg"),
    SillyPoint      UMETA(DisplayName = "Silly Point"),
    SillyMidOff     UMETA(DisplayName = "Silly Mid-Off"),
    SillyMidOn      UMETA(DisplayName = "Silly Mid-On"),

    // Inner Ring
    Point           UMETA(DisplayName = "Point"),
    BackwardPoint   UMETA(DisplayName = "Backward Point"),
    Cover           UMETA(DisplayName = "Cover"),
    ExtraCover      UMETA(DisplayName = "Extra Cover"),
    MidOff          UMETA(DisplayName = "Mid-Off"),
    MidOn           UMETA(DisplayName = "Mid-On"),
    SquareLeg       UMETA(DisplayName = "Square Leg"),
    BackwardSquareLeg UMETA(DisplayName = "Backward Square Leg"),
    FineLeg         UMETA(DisplayName = "Fine Leg"),
    ThirdMan        UMETA(DisplayName = "Third Man"),

    // Boundary
    DeepPoint       UMETA(DisplayName = "Deep Point"),
    DeepCover       UMETA(DisplayName = "Deep Cover"),
    DeepExtraCover  UMETA(DisplayName = "Deep Extra Cover"),
    LongOff         UMETA(DisplayName = "Long Off"),
    LongOn          UMETA(DisplayName = "Long On"),
    DeepSquareLeg   UMETA(DisplayName = "Deep Square Leg"),
    DeepBackwardSquare UMETA(DisplayName = "Deep Backward Square"),
    DeepFineLeg     UMETA(DisplayName = "Deep Fine Leg"),
    DeepThirdMan    UMETA(DisplayName = "Deep Third Man"),
    CowCorner       UMETA(DisplayName = "Cow Corner"),

    // Wicketkeeper
    Wicketkeeper    UMETA(DisplayName = "Wicketkeeper")
};

UENUM(BlueprintType)
enum class EFieldingActionType : uint8
{
    None            UMETA(DisplayName = "None"),
    GroundFielding  UMETA(DisplayName = "Ground Fielding"),
    Catch           UMETA(DisplayName = "Catch"),
    Throw           UMETA(DisplayName = "Throw"),
    Dive            UMETA(DisplayName = "Dive"),
    Slide           UMETA(DisplayName = "Slide"),
    RunOut          UMETA(DisplayName = "Run Out"),
    Stumping        UMETA(DisplayName = "Stumping"),
    Wicketkeeping   UMETA(DisplayName = "Wicketkeeping")
};

UENUM(BlueprintType)
enum class EFieldingAction : uint8
{
    None            UMETA(DisplayName = "None"),
    Move            UMETA(DisplayName = "Move"),
    Sprint          UMETA(DisplayName = "Sprint"),
    Dive            UMETA(DisplayName = "Dive"),
    Slide           UMETA(DisplayName = "Slide"),
    Catch           UMETA(DisplayName = "Catch"),
    Throw           UMETA(DisplayName = "Throw"),
    RunOut          UMETA(DisplayName = "Run Out"),
    Stumping        UMETA(DisplayName = "Stumping")
};

UENUM(BlueprintType)
enum class EWicketkeeperStance : uint8
{
    Standing     UMETA(DisplayName = "Standing Up"),
    Crouching    UMETA(DisplayName = "Crouching"),
    SemiCrouch   UMETA(DisplayName = "Semi Crouch")
};

UENUM(BlueprintType)
enum class EFieldingOutcome : uint8
{
    Success     UMETA(DisplayName = "Success"),
    Partial     UMETA(DisplayName = "Partial"),
    Failed      UMETA(DisplayName = "Failed"),
    Dropped     UMETA(DisplayName = "Dropped"),
    Overthrow   UMETA(DisplayName = "Overthrow"),
    DirectHit   UMETA(DisplayName = "Direct Hit")
};

UENUM(BlueprintType)
enum class EBattingStyle : uint8
{
    RightHanded     UMETA(DisplayName = "Right Handed"),
    LeftHanded      UMETA(DisplayName = "Left Handed")
};

UENUM(BlueprintType)
enum class EBowlingArm : uint8
{
    RightArm        UMETA(DisplayName = "Right Arm"),
    LeftArm         UMETA(DisplayName = "Left Arm")
};

UENUM(BlueprintType)
enum class EBowlingAction : uint8
{
    FrontOn         UMETA(DisplayName = "Front On"),
    SideOn          UMETA(DisplayName = "Side On"),
    Mixed           UMETA(DisplayName = "Mixed"),
    Slingy          UMETA(DisplayName = "Slingy")
};

// ============================================================================
// DATA STRUCTURES FOR DATA TABLES
// ============================================================================

USTRUCT(BlueprintType)
struct FCricketBallPhysicsRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName BallId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECricketBallType BallType = ECricketBallType::RedBall;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float Mass = 0.156f; // kg (156g)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float Radius = 0.036f; // meters (36mm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float SeamHeight = 0.001f; // meters (1mm)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float DragCoefficientBase = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float DragCoefficientSwing = 0.6f; // When seam angled

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float MagnusCoefficient = 1.2e-4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float LiftCoefficient = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear")
    float WearRatePerOver = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear")
    float SeamDegradationRate = 0.0005f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear")
    float PolishRetention = 0.8f; // How well ball keeps shine

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
    float RestitutionNew = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
    float RestitutionOld = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
    float FrictionNew = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounce")
    float FrictionOld = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* BallMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* SwingTrailEffect = nullptr;
};

USTRUCT(BlueprintType)
struct FPitchProfileRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PitchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPitchType PitchType = EPitchType::FlatBatting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Properties")
    float BaseRestitution = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Properties")
    float BaseFriction = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Properties")
    float SeamMovement = 1.0f; // Multiplier

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Properties")
    float SpinAssistance = 1.0f; // Multiplier

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Properties")
    float PaceRetention = 0.85f; // Speed after bounce

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Settings")
    float WearRateBowlerEnd = 0.02f; // Per over
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Settings")
    float WearRateBatterEnd = 0.015f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Settings")
    float WearRateMiddle = 0.005f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Settings")
    float CrackFormationThreshold = 0.6f; // Wear level for cracks

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Settings")
    float FootmarkDepth = 0.02f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night")
    float DayRestitutionMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night")
    float NightRestitutionMultiplier = 1.1f; // Dew factor

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* PitchMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TArray<UMaterialInterface*> WearDecals; // Progressive wear decals

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* DustEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* CrackEffect = nullptr;
};

USTRUCT(BlueprintType)
struct FPlayerAttributesRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PlayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlayerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TeamId = 0;

    // Batting Attributes (0-100)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingTechnique = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingPower = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingTiming = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingFootwork = 65;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingTemperament = 70; // Pressure handling

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    int32 BattingConcentration = 70; // Long innings ability

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    EBattingStyle BattingStyle = EBattingStyle::RightHanded;

    // Shot Preferences (0-100 affinity)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Preferences")
    TMap<EShotType, int32> ShotAffinities;

    // Bowling Attributes (0-100)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingPace = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingSwing = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingSeam = 60;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingSpin = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingControl = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingStamina = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    int32 BowlingVariations = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    EBowlingArm BowlingArm = EBowlingArm::RightArm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    EBowlingAction BowlingAction = EBowlingAction::SideOn;

    // Delivery Preferences
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    TMap<EDeliveryType, int32> DeliveryAffinities;

    // Fielding Attributes (0-100)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    int32 FieldingAgility = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    int32 FieldingReflexes = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    int32 FieldingThrowing = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    int32 FieldingCatching = 70;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    int32 FieldingAnticipation = 70;

    // Wicketkeeping Specific
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    int32 WKKeeping = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    int32 WKStumping = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicketkeeping")
    int32 WKReactions = 50;

    // Physical
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical")
    float Height = 1.80f; // meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical")
    float Weight = 80.0f; // kg

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical")
    int32 Age = 25;

    // Roles
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
    bool bIsBatsman = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
    bool bIsBowler = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
    bool bIsAllRounder = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
    bool bIsWicketkeeper = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Role")
    bool bIsCaptain = false;
};

USTRUCT(BlueprintType)
struct FShotDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ShotId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EShotType ShotType = EShotType::ForwardDefense;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    bool bIsAttacking = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    bool bIsAerial = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    bool bRequiresFrontFoot = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot Properties")
    bool bRequiresBackFoot = false;

    // Footwork
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    FVector2D FrontFootOffset = FVector2D(0.0f, 0.0f); // X=forward/back, Y=off/leg

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    FVector2D BackFootOffset = FVector2D(0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footwork")
    float WeightTransfer = 0.5f; // 0=back foot, 1=front foot

    // Bat Angle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat Angle")
    float BatFaceAngle = 0.0f; // Degrees, 0=straight, +ve=closed, -ve=open

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat Angle")
    float WristRotation = 0.0f; // Degrees

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat Angle")
    float FollowThroughAngle = 90.0f; // Degrees

    // Timing Windows (seconds relative to ideal contact)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float PerfectWindowEarly = -0.02f; // 20ms early
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float PerfectWindowLate = 0.02f;   // 20ms late

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float GoodWindowEarly = -0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float GoodWindowLate = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float EarlyWindowEarly = -0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float EarlyWindowLate = -0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float LateWindowEarly = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    float LateWindowLate = 0.1f;

    // Power & Placement Modifiers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float PowerMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float ControlMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float PlacementSpread = 15.0f; // Degrees of variation

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float EdgeProbability = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outcome")
    float MissProbability = 0.1f;

    // Suitable Delivery Types
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suitability")
    TArray<EDeliveryType> SuitableDeliveries;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Suitability")
    TArray<EDeliveryType> UnsuitableDeliveries;

    // Animation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* ShotMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    float MontagePlayRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    FName StartSectionName;

    // Audio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* ContactSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* EdgeSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* MissSound = nullptr;

    // Visual Feedback
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* ContactEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* EdgeEffect = nullptr;
};

USTRUCT(BlueprintType)
struct FDeliveryTypeRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName DeliveryId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDeliveryType DeliveryType = EDeliveryType::StockDelivery;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery Properties")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery Properties")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery Properties")
    EDistrictType SuitableBowlerType = EDistrictType::FastBowler;

    // Physics Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float PaceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SwingAmount = 0.0f; // Degrees of swing

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SeamMovement = 0.0f; // Degrees of seam deviation

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float SpinRPM = 0.0f; // Revolutions per minute

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    FVector SpinAxis = FVector(0.0f, 0.0f, 1.0f); // Normalized

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float ReleaseHeight = 2.2f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float ReleaseAngle = -5.0f; // Degrees (negative = down)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float WristPosition = 0.0f; // Degrees

    // Control
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float ControlDifficulty = 0.5f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float StaminaCost = 1.0f; // Multiplier

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Control")
    float AccuracyRadius = 0.3f; // Meters at pitch

    // Run-up
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    float RunupSpeed = 6.0f; // m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    int32 RunupSteps = 15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run-up")
    float RhythmTolerance = 0.1f; // Timing window for run-up

    // Wicket Taking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicket Taking")
    float WicketProbability = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wicket Taking")
    TArray<EShotType> InducedShots; // Shots this delivery encourages

    // Visual
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* TrailEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* BallMaterialOverride = nullptr;

    // Audio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* ReleaseSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* FlightSound = nullptr;
};

USTRUCT(BlueprintType)
struct FFieldingPositionRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName PositionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFieldingPosition Position = EFieldingPosition::Point;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    FVector WorldOffset = FVector::ZeroVector; // Relative to striker's end

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    float DistanceFromStriker = 20.0f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    float AngleFromStriker = 0.0f; // Degrees (0=straight, 90=off side)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    bool bIsCloseCatching = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    bool bIsBoundary = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Position")
    bool bIsWicketkeeper = false;

    // Required Attributes (minimum values)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
    int32 MinAgility = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
    int32 MinReflexes = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
    int32 MinThrowing = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
    int32 MinCatching = 50;

    // Reaction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float ReactionTime = 0.2f; // Seconds

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float InterceptionRadius = 3.0f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float DiveProbability = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float ThrowAccuracy = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
    float ThrowPower = 0.7f;

    // Animation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* IdleAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* MoveAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* DiveAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* ThrowAnimation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
    UAnimMontage* CatchAnimation = nullptr;
};

USTRUCT(BlueprintType)
struct FStadiumDataRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName StadiumId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StadiumName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Capacity = 50000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    FName DefaultPitchProfile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    TArray<FName> AvailablePitchProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float StraightBoundary = 75.0f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float SquareBoundary = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float FineLegBoundary = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boundary")
    float ThirdManBoundary = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
    float DefaultWindSpeed = 0.0f; // m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
    float DefaultWindDirection = 0.0f; // Degrees

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
    float DefaultHumidity = 60.0f; // Percent

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
    float DefaultTemperature = 25.0f; // Celsius

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
    float DefaultCloudCover = 0.3f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    ULevel* StadiumLevel = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TArray<UStaticMesh*> StandsMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* GrassMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* PitchMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UTexture2D* BoundaryRopeTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UTexture2D* SightScreenTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* CrowdAmbience = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* CrowdCheer = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* CrowdApplause = nullptr;
};

USTRUCT(BlueprintType)
struct FTeamDataRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TeamId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TeamName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ShortName; // 3-4 chars

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor PrimaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SecondaryColor = FLinearColor::Blue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor TertiaryColor = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* TeamLogo = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* KitTexture = nullptr; // Jersey texture

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> PlayerIds; // References to PlayerAttributes table

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategy")
    float AggressionBatting = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategy")
    float AggressionBowling = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategy")
    float AggressionFielding = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategy")
    TMap<EFieldingPosition, FName> DefaultFieldPlacement; // Position -> PlayerId
};

// ============================================================================
// DATA ASSETS (For complex configurable objects)
// ============================================================================

UCLASS(BlueprintType)
class CRICKETGAME_API UCricketBallPhysicsProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball Type")
    ECricketBallType BallType = ECricketBallType::RedBall;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float Mass = 0.156f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float Radius = 0.036f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float SeamHeight = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physical Properties")
    float MomentOfInertia = 0.0001f; // kg⋅m²

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    UCurveFloat* DragCoefficientVsSpeed; // Cd vs Reynolds number

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    UCurveFloat* MagnusCoefficientVsSpin; // Cl vs spin parameter

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    UCurveFloat* LiftCoefficientVsSeamAngle; // Cl vs seam angle

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float CriticalReynoldsNumber = 120000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float SuperCriticalDragCoefficient = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    UCurveFloat* RestitutionVsWear; // Restitution vs wear level (0-1)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    UCurveFloat* FrictionVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    UCurveFloat* SeamHeightVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    UCurveFloat* PolishVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    float WearRatePerOver = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Simulation")
    float WearRatePerImpact = 0.0001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* BallMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* BallMaterialWorn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* SwingTrailEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* SpinTrailEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* ImpactSoundBat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* ImpactSoundPitch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* ImpactSoundStumps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* WhooshSound;
};

UCLASS(BlueprintType)
class CRICKETGAME_API UPitchProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Type")
    EPitchType PitchType = EPitchType::FlatBatting;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Type")
    FString DisplayName;

    // Base Physics
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float BaseRestitution = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float BaseFriction = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float SeamMovementMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float SpinAssistanceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float PaceRetention = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Physics")
    float LateralDeviation = 0.0f; // Inherent pitch deviation

    // Zone Multipliers (5x3 grid)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Physics")
    TArray<float> ZoneRestitutionMultipliers; // 15 values

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Physics")
    TArray<float> ZoneFrictionMultipliers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Physics")
    TArray<float> ZoneSeamMultipliers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Physics")
    TArray<float> ZoneSpinMultipliers;

    // Wear Evolution
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    UCurveFloat* WearVsOvers; // Wear level vs overs bowled

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    UCurveFloat* RestitutionVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    UCurveFloat* FrictionVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    UCurveFloat* SeamVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    UCurveFloat* SpinVsWear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    float CrackFormationThreshold = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    float MaxCrackDepth = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wear Evolution")
    float FootmarkFormationRate = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night Cycle")
    UCurveFloat* RestitutionVsTimeOfDay; // 0=midnight, 0.5=noon, 1=midnight

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night Cycle")
    UCurveFloat* FrictionVsTimeOfDay;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night Cycle")
    UCurveFloat* SwingVsHumidity; // Humidity 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day/Night Cycle")
    float DewFactor = 1.1f; // Night multiplier

    // Visual
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* PitchMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TArray<UMaterialInterface*> WearDecalMaterials; // 0-100% wear

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    TArray<UMaterialInterface*> CrackDecalMaterials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UMaterialInterface* FootmarkDecalMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* DustKickupEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    UParticleSystem* CrackEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* BallBounceSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    USoundBase* BallBounceWornSound;
};

UCLASS(BlueprintType)
class CRICKETGAME_API UCricketGameplayConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // Match Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 DefaultOversPerInnings = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 MaxOversPerBowler = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 PowerplayOvers = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 PowerplayFieldersOutside = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 MiddleOversFieldersOutside = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 DeathOversFieldersOutside = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    int32 DRSReviewsPerInnings = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Match Settings")
    float DRSReviewTimeout = 15.0f; // Seconds

    // Physics
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float BallPhysicsSubstep = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    int32 MaxPhysicsSubsteps = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float Gravity = 980.0f; // cm/s²

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
    float AirDensity = 1.225f; // kg/m³

    // Batting
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float BaseTimingWindow = 0.04f; // ±20ms perfect window

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float MinTimingWindow = 0.01f; // ±5ms at max difficulty

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float MaxTimingWindow = 0.1f;  // ±50ms at min difficulty

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float SweetSpotRadius = 0.05f; // 5cm from center

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float EdgeZoneWidth = 0.02f; // 2cm edge zone

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float MaxBatSpeed = 40.0f; // m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting")
    float BatMass = 1.2f; // kg

    // Bowling
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float MaxBowlingSpeed = 45.0f; // m/s (162 km/h)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float MinBowlingSpeed = 15.0f; // m/s (54 km/h)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float MaxSpinRPM = 3500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float NoBallFrontFootMargin = 0.01f; // 1cm tolerance

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float WideLineOffSide = 0.85f; // Meters from off stump

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
    float WideLineLegSide = 0.45f; // Meters from leg stump

    // Fielding
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    float MaxThrowSpeed = 35.0f; // m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    float BaseCatchProbability = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    float DiveReachExtension = 1.5f; // Meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fielding")
    float ReactionTimeBase = 0.2f; // Seconds

    // DRS
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS")
    float HawkeyePredictionTime = 2.0f; // Seconds forward

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS")
    float HawkeyeConfidenceThreshold = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS")
    float UltraEdgeSensitivity = 0.1f; // Audio threshold

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRS")
    float HotSpotThreshold = 0.3f; // Thermal threshold

    // Phone Bat
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat")
    float PhoneBatUpdateRate = 60.0f; // Hz

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat")
    float MaxPhoneLatency = 100.0f; // ms

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat")
    float JitterBufferFrames = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat")
    float CalibrationSwingSpeedThreshold = 5.0f; // m/s

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phone Bat")
    float SwingDetectionConfidenceThreshold = 0.7f;

    // Replay
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
    float ReplayRecordRate = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
    int32 MaxReplayFrames = 36000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
    float KeyframeInterval = 1.0f;

    // Data Table References
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* BallPhysicsTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* PitchProfileTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* PlayerAttributesTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* ShotDefinitionsTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* DeliveryTypesTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* FieldingPositionsTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* StadiumsTable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Tables")
    UDataTable* TeamsTable;

    // Data Asset References
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Assets")
    TArray<UCricketBallPhysicsProfile*> BallPhysicsProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data Assets")
    TArray<UPitchProfile*> PitchProfiles;
};