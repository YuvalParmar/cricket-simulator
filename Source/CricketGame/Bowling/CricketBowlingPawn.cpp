#include "CricketBowlingPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/ArrowComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketBall.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "CricketGameplayAbilities.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

ACricketBowlingPawn::ACricketBowlingPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    // Capsule
    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    CapsuleComponent->InitCapsuleSize(42.0f, 96.0f);
    CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));
    RootComponent = CapsuleComponent;

    // Body mesh
    BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(RootComponent);
    BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    BodyMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    BodyMesh->SetCollisionProfileName(TEXT("CharacterMesh"));

    // Camera
    CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
    CameraSpringArm->SetupAttachment(RootComponent);
    CameraSpringArm->TargetArmLength = 500.0f;
    CameraSpringArm->SetRelativeRotation(FRotator(-20.0f, 0.0f, 0.0f));
    CameraSpringArm->bUsePawnControlRotation = true;
    CameraSpringArm->bInheritPitch = true;
    CameraSpringArm->bInheritYaw = true;
    CameraSpringArm->bInheritRoll = false;
    CameraSpringArm->bDoCollisionTest = true;
    CameraSpringArm->CameraLagSpeed = 3.0f;

    BowlingCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BowlingCamera"));
    BowlingCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
    BowlingCamera->bUsePawnControlRotation = false;

    // GAS Components
    AbilitySystemComponent = CreateDefaultSubobject<UCricketAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UCricketAttributeSet>(TEXT("AttributeSet"));
    AbilitySystemComponent->AddAttributeSet(AttributeSet);

    // Markers
    RunUpMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("RunUpMarker"));
    RunUpMarker->SetupAttachment(RootComponent);
    RunUpMarker->SetHiddenInGame(true);
    RunUpMarker->ArrowSize = 2.0f;
    RunUpMarker->ArrowColor = FColor::Green;

    TargetMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("TargetMarker"));
    TargetMarker->SetupAttachment(RootComponent);
    TargetMarker->SetHiddenInGame(true);
    TargetMarker->ArrowSize = 1.5f;
    TargetMarker->ArrowColor = FColor::Red;

    // Component
    BowlingComponent = CreateDefaultSubobject<UCricketBowlingComponent>(TEXT("BowlingComponent"));

    // Defaults
    CurrentPhase = EBowlingPhase::SettingField;
    RunUpProgress = 0.0f;
    TargetRunUpDistance = 2000.0f; // 20 meters in cm
    ReleaseHeight = 220.0f; // cm
    ReleaseAngle = 0.0f;
    Effort = 0.5f;
    bBowlButtonPressed = false;
    BowlButtonPressure = 0.0f;
    RhythmTapCount = 0;
    RhythmTimingScore = 1.0f;
    LastRhythmTapTime = 0.0f;
    CurrentStamina = 100.0f;
    MaxStamina = 100.0f;
    PressureLevel = 0.0f;
    FormLevel = 0.0f;
    FatigueLevel = 0.0f;
}

void ACricketBowlingPawn::BeginPlay()
{
    Super::BeginPlay();

    // Initialize GAS
    InitializeAttributeSet();
    GrantDefaultAbilities();
    InitializeAbilities();

    // Load delivery definitions from DataTable if provided
    if (DeliveryDefinitionsTable)
    {
        TArray<FDeliveryDefinition*> Rows;
        DeliveryDefinitionsTable->GetAllRows(TEXT(""), Rows);
        for (FDeliveryDefinition* Row : Rows)
        {
            if (Row)
            {
                DeliveryDefinitions.Add(*Row);
            }
        }
    }

    // Set default delivery if none selected
    if (DeliveryDefinitions.Num() > 0)
    {
        SelectedDeliveryType = DeliveryDefinitions[0].DeliveryType;
    }

    // Position at bowling crease initially
    SetActorLocation(FVector(-1000.0f, 0.0f, 0.0f)); // Behind bowling crease
}

UAbilitySystemComponent* ACricketBowlingPawn::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACricketBowlingPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateBowlingState(DeltaSeconds);
    UpdateCamera(DeltaSeconds);
    UpdateGASAttributes(DeltaTime);
}

void ACricketBowlingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Enhanced input bindings handled by PlayerController
}

void ACricketBowlingPawn::SetRunUpMarker(const FVector& Location)
{
    RunUpMarkerLocation = Location;
    RunUpMarker->SetWorldLocation(Location);
    RunUpMarker->SetHiddenInGame(false);

    // Calculate run-up distance
    FVector StartLoc = GetActorLocation();
    RunUpDistance = FVector::Dist(StartLoc, Location);
    TargetRunUpDistance = RunUpDistance;

    UE_LOG(LogTemp, Log, TEXT("Run-up marker set at distance: %.1fm"), RunUpDistance / 100.0f);
}

void ACricketBowlingPawn::SelectDeliveryType(int32 DeliveryTypeIndex)
{
    if (DeliveryTypeIndex >= 0 && DeliveryTypeIndex < DeliveryDefinitions.Num())
    {
        SelectedDeliveryType = DeliveryDefinitions[DeliveryTypeIndex].DeliveryType;
        UE_LOG(LogTemp, Log, TEXT("Selected delivery type: %s"), *UEnum::GetValueAsString(SelectedDeliveryType));
    }
}

void ACricketBowlingPawn::SetAimOffset(const FVector2D& Offset)
{
    AimOffset = Offset;

    // Update target marker
    FVector TargetLoc = GetActorLocation() + FVector(2000.0f, Offset.X * 100.0f, Offset.Y * 100.0f);
    TargetMarker->SetWorldLocation(TargetLoc);
    TargetMarker->SetHiddenInGame(false);
}

void ACricketBowlingPawn::SetReleasePoint(float Height, float Angle)
{
    ReleaseHeight = FMath::Clamp(Height, 150.0f, 300.0f); // 1.5m to 3.0m
    ReleaseAngle = FMath::Clamp(Angle, -30.0f, 30.0f);
}

void ACricketBowlingPawn::SetSeamOrientation(const FVector& Orientation)
{
    SeamOrientation = Orientation.GetSafeNormal();
}

void ACricketBowlingPawn::SetSpinAxis(const FVector& Axis, float RPM)
{
    SpinAxis = Axis.GetSafeNormal();
    SpinRPM = FMath::Max(RPM, 0.0f);
}

void ACricketBowlingPawn::SetEffort(float InEffort)
{
    Effort = FMath::Clamp(InEffort, 0.0f, 1.0f);
}

void ACricketBowlingPawn::OnRunUpRhythmTap()
{
    float CurrentTime = GetWorld()->GetTimeSeconds();

    if (LastRhythmTapTime > 0.0f)
    {
        float Interval = CurrentTime - LastRhythmTapTime;
        // Ideal interval ~0.5s for running rhythm
        float IdealInterval = 0.5f;
        float Deviation = FMath::Abs(Interval - IdealInterval);
        float TapScore = FMath::Max(0.0f, 1.0f - Deviation / IdealInterval);

        RhythmTimingScore = (RhythmTimingScore * RhythmTapCount + TapScore) / (RhythmTapCount + 1);
    }

    RhythmTapCount++;
    LastRhythmTapTime = CurrentTime;

    // Haptic feedback for rhythm
    // Would trigger via PlayerController
}

void ACricketBowlingPawn::OnBowlButtonPressed()
{
    if (CurrentPhase == EBowlingPhase::SettingField || CurrentPhase == EBowlingPhase::RunUpMarker)
    {
        bBowlButtonPressed = true;
        BowlButtonPressure = 1.0f;

        // Start run-up
        if (RunUpMarkerLocation != FVector::ZeroVector)
        {
            SetPhase(EBowlingPhase::RunUp);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("No run-up marker set!"));
        }
    }
    else if (CurrentPhase == EBowlingPhase::RunUp)
    {
        BowlButtonPressure = 1.0f;
    }
}

void ACricketBowlingPawn::OnBowlButtonReleased()
{
    bBowlButtonPressed = false;
    BowlButtonPressure = 0.0f;

    if (CurrentPhase == EBowlingPhase::RunUp)
    {
        // Release the ball
        ExecuteDelivery();
    }
}

void ACricketBowlingPawn::CancelDelivery()
{
    if (CurrentPhase == EBowlingPhase::RunUp || CurrentPhase == EBowlingPhase::Release)
    {
        SetPhase(EBowlingPhase::SettingField);
        RunUpProgress = 0.0f;
        RhythmTapCount = 0;
        RhythmTimingScore = 1.0f;
        LastRhythmTapTime = 0.0f;
    }
}

void ACricketBowlingPawn::UpdateBowlingState(float DeltaSeconds)
{
    switch (CurrentPhase)
    {
        case EBowlingPhase::RunUp:
            UpdateRunUp(DeltaSeconds);
            break;
        case EBowlingPhase::Release:
            UpdateRelease(DeltaSeconds);
            break;
        case EBowlingPhase::FollowThrough:
            // Animation plays, then return to idle
            break;
        default:
            break;
    }
}

void ACricketBowlingPawn::UpdateRunUp(float DeltaSeconds)
{
    // Move towards run-up marker
    FVector CurrentLoc = GetActorLocation();
    FVector Direction = (RunUpMarkerLocation - CurrentLoc).GetSafeNormal2D();
    float DistanceToMarker = FVector::Dist2D(CurrentLoc, RunUpMarkerLocation);

    // Run-up speed based on effort and delivery type
    float BaseSpeed = 600.0f; // cm/s (6 m/s)
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition();
    if (DelDef)
    {
        BaseSpeed = FMath::Lerp(400.0f, 800.0f, Effort); // 4-8 m/s
    }

    // Rhythm affects speed consistency
    float SpeedMultiplier = 0.8f + RhythmTimingScore * 0.4f; // 0.8 to 1.2

    FVector Velocity = Direction * BaseSpeed * SpeedMultiplier;
    FVector NewLoc = CurrentLoc + Velocity * DeltaSeconds;

    // Check if reached marker
    if (DistanceToMarker < BaseSpeed * DeltaSeconds * 1.5f)
    {
        NewLoc = RunUpMarkerLocation;
        SetPhase(EBowlingPhase::Release);
    }

    SetActorLocation(NewLoc);

    // Update progress
    float TotalDist = FVector::Dist2D(GetActorLocation(), RunUpMarkerLocation);
    RunUpProgress = 1.0f - (DistanceToMarker / TargetRunUpDistance);
    RunUpProgress = FMath::Clamp(RunUpProgress, 0.0f, 1.0f);

    OnRunUpProgress.Broadcast(RunUpProgress, NewLoc);
}

void ACricketBowlingPawn::UpdateRelease(float DeltaSeconds)
{
    // Release phase is very brief - ball is spawned and released
    // The actual release happens in ExecuteDelivery which transitions to FollowThrough
}

void ACricketBowlingPawn::ExecuteDelivery()
{
    if (CurrentPhase != EBowlingPhase::RunUp && CurrentPhase != EBowlingPhase::Release)
    {
        return;
    }

    SetPhase(EBowlingPhase::Release);

    // Calculate delivery parameters
    FVector ReleaseLoc = CalculateReleaseLocation();
    FVector ReleaseVel = CalculateReleaseVelocity();

    // Spawn ball
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();

    ACricketBall* Ball = GetWorld()->SpawnActor<ACricketBall>(ACricketBall::StaticClass(), ReleaseLoc, FRotator::ZeroRotator, SpawnParams);

    if (Ball)
    {
        // Initialize ball with delivery parameters
        FBallPhysicsParams BallParams;
        BallParams.Mass = 0.156f;
        BallParams.Radius = 0.036f;

        Ball->Initialize(EBallType::RedBall, BallParams);
        Ball->ReleaseFromHand(ReleaseLoc, ReleaseVel, SpinAxis * SpinRPM * (PI / 30.0f)); // RPM to rad/s

        // Set seam orientation
        Ball->PhysicsState.SeamOrientation = SeamOrientation;
    }

    // Calculate result
    FDeliveryResult Result;
    CalculateDeliveryResult(Result);
    LastDeliveryResult = Result;

    // Check for no-ball
    CheckNoBall(ReleaseLoc);

    // Broadcast event
    OnDeliveryBowled.Broadcast(Result);

    // Notify game mode
    if (ACricketGameMode* GameMode = Cast<ACricketGameMode>(GetWorld()->GetAuthGameMode()))
    {
        GameMode->OnBallDelivered(Ball);
    }

    // Play animation
    PlayDeliveryAnimation(SelectedDeliveryType, 1.0f);

    // Transition to follow through
    SetPhase(EBowlingPhase::FollowThrough);

    // Reset for next ball after delay
    FTimerHandle ResetTimer;
    GetWorldTimerManager().SetTimer(ResetTimer, [this]()
    {
        SetPhase(EBowlingPhase::SettingField);
        RunUpProgress = 0.0f;
        RhythmTapCount = 0;
        RhythmTimingScore = 1.0f;
        LastRhythmTapTime = 0.0f;
        bBowlButtonPressed = false;
        BowlButtonPressure = 0.0f;
    }, 3.0f, false);
}

void ACricketBowlingPawn::CalculateDeliveryResult(FDeliveryResult& OutResult)
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition();

    OutResult.DeliveryType = SelectedDeliveryType;

    if (DelDef)
    {
        // Actual speed varies based on effort, timing, rhythm
        float SpeedVariation = FMath::RandRange(-5.0f, 5.0f); // ±5 km/h random
        float EffortMultiplier = FMath::Lerp(0.8f, 1.0f, Effort);
        float RhythmMultiplier = 0.9f + RhythmTimingScore * 0.2f; // 0.9 to 1.1

        OutResult.ActualSpeed = DelDef->BaseSpeed * EffortMultiplier * RhythmMultiplier + SpeedVariation;
        OutResult.ActualSpeed = FMath::Clamp(OutResult.ActualSpeed, DelDef->BaseSpeed * 0.7f, DelDef->MaxSpeed);

        // Spin
        OutResult.ActualSpinRPM = DelDef->SpinRPM * EffortMultiplier;
        OutResult.ActualSpinAxis = SpinAxis;

        // Swing
        OutResult.ActualSwing = DelDef->SwingAmount * (1.0f - DelDef->SeamPosition * 0.5f); // Seam affects swing

        // Release location
        OutResult.ReleaseLocation = CalculateReleaseLocation();
        OutResult.ReleaseVelocity = CalculateReleaseVelocity();

        // Predicted pitch location (simplified)
        OutResult.PredictedPitchLocation = OutResult.ReleaseLocation + OutResult.ReleaseVelocity.GetSafeNormal2D() * 1800.0f; // ~18m
        OutResult.PredictedPitchLocation.Z = 0.0f;

        // Timing score (rhythm + button release timing)
        OutResult.TimingScore = RhythmTimingScore;

        // Accuracy score (aim + timing)
        float AimAccuracy = 1.0f - FMath::Clamp(AimOffset.Size() / 100.0f, 0.0f, 1.0f);
        OutResult.AccuracyScore = (AimAccuracy + RhythmTimingScore) * 0.5f;

        // No-ball check
        OutResult.bNoBall = false; // Set by CheckNoBall

        // Wide check (simplified)
        OutResult.bWide = FMath::Abs(AimOffset.X) > 80.0f; // Outside off stump
    }
}

void ACricketBowlingPawn::PlayDeliveryAnimation(EDeliveryType DeliveryType, float PlayRate)
{
    if (!BodyMesh || !BodyMesh->GetAnimInstance())
    {
        return;
    }

    for (const FDeliveryDefinition& Def : DeliveryDefinitions)
    {
        if (Def.DeliveryType == DeliveryType && !Def.AnimationMontageName.IsNone())
        {
            UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Def.AnimationMontageName.ToString());
            if (Montage)
            {
                BodyMesh->GetAnimInstance()->Montage_Play(Montage, PlayRate);
                break;
            }
        }
    }
}

void ACricketBowlingPawn::UpdateCamera(float DeltaSeconds)
{
    // Camera follows bowler during run-up, then switches to ball view
    if (CurrentPhase == EBowlingPhase::RunUp)
    {
        // Camera behind bowler
        CameraSpringArm->TargetArmLength = FMath::FInterpTo(CameraSpringArm->TargetArmLength, 400.0f, DeltaSeconds, 5.0f);
    }
    else if (CurrentPhase == EBowlingPhase::Release || CurrentPhase == EBowlingPhase::FollowThrough)
    {
        // Camera transitions to follow ball
        CameraSpringArm->TargetArmLength = FMath::FInterpTo(CameraSpringArm->TargetArmLength, 800.0f, DeltaSeconds, 3.0f);
    }
    else
    {
        // Default
        CameraSpringArm->TargetArmLength = FMath::FInterpTo(CameraSpringArm->TargetArmLength, 500.0f, DeltaSeconds, 5.0f);
    }
}

void ACricketBowlingPawn::CheckNoBall(const FVector& FrontFootLocation)
{
    // No-ball if front foot lands beyond popping crease
    // Popping crease is 1.22m (122cm) in front of stumps
    // Stumps at X=0, bowling crease at X=-122cm, popping crease at X=122cm

    float PoppingCreaseX = 122.0f; // cm from stumps

    // Convert to world coordinates (assuming stumps at origin)
    // This is simplified - real implementation would use actual stump locations
    if (FrontFootLocation.X > PoppingCreaseX)
    {
        LastDeliveryResult.bNoBall = true;
        UE_LOG(LogTemp, Log, TEXT("NO BALL! Front foot at X=%.1f, crease at %.1f"), FrontFootLocation.X, PoppingCreaseX);
    }
}

FVector ACricketBowlingPawn::CalculateReleaseLocation() const
{
    FVector Location = GetActorLocation();
    Location.Z = ReleaseHeight; // Set release height

    // Add small offset for arm extension
    FVector Forward = GetActorForwardVector();
    Location += Forward * 50.0f; // 50cm in front of body

    return Location;
}

FVector ACricketBowlingPawn::CalculateReleaseVelocity() const
{
    const FDeliveryDefinition* DelDef = GetDeliveryDefinition();
    float SpeedKMH = DelDef ? DelDef->BaseSpeed : 130.0f;
    float SpeedCMS = SpeedKMH * 100000.0f / 3600.0f; // km/h to cm/s

    // Apply effort
    SpeedCMS *= FMath::Lerp(0.8f, 1.0f, Effort);

    // Direction towards target
    FVector Direction = FVector(1.0f, AimOffset.X / 100.0f, AimOffset.Y / 100.0f).GetSafeNormal();

    // Add vertical component from release angle
    Direction.Z = FMath::Sin(FMath::DegreesToRadians(ReleaseAngle));
    Direction = Direction.GetSafeNormal();

    return Direction * SpeedCMS;
}

void ACricketBowlingPawn::SetPhase(EBowlingPhase NewPhase)
{
    if (CurrentPhase != NewPhase)
    {
        CurrentPhase = NewPhase;
        OnBowlingPhaseChanged.Broadcast(NewPhase);
        UE_LOG(LogTemp, Log, TEXT("Bowling phase changed to: %s"), *UEnum::GetValueAsString(NewPhase));
    }
}

float ACricketBowlingPawn::CalculateRhythmScore() const
{
    return RhythmTimingScore;
}

const FDeliveryDefinition* ACricketBowlingPawn::GetDeliveryDefinition() const
{
    for (const FDeliveryDefinition& Def : DeliveryDefinitions)
    {
        if (Def.DeliveryType == SelectedDeliveryType)
        {
            return &Def;
        }
    }
    return nullptr;
}

// ============================================================================
// GAS Implementation
// ============================================================================

void ACricketBowlingPawn::InitializeAttributeSet()
{
    if (!AbilitySystemComponent || !AttributeSet)
    {
        return;
    }

    // Set initial bowling attribute values
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingPaceAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingSwingAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingSeamAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingSpinAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingControlAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingStaminaAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetBowlingVariationsAttribute(), 100.0f);

    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPressureLevelAttribute(), PressureLevel);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFormLevelAttribute(), FormLevel);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
}

void ACricketBowlingPawn::GrantDefaultAbilities()
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    // Grant default gameplay abilities
    for (TSubclassOf<UCricketGameplayAbility> AbilityClass : DefaultAbilities)
    {
        if (AbilityClass)
        {
            FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(Spec);
        }
    }
}

void ACricketBowlingPawn::InitializeAbilities()
{
    // Grant delivery abilities
    if (AbilitySystemComponent)
    {
        for (TSubclassOf<UCricketBowlingAbility> DeliveryAbilityClass : AvailableDeliveryAbilities)
        {
            if (DeliveryAbilityClass)
            {
                FGameplayAbilitySpec Spec(DeliveryAbilityClass, 1, INDEX_NONE, this);
                AbilitySystemComponent->GiveAbility(Spec);
            }
        }

        // Grant run-up rhythm ability
        if (RunupRhythmAbilityClass)
        {
            FGameplayAbilitySpec Spec(RunupRhythmAbilityClass, 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(Spec);
        }
    }
}

void ACricketBowlingPawn::ActivateDeliveryAbility(EDeliveryType DeliveryType)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    UCricketBowlingAbility* Ability = FindDeliveryAbility(DeliveryType);
    if (Ability)
    {
        // End any currently active delivery ability
        if (ActiveDeliveryAbility && ActiveDeliveryAbility != Ability)
        {
            EndActiveDeliveryAbility();
        }

        // Activate the new delivery ability
        FGameplayTagContainer ActivationTags;
        ActivationTags.AddTag(Ability->DeliveryGameplayTag);

        bool bActivated = AbilitySystemComponent->TryActivateAbilityByTag(ActivationTags);
        if (bActivated)
        {
            ActiveDeliveryAbility = Ability;
            OnDeliveryAbilityActivated(Ability);

            // Apply bowling input from current pawn state
            if (Ability)
            {
                Ability->SetBowlingInput(
                    RhythmTimingScore, // RunupRhythm
                    AimOffset.X,       // AimX
                    AimOffset.Y,       // AimY
                    0.0f,              // SeamAngle - would come from input
                    0.0f,              // SpinAxis - would come from input
                    Effort             // Effort
                );
            }
        }
    }
}

void ACricketBowlingPawn::ActivateRunupRhythmAbility()
{
    if (!AbilitySystemComponent || !RunupRhythmAbilityClass)
    {
        return;
    }

    // Activate run-up rhythm ability
    FGameplayTagContainer ActivationTags;
    ActivationTags.AddTag(FGameplayTag::RequestGameplayTag("Cricket.Runup.Rhythm"));

    bool bActivated = AbilitySystemComponent->TryActivateAbilityByTag(ActivationTags);
    if (bActivated)
    {
        ActiveRunupAbility = Cast<UCricketRunupRhythmAbility>(AbilitySystemComponent->FindAbilitySpecFromClass(RunupRhythmAbilityClass).Ability);
    }
}

void ACricketBowlingPawn::EndActiveDeliveryAbility()
{
    if (AbilitySystemComponent && ActiveDeliveryAbility)
    {
        AbilitySystemComponent->CancelAbilityHandle(ActiveDeliveryAbility->GetCurrentAbilitySpecHandle());
        OnDeliveryAbilityEnded(ActiveDeliveryAbility);
        ActiveDeliveryAbility = nullptr;
    }
}

void ACricketBowlingPawn::ApplyStaminaCost(float Cost)
{
    if (AbilitySystemComponent)
    {
        CurrentStamina = FMath::Max(0.0f, CurrentStamina - Cost);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    }
}

void ACricketBowlingPawn::RegenerateStamina(float DeltaTime)
{
    if (AbilitySystemComponent && CurrentStamina < MaxStamina)
    {
        float RegenRate = 3.0f; // Stamina per second (slower than batting)
        // Modified by fatigue
        RegenRate *= (1.0f - FatigueLevel * 0.5f);

        CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + RegenRate * DeltaTime);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    }
}

void ACricketBowlingPawn::SetPressureLevel(float Level)
{
    PressureLevel = FMath::Clamp(Level, 0.0f, 1.0f);
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPressureLevelAttribute(), PressureLevel);
    }
}

void ACricketBowlingPawn::UpdateForm(float PerformanceDelta)
{
    // PerformanceDelta: positive for good deliveries, negative for poor deliveries
    FormLevel = FMath::Clamp(FormLevel + PerformanceDelta * 0.1f, -1.0f, 1.0f);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFormLevelAttribute(), FormLevel);
    }
}

void ACricketBowlingPawn::AddFatigue(float Amount)
{
    FatigueLevel = FMath::Clamp(FatigueLevel + Amount, 0.0f, 1.0f);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
    }
}

void ACricketBowlingPawn::RecoverFatigue(float DeltaTime)
{
    if (FatigueLevel > 0.0f)
    {
        float RecoveryRate = 0.03f; // Per second (slower recovery than batting)
        FatigueLevel = FMath::Max(0.0f, FatigueLevel - RecoveryRate * DeltaTime);

        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
        }
    }
}

UCricketBowlingAbility* ACricketBowlingPawn::FindDeliveryAbility(EDeliveryType DeliveryType) const
{
    if (!AbilitySystemComponent)
    {
        return nullptr;
    }

    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        UCricketBowlingAbility* DeliveryAbility = Cast<UCricketBowlingAbility>(Spec.Ability);
        if (DeliveryAbility && DeliveryAbility->DeliveryType == DeliveryType)
        {
            return DeliveryAbility;
        }
    }

    return nullptr;
}

void ACricketBowlingPawn::OnDeliveryAbilityActivated(UCricketBowlingAbility* Ability)
{
    if (Ability)
    {
        // Apply modifiers based on pressure, form, fatigue
        float ControlModifier = 1.0f;

        // Pressure reduces control
        ControlModifier *= (1.0f - PressureLevel * 0.4f);

        // Form affects control
        ControlModifier *= (1.0f + FormLevel * 0.15f);

        // Fatigue reduces control and accuracy
        ControlModifier *= (1.0f - FatigueLevel * 0.3f);

        // Apply to ability
        Ability->AccuracyRadius /= ControlModifier; // Smaller radius = better accuracy
        Ability->ControlDifficulty *= (2.0f - ControlModifier); // Higher difficulty = harder to control

        // Apply stamina cost
        ApplyStaminaCost(Ability->StaminaCost * Effort);
    }
}

void ACricketBowlingPawn::OnDeliveryAbilityEnded(UCricketBowlingAbility* Ability)
{
    if (Ability)
    {
        // Update form based on delivery outcome
        // This would be called with actual result data
    }
}

void ACricketBowlingPawn::UpdateGASAttributes(float DeltaTime)
{
    // Regenerate stamina
    RegenerateStamina(DeltaTime);

    // Recover fatigue
    RecoverFatigue(DeltaTime);

    // Update pressure based on match situation (would come from GameState)
    // Pressure naturally decays slightly over time
    if (PressureLevel > 0.0f)
    {
        PressureLevel = FMath::Max(0.0f, PressureLevel - 0.01f * DeltaTime);
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPressureLevelAttribute(), PressureLevel);
        }
    }
}