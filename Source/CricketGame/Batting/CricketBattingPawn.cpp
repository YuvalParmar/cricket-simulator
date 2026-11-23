#include "CricketBattingPawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "CricketBattingComponent.h"
#include "PhoneBatReceiverComponent.h"
#include "CricketBall.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "CricketGameplayAbilities.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"

ACricketBattingPawn::ACricketBattingPawn()
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

    // Bat mesh
    BatMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BatMesh"));
    BatMesh->SetupAttachment(BodyMesh, TEXT("RightHandSocket")); // Assumes socket exists
    BatMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // First person camera (at eye level)
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(BodyMesh, TEXT("HeadSocket"));
    FirstPersonCamera->SetRelativeLocation(FVector(10.0f, 0.0f, 0.0f));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetActive(false);

    // Third person camera
    ThirdPersonSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonSpringArm"));
    ThirdPersonSpringArm->SetupAttachment(RootComponent);
    ThirdPersonSpringArm->TargetArmLength = 400.0f;
    ThirdPersonSpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
    ThirdPersonSpringArm->bUsePawnControlRotation = true;
    ThirdPersonSpringArm->bInheritPitch = true;
    ThirdPersonSpringArm->bInheritYaw = true;
    ThirdPersonSpringArm->bInheritRoll = false;
    ThirdPersonSpringArm->bDoCollisionTest = true;
    ThirdPersonSpringArm->CameraLagSpeed = 5.0f;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(ThirdPersonSpringArm, USpringArmComponent::SocketName);
    ThirdPersonCamera->bUsePawnControlRotation = false;
    ThirdPersonCamera->SetActive(true);

    // GAS Components
    AbilitySystemComponent = CreateDefaultSubobject<UCricketAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UCricketAttributeSet>(TEXT("AttributeSet"));
    AbilitySystemComponent->AddAttributeSet(AttributeSet);

    // Components
    BattingComponent = CreateDefaultSubobject<UCricketBattingComponent>(TEXT("BattingComponent"));
    PhoneBatReceiver = CreateDefaultSubobject<UPhoneBatReceiverComponent>(TEXT("PhoneBatReceiver"));

    // Defaults
    bFirstPersonCamera = false;
    bUsePhoneBat = false;
    bPhoneBatConnected = false;
    PhoneBatSwingPhase = 0;
    BatFaceAngle = 0.0f;
    WristRotation = 0.0f;
    bIsSwinging = false;
    SwingProgress = 0.0f;
    bPowerModifierActive = false;
    bAggressiveMode = false;
    CurrentStamina = 100.0f;
    MaxStamina = 100.0f;
    PressureLevel = 0.0f;
    FormLevel = 0.0f;
    FatigueLevel = 0.0f;
}

void ACricketBattingPawn::BeginPlay()
{
    Super::BeginPlay();

    // Initialize GAS
    InitializeAttributeSet();
    GrantDefaultAbilities();
    InitializeAbilities();

    // Load shot definitions from DataTable if provided
    if (ShotDefinitionsTable)
    {
        TArray<FShotDefinition*> Rows;
        ShotDefinitionsTable->GetAllRows(TEXT(""), Rows);
        for (FShotDefinition* Row : Rows)
        {
            if (Row)
            {
                ShotDefinitions.Add(*Row);
            }
        }
    }

    // Set initial camera
    SetCameraPerspective(bFirstPersonCamera);

    // Start listening for ball arrival (would be set by GameMode)
    // CurrentBallArrivalTime = GetWorld()->GetTimeSeconds() + 2.0f; // Demo
}

UAbilitySystemComponent* ACricketBattingPawn::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACricketBattingPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateBatTransform(DeltaSeconds);
    UpdatePhoneBatIntegration(DeltaSeconds);
    DetectTimingWindow();
    UpdateCameraBlending(DeltaSeconds);
    UpdateGASAttributes(DeltaSeconds);
}

void ACricketBattingPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // Enhanced input bindings are handled by PlayerController
}

void ACricketBattingPawn::SelectShotType(int32 ShotTypeIndex)
{
    if (ShotTypeIndex >= 0 && ShotTypeIndex < ShotDefinitions.Num())
    {
        SelectedShotType = ShotDefinitions[ShotTypeIndex].ShotType;
        UE_LOG(LogTemp, Log, TEXT("Selected shot type: %s"), *UEnum::GetValueAsString(SelectedShotType));
    }
}

void ACricketBattingPawn::SetFootworkInput(const FVector2D& Direction, float Magnitude)
{
    // Convert direction to footwork type
    float Angle = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));

    if (Magnitude < 0.2f)
    {
        CurrentFootwork = EFootworkType::None;
    }
    else if (Angle >= -45.0f && Angle <= 45.0f)
    {
        CurrentFootwork = EFootworkType::ForwardPress;
    }
    else if (Angle >= 135.0f || Angle <= -135.0f)
    {
        CurrentFootwork = EFootworkType::BackFoot;
    }
    else if (Angle > 45.0f && Angle < 135.0f)
    {
        CurrentFootwork = EFootworkType::ShuffleAcross;
    }
    else
    {
        CurrentFootwork = EFootworkType::OpenStance;
    }
}

void ACricketBattingPawn::SetBatAngleInput(float FaceAngle, float WristRotationInput)
{
    BatFaceAngle = FMath::Clamp(FaceAngle, -90.0f, 90.0f);
    WristRotation = FMath::Clamp(WristRotationInput, -45.0f, 45.0f);
}

void ACricketBattingPawn::OnSwingCommit()
{
    if (!bIsSwinging)
    {
        bIsSwinging = true;
        SwingStartTime = GetWorld()->GetTimeSeconds();
        SwingCommitTime = SwingStartTime;
        SwingProgress = 0.0f;

        UE_LOG(LogTemp, Log, TEXT("Swing committed at time: %f"), SwingStartTime);
    }
}

void ACricketBattingPawn::OnSwingRelease()
{
    if (bIsSwinging)
    {
        // Swing release triggers the actual shot execution
        // The bat should be at or near contact point
        FShotResult Result;
        CalculateShotResult(Result);
        ProcessShot(Result);
    }
}

void ACricketBattingPawn::SetPowerModifier(bool bAggressive)
{
    bAggressiveMode = bAggressive;
    bPowerModifierActive = true;
}

void ACricketBattingPawn::UpdatePhoneBatPose(const FTransform& BatTransform, const FVector& LinearVelocity, const FVector& AngularVelocity)
{
    if (!bUsePhoneBat || !bPhoneBatConnected)
    {
        return;
    }

    // Apply calibration transform
    CurrentPhoneBatTransform = PhoneBatCalibrationTransform * BatTransform;
    PhoneBatLinearVelocity = LinearVelocity;
    PhoneBatAngularVelocity = AngularVelocity;

    // Update visual bat if in first person
    if (bFirstPersonCamera && BatMesh)
    {
        // Smooth interpolation for visual representation
        FTransform TargetTransform = CurrentPhoneBatTransform;
        BatMesh->SetWorldTransform(FMath::Lerp(BatMesh->GetComponentTransform(), TargetTransform, 0.5f));
    }
}

void ACricketBattingPawn::OnPhoneBatSwingPhaseChanged(int32 Phase)
{
    PhoneBatSwingPhase = Phase;

    switch (Phase)
    {
        case 1: // Backswing
            if (!bIsSwinging)
            {
                OnSwingCommit();
            }
            break;
        case 2: // Downswing
            // Swing is in progress
            break;
        case 3: // Follow through
            if (bIsSwinging)
            {
                OnSwingRelease();
            }
            break;
        case 0: // Idle
        default:
            bIsSwinging = false;
            SwingProgress = 0.0f;
            break;
    }
}

void ACricketBattingPawn::OnPhoneBatContactPredicted(float TimeToContact, float Confidence)
{
    PhoneBatContactPredictionTime = TimeToContact;
    PhoneBatContactConfidence = Confidence;

    // Could use this to adjust timing window or provide visual feedback
}

void ACricketBattingPawn::SetPhoneBatCalibration(const FTransform& PhoneToBatTransform)
{
    PhoneBatCalibrationTransform = PhoneToBatTransform;
    bPhoneBatConnected = true;
    UE_LOG(LogTemp, Log, TEXT("Phone bat calibrated"));
}

void ACricketBattingPawn::SetCameraPerspective(bool bFirstPerson)
{
    bFirstPersonCamera = bFirstPerson;

    if (FirstPersonCamera && ThirdPersonCamera)
    {
        FirstPersonCamera->SetActive(bFirstPerson);
        ThirdPersonCamera->SetActive(!bFirstPerson);
    }

    // Enable phone bat in first person for AR-like experience
    bUsePhoneBat = bFirstPerson && bPhoneBatConnected;
}

void ACricketBattingPawn::UpdateBatTransform(float DeltaSeconds)
{
    if (!BatMesh)
    {
        return;
    }

    FTransform CurrentBatTransform = BatMesh->GetComponentTransform();
    FTransform TargetTransform = CurrentBatTransform;

    if (bUsePhoneBat && bPhoneBatConnected)
    {
        // Phone bat drives the transform directly
        TargetTransform = CurrentPhoneBatTransform;
    }
    else
    {
        // Procedural bat animation based on input
        if (bIsSwinging)
        {
            SwingProgress += DeltaSeconds * 3.0f; // Swing speed
            SwingProgress = FMath::Clamp(SwingProgress, 0.0f, 1.0f);

            // Calculate bat rotation based on swing progress
            float SwingAngle = SwingProgress * 180.0f; // Full 180 degree swing

            // Apply shot-specific modifications
            const FShotDefinition* ShotDef = nullptr;
            for (const FShotDefinition& Def : ShotDefinitions)
            {
                if (Def.ShotType == SelectedShotType)
                {
                    ShotDef = &Def;
                    break;
                }
            }

            if (ShotDef)
            {
                // Modify swing based on shot type
                // This is simplified - real implementation would use animation montages
            }

            // Update bat transform
            FQuat BatRotation = FQuat(FRotator(0.0f, BatFaceAngle + SwingAngle, WristRotation));
            TargetTransform.SetRotation(BatRotation);
        }
        else
        {
            // Idle stance
            FQuat IdleRotation = FQuat(FRotator(0.0f, BatFaceAngle, WristRotation));
            TargetTransform.SetRotation(IdleRotation);
            SwingProgress = 0.0f;
        }
    }

    // Smooth interpolation
    BatMesh->SetWorldTransform(FMath::Lerp(CurrentBatTransform, TargetTransform, DeltaSeconds * 15.0f));

    // Broadcast swing update
    OnBatSwingUpdate.Broadcast(SwingProgress, BatMesh->GetComponentTransform());
}

void ACricketBattingPawn::UpdatePhoneBatIntegration(float DeltaSeconds)
{
    if (!bUsePhoneBat || !bPhoneBatConnected)
    {
        return;
    }

    // Phone bat provides real-time pose
    // The UpdatePhoneBatPose function is called from network messages
    // Here we just ensure the visual representation is updated
}

void ACricketBattingPawn::DetectTimingWindow()
{
    if (!bIsSwinging)
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float TimeSinceSwingStart = CurrentTime - SwingStartTime;

    // In a real implementation, CurrentBallArrivalTime would be predicted by the physics system
    // For now, we'll simulate it
    if (CurrentBallArrivalTime == 0.0f)
    {
        CurrentBallArrivalTime = SwingStartTime + 0.5f; // Ball arrives 0.5s after swing start (demo)
    }

    float TimeToContact = CurrentBallArrivalTime - CurrentTime;

    // Determine timing window
    ETimingWindow Window = ETimingWindow::Missed;

    if (TimeToContact > 0.15f)
    {
        Window = ETimingWindow::VeryEarly;
    }
    else if (TimeToContact > 0.05f)
    {
        Window = ETimingWindow::Early;
    }
    else if (TimeToContact > -0.02f)
    {
        Window = ETimingWindow::Perfect;
    }
    else if (TimeToContact > -0.08f)
    {
        Window = ETimingWindow::Late;
    }
    else if (TimeToContact > -0.15f)
    {
        Window = ETimingWindow::VeryLate;
    }

    OnTimingWindowUpdate.Broadcast(Window);

    // Auto-trigger shot if swing reaches follow-through without manual release
    if (SwingProgress >= 0.95f && bIsSwinging)
    {
        OnSwingRelease();
    }
}

void ACricketBattingPawn::ProcessShot(const FShotResult& Result)
{
    LastShotResult = Result;
    bIsSwinging = false;
    SwingProgress = 0.0f;
    CurrentBallArrivalTime = 0.0f; // Reset for next ball

    // Broadcast event
    OnShotPlayed.Broadcast(Result);

    // Notify game mode
    if (ACricketGameMode* GameMode = Cast<ACricketGameMode>(GetWorld()->GetAuthGameMode()))
    {
        // GameMode->OnShotPlayed(Result);
    }

    UE_LOG(LogTemp, Log, TEXT("Shot played: %s, Timing: %s, Power: %.1f, Control: %.1f"),
        *UEnum::GetValueAsString(Result.ShotType),
        *UEnum::GetValueAsString(Result.Timing),
        Result.ShotPower,
        Result.ShotControl);
}

void ACricketBattingPawn::PlayShotAnimation(EShotType ShotType, float PlayRate)
{
    if (!BodyMesh || !BodyMesh->GetAnimInstance())
    {
        return;
    }

    // Find montage for shot type
    for (const FShotDefinition& Def : ShotDefinitions)
    {
        if (Def.ShotType == ShotType && !Def.AnimationMontageName.IsNone())
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

void ACricketBattingPawn::UpdateCameraBlending(float DeltaSeconds)
{
    // Smooth camera transitions if needed
}

void ACricketBattingPawn::CalculateShotResult(FShotResult& OutResult)
{
    OutResult.ShotType = SelectedShotType;

    // Find shot definition
    const FShotDefinition* ShotDef = nullptr;
    for (const FShotDefinition& Def : ShotDefinitions)
    {
        if (Def.ShotType == SelectedShotType)
        {
            ShotDef = &Def;
            break;
        }
    }

    // Determine timing window
    float CurrentTime = GetWorld()->GetTimeSeconds();
    OutResult.Timing = DetermineTimingWindow(CurrentTime, CurrentBallArrivalTime);

    // Calculate bat speed
    float BatSpeed = CalculateBatSpeed();
    OutResult.BatSpeedAtContact = BatSpeed;

    // Calculate contact angle
    OutResult.ContactAngle = BatFaceAngle; // Simplified

    // Contact point (sweet spot)
    OutResult.ContactPoint = CalculateBatSweetSpot();

    // Ball velocity at contact (would come from ball actor)
    // OutResult.BallVelocityAtContact = CurrentBallActor->GetVelocity();

    // Bat velocity at contact
    OutResult.BatVelocityAtContact = PhoneBatLinearVelocity.Size() > 0.0f ?
        PhoneBatLinearVelocity : FVector(BatSpeed, 0.0f, 0.0f);

    // Calculate scores
    if (ShotDef)
    {
        // Timing score
        switch (OutResult.Timing)
        {
            case ETimingWindow::Perfect: OutResult.TimingScore = 1.0f; break;
            case ETimingWindow::Early: OutResult.TimingScore = 0.7f; break;
            case ETimingWindow::Late: OutResult.TimingScore = 0.6f; break;
            case ETimingWindow::VeryEarly: OutResult.TimingScore = 0.3f; break;
            case ETimingWindow::VeryLate: OutResult.TimingScore = 0.2f; break;
            default: OutResult.TimingScore = 0.0f; break;
        }

        // Placement score (based on footwork, bat angle, delivery suitability)
        OutResult.PlacementScore = 0.5f; // Base
        if (CurrentFootwork == ShotDef->RequiredFootwork)
        {
            OutResult.PlacementScore += 0.2f;
        }
        if (FMath::Abs(BatFaceAngle - ShotDef->IdealContactAngle) < ShotDef->AngleTolerance)
        {
            OutResult.PlacementScore += 0.3f;
        }

        // Power score
        float SpeedRatio = FMath::GetMappedRangeValueClamped(
            FVector2D(ShotDef->MinBatSpeed, ShotDef->MaxBatSpeed),
            FVector2D(0.0f, 1.0f),
            BatSpeed
        );
        OutResult.PowerScore = SpeedRatio * ShotDef->PowerMultiplier;
        if (bAggressiveMode)
        {
            OutResult.PowerScore *= 1.2f;
        }

        // Combined shot power and control
        OutResult.ShotPower = OutResult.TimingScore * OutResult.PowerScore * ShotDef->PowerMultiplier;
        OutResult.ShotControl = OutResult.TimingScore * OutResult.PlacementScore * ShotDef->ControlMultiplier;

        // Predict runs (simplified)
        OutResult.PredictedRuns = FMath::RoundToInt(OutResult.ShotPower * 6.0f);

        // Wicket chance
        if (OutResult.TimingScore < 0.3f || OutResult.PlacementScore < 0.3f)
        {
            OutResult.bIsWicket = FMath::RandBool(); // Simplified
            if (OutResult.bIsWicket)
            {
                OutResult.WicketType = FGameplayTag::RequestGameplayTag("Wicket.Caught");
            }
        }
    }
}

ETimingWindow ACricketBattingPawn::DetermineTimingWindow(float SwingTime, float BallArrivalTime) const
{
    float TimeDiff = SwingTime - BallArrivalTime;

    if (TimeDiff > 0.15f) return ETimingWindow::VeryEarly;
    if (TimeDiff > 0.05f) return ETimingWindow::Early;
    if (TimeDiff > -0.02f) return ETimingWindow::Perfect;
    if (TimeDiff > -0.08f) return ETimingWindow::Late;
    if (TimeDiff > -0.15f) return ETimingWindow::VeryLate;

    return ETimingWindow::Missed;
}

FVector ACricketBattingPawn::CalculateBatSweetSpot() const
{
    if (!BatMesh)
    {
        return GetActorLocation() + FVector(100.0f, 0.0f, 0.0f);
    }

    // Sweet spot is typically ~15-20cm from the toe of the bat
    // This would be a socket on the bat mesh in practice
    return BatMesh->GetSocketLocation(TEXT("SweetSpot"));
}

float ACricketBattingPawn::CalculateBatSpeed() const
{
    if (bUsePhoneBat && bPhoneBatConnected)
    {
        return PhoneBatLinearVelocity.Size();
    }

    // Estimate from swing progress
    return FMath::Lerp(500.0f, 3000.0f, SwingProgress);
}

// ============================================================================
// GAS Implementation
// ============================================================================

void ACricketBattingPawn::InitializeAttributeSet()
{
    if (!AbilitySystemComponent || !AttributeSet)
    {
        return;
    }

    // Set initial attribute values from player data
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetTechniqueAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPowerAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetTimingAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFootworkAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetTemperamentAttribute(), 100.0f);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetConcentrationAttribute(), 100.0f);

    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetMaxStaminaAttribute(), MaxStamina);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPressureLevelAttribute(), PressureLevel);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFormLevelAttribute(), FormLevel);
    AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
}

void ACricketBattingPawn::GrantDefaultAbilities()
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

void ACricketBattingPawn::InitializeAbilities()
{
    // Grant shot abilities
    if (AbilitySystemComponent)
    {
        for (TSubclassOf<UCricketShotAbility> ShotAbilityClass : AvailableShotAbilities)
        {
            if (ShotAbilityClass)
            {
                FGameplayAbilitySpec Spec(ShotAbilityClass, 1, INDEX_NONE, this);
                AbilitySystemComponent->GiveAbility(Spec);
            }
        }

        // Grant footwork ability
        if (FootworkAbilityClass)
        {
            FGameplayAbilitySpec Spec(FootworkAbilityClass, 1, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(Spec);
        }
    }
}

void ACricketBattingPawn::ActivateShotAbility(EShotType ShotType)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    UCricketShotAbility* Ability = FindShotAbility(ShotType);
    if (Ability)
    {
        // End any currently active shot ability
        if (ActiveShotAbility && ActiveShotAbility != Ability)
        {
            EndActiveShotAbility();
        }

        // Activate the new shot ability
        FGameplayTagContainer ActivationTags;
        ActivationTags.AddTag(Ability->ShotGameplayTag);

        bool bActivated = AbilitySystemComponent->TryActivateAbilityByTag(ActivationTags);
        if (bActivated)
        {
            ActiveShotAbility = Ability;
            OnShotAbilityActivated(Ability);

            // Apply shot input from current pawn state
            if (Ability)
            {
                Ability->SetShotInput(
                    0.0f, // FootworkX - would come from input
                    0.0f, // FootworkY
                    BatFaceAngle,
                    WristRotation,
                    0.0f  // TimingInput - set when swing committed
                );
            }
        }
    }
}

void ACricketBattingPawn::ActivateFootworkAbility(EFootworkType FootworkType)
{
    if (!AbilitySystemComponent || !FootworkAbilityClass)
    {
        return;
    }

    // End any active footwork ability
    if (ActiveFootworkAbility)
    {
        AbilitySystemComponent->CancelAbilityHandle(ActiveFootworkAbility->GetCurrentAbilitySpecHandle());
    }

    // Activate footwork ability
    FGameplayTagContainer ActivationTags;
    ActivationTags.AddTag(FGameplayTag::RequestGameplayTag("Cricket.Footwork"));

    bool bActivated = AbilitySystemComponent->TryActivateAbilityByTag(ActivationTags);
    if (bActivated)
    {
        ActiveFootworkAbility = Cast<UCricketFootworkAbility>(AbilitySystemComponent->FindAbilitySpecFromClass(FootworkAbilityClass).Ability);
        if (ActiveFootworkAbility)
        {
            ActiveFootworkAbility->SetFootworkDirection(FVector2D::ZeroVector); // Would come from input
        }
    }
}

void ACricketBattingPawn::EndActiveShotAbility()
{
    if (AbilitySystemComponent && ActiveShotAbility)
    {
        AbilitySystemComponent->CancelAbilityHandle(ActiveShotAbility->GetCurrentAbilitySpecHandle());
        OnShotAbilityEnded(ActiveShotAbility);
        ActiveShotAbility = nullptr;
    }
}

void ACricketBattingPawn::ApplyStaminaCost(float Cost)
{
    if (AbilitySystemComponent)
    {
        CurrentStamina = FMath::Max(0.0f, CurrentStamina - Cost);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    }
}

void ACricketBattingPawn::RegenerateStamina(float DeltaTime)
{
    if (AbilitySystemComponent && CurrentStamina < MaxStamina)
    {
        float RegenRate = 5.0f; // Stamina per second
        // Modified by fatigue
        RegenRate *= (1.0f - FatigueLevel * 0.5f);

        CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + RegenRate * DeltaTime);
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetCurrentStaminaAttribute(), CurrentStamina);
    }
}

void ACricketBattingPawn::SetPressureLevel(float Level)
{
    PressureLevel = FMath::Clamp(Level, 0.0f, 1.0f);
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetPressureLevelAttribute(), PressureLevel);
    }
}

void ACricketBattingPawn::UpdateForm(float PerformanceDelta)
{
    // PerformanceDelta: positive for good shots, negative for poor shots
    FormLevel = FMath::Clamp(FormLevel + PerformanceDelta * 0.1f, -1.0f, 1.0f);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFormLevelAttribute(), FormLevel);
    }
}

void ACricketBattingPawn::AddFatigue(float Amount)
{
    FatigueLevel = FMath::Clamp(FatigueLevel + Amount, 0.0f, 1.0f);

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
    }
}

void ACricketBattingPawn::RecoverFatigue(float DeltaTime)
{
    if (FatigueLevel > 0.0f)
    {
        float RecoveryRate = 0.05f; // Per second
        FatigueLevel = FMath::Max(0.0f, FatigueLevel - RecoveryRate * DeltaTime);

        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->SetNumericAttributeBase(AttributeSet->GetFatigueLevelAttribute(), FatigueLevel);
        }
    }
}

UCricketShotAbility* ACricketBattingPawn::FindShotAbility(EShotType ShotType) const
{
    if (!AbilitySystemComponent)
    {
        return nullptr;
    }

    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        UCricketShotAbility* ShotAbility = Cast<UCricketShotAbility>(Spec.Ability);
        if (ShotAbility && ShotAbility->ShotType == ShotType)
        {
            return ShotAbility;
        }
    }

    return nullptr;
}

void ACricketBattingPawn::OnShotAbilityActivated(UCricketShotAbility* Ability)
{
    if (Ability)
    {
        // Apply timing window modifiers based on pressure and form
        float TimingModifier = 1.0f;

        // Pressure reduces timing window
        TimingModifier *= (1.0f - PressureLevel * 0.3f);

        // Form affects timing window
        TimingModifier *= (1.0f + FormLevel * 0.2f);

        // Fatigue reduces timing window
        TimingModifier *= (1.0f - FatigueLevel * 0.2f);

        // Apply to ability
        Ability->PerfectWindowMs *= TimingModifier;
        Ability->GoodWindowMs *= TimingModifier;
        Ability->EarlyWindowMs *= TimingModifier;
        Ability->LateWindowMs *= TimingModifier;

        // Apply stamina cost
        ApplyStaminaCost(Ability->GetStaminaCost());
    }
}

void ACricketBattingPawn::OnShotAbilityEnded(UCricketShotAbility* Ability)
{
    if (Ability)
    {
        // Update form based on shot outcome
        // This would be called with actual result data
    }
}

void ACricketBattingPawn::UpdateGASAttributes(float DeltaTime)
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