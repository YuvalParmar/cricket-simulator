#include "CricketGameplayAbilities.h"
#include "CricketBattingPawn.h"
#include "CricketBowlingPawn.h"
#include "CricketFieldingPawn.h"
#include "CricketBall.h"
#include "PitchActor.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagAssetInterface.h"

// ============================================================================
// BASE CRICKET ABILITY
// ============================================================================

UCricketGameplayAbility::UCricketGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    AbilityCooldown = 0.0f;
}

bool UCricketGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // Check required state tags
    if (RequiredStateTags.Num() > 0)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC && !ASC->HasAllMatchingGameplayTags(RequiredStateTags))
        {
            return false;
        }
    }

    // Check blocked tags
    if (BlockedByTags.Num() > 0)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC && ASC->HasAnyMatchingGameplayTags(BlockedByTags))
        {
            return false;
        }
    }

    return true;
}

void UCricketGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // Apply owned tags
    if (ActivationOwnedTags.Num() > 0)
    {
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTags(ActivationOwnedTags);
    }

    // Apply cooldown if set
    if (AbilityCooldown > 0.0f)
    {
        FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(FGameplayEffect::StaticClass(), 1.0f, Handle);
        if (SpecHandle.IsValid())
        {
            SpecHandle.Data->DurationPolicy = EGameplayEffectDurationType::HasDuration;
            SpecHandle.Data->Duration = AbilityCooldown;
            ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void UCricketGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    // Remove owned tags
    if (ActivationOwnedTags.Num() > 0)
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTags(ActivationOwnedTags);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

ACricketBattingPawn* UCricketGameplayAbility::GetBattingPawn(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return Cast<ACricketBattingPawn>(ActorInfo->AvatarActor.Get());
}

ACricketBowlingPawn* UCricketGameplayAbility::GetBowlingPawn(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return Cast<ACricketBowlingPawn>(ActorInfo->AvatarActor.Get());
}

ACricketFieldingPawn* UCricketGameplayAbility::GetFieldingPawn(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return Cast<ACricketFieldingPawn>(ActorInfo->AvatarActor.Get());
}

// ============================================================================
// BATTING ABILITIES
// ============================================================================

UCricketShotAbility::UCricketShotAbility()
{
    ShotType = EShotType::ForwardDefense;
    PerfectWindowMs = 20.0f;
    GoodWindowMs = 50.0f;
    EarlyWindowMs = 100.0f;
    LateWindowMs = 100.0f;
    PowerMultiplier = 1.0f;
    ControlMultiplier = 1.0f;
    PlacementSpreadDegrees = 15.0f;
    EdgeProbability = 0.05f;
    MissProbability = 0.1f;
    MontagePlayRate = 1.0f;
}

void UCricketShotAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ACricketBattingPawn* BattingPawn = GetBattingPawn(ActorInfo);
    if (BattingPawn)
    {
        // Initialize shot input state
        CurrentShotInput = FShotInputState();
        CurrentShotInput.InputTime = ActorInfo->GetWorld()->GetTimeSeconds();

        // Play shot montage if available
        if (ShotMontage)
        {
            BattingPawn->PlayAnimMontage(ShotMontage, MontagePlayRate, StartSectionName);
        }

        // Notify pawn of shot start
        BattingPawn->OnShotAbilityActivated(this);
    }
}

void UCricketShotAbility::OnShotTriggered(const FGameplayEventData* EventData)
{
    // Called when player triggers shot (presses timing button)
    CurrentShotInput.bTimingTriggered = true;
}

void UCricketShotAbility::OnContactPredicted(float TimeToContact, const FVector& PredictedContactPoint)
{
    // Called when physics predicts contact
    // Can be used for visual feedback (timing meter)
}

void UCricketShotAbility::OnContactMade(const FVector& ContactPoint, const FVector& BallVelocity, float TimingQuality)
{
    // Called when bat actually hits ball
    // Calculate outcome based on timing quality and shot type
}

void UCricketShotAbility::OnShotComplete(EShotOutcome Outcome, const FVector& BallDirection, float Power, float Placement)
{
    // Called when shot completes (ball leaves bat or misses)
    ACricketBattingPawn* BattingPawn = GetBattingPawn(GetActorInfo());
    if (BattingPawn)
    {
        BattingPawn->OnShotAbilityComplete(this, Outcome, BallDirection, Power, Placement);
    }
}

void UCricketShotAbility::SetShotInput(float FootworkX, float FootworkY, float BatAngle, float WristRotation, float TimingInput)
{
    CurrentShotInput.FootworkX = FootworkX;
    CurrentShotInput.FootworkY = FootworkY;
    CurrentShotInput.BatAngle = BatAngle;
    CurrentShotInput.WristRotation = WristRotation;
    CurrentShotInput.TimingInput = TimingInput;
}

UCricketDefensiveShotAbility::UCricketDefensiveShotAbility()
{
    ShotType = EShotType::ForwardDefense;
    PowerMultiplier = 0.3f;
    ControlMultiplier = 1.5f;
    PlacementSpreadDegrees = 10.0f;
    EdgeProbability = 0.02f;
    MissProbability = 0.05f;
    bRequiresFrontFoot = true;
}

UCricketDriveAbility::UCricketDriveAbility()
{
    ShotType = EShotType::CoverDrive;
    PowerMultiplier = 1.2f;
    ControlMultiplier = 1.0f;
    PlacementSpreadDegrees = 20.0f;
    EdgeProbability = 0.08f;
    MissProbability = 0.15f;
    bRequiresFrontFoot = true;
}

UCricketHorizontalBatAbility::UCricketHorizontalBatAbility()
{
    ShotType = EShotType::PullShot;
    PowerMultiplier = 1.5f;
    ControlMultiplier = 0.7f;
    PlacementSpreadDegrees = 25.0f;
    EdgeProbability = 0.12f;
    MissProbability = 0.2f;
    bRequiresBackFoot = true;
}

UCricketSweepAbility::UCricketSweepAbility()
{
    ShotType = EShotType::SweepShot;
    PowerMultiplier = 1.1f;
    ControlMultiplier = 0.8f;
    PlacementSpreadDegrees = 30.0f;
    EdgeProbability = 0.1f;
    MissProbability = 0.18f;
    bRequiresFrontFoot = true;
}

UCricketLoftedShotAbility::UCricketLoftedShotAbility()
{
    ShotType = EShotType::LoftedDrive;
    PowerMultiplier = 1.8f;
    ControlMultiplier = 0.5f;
    PlacementSpreadDegrees = 35.0f;
    EdgeProbability = 0.15f;
    MissProbability = 0.25f;
    bRequiresFrontFoot = true;
}

UCricketSpecialShotAbility::UCricketSpecialShotAbility()
{
    ShotType = EShotType::RampShot;
    PowerMultiplier = 1.3f;
    ControlMultiplier = 0.6f;
    PlacementSpreadDegrees = 40.0f;
    EdgeProbability = 0.1f;
    MissProbability = 0.2f;
}

UCricketFootworkAbility::UCricketFootworkAbility()
{
    FootworkType = EFootworkType::ForwardPress;
    MovementSpeed = 3.0f;
}

void UCricketFootworkAbility::SetFootworkDirection(const FVector2D& Direction)
{
    TargetOffset = Direction;
}

// ============================================================================
// BOWLING ABILITIES
// ============================================================================

UCricketBowlingAbility::UCricketBowlingAbility()
{
    DeliveryType = EDeliveryType::StockDelivery;
    RunupSpeed = 6.0f;
    RunupSteps = 15;
    RhythmTolerance = 0.1f;
    ReleaseHeight = 2.2f;
    ReleaseAngle = -5.0f;
    WristPosition = 0.0f;
    SeamOrientation = 0.0f;
    PaceMultiplier = 1.0f;
    SwingAmount = 0.0f;
    SeamMovement = 0.0f;
    SpinRPM = 0.0f;
    SpinAxis = FVector(0.0f, 0.0f, 1.0f);
    ControlDifficulty = 0.5f;
    StaminaCost = 1.0f;
    AccuracyRadius = 0.3f;
}

void UCricketBowlingAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ACricketBowlingPawn* BowlingPawn = GetBowlingPawn(ActorInfo);
    if (BowlingPawn)
    {
        CurrentBowlingInput = FBowlingInputState();
        BowlingPawn->OnBowlingAbilityActivated(this);
    }
}

void UCricketBowlingAbility::OnRunupStarted()
{
    CurrentBowlingInput.CurrentStep = 0;
}

void UCricketBowlingAbility::OnRunupStep(int32 StepNumber, float TimingQuality)
{
    CurrentBowlingInput.CurrentStep = StepNumber;
}

void UCricketBowlingAbility::OnRunupCompleted(float RhythmQuality)
{
    // Rhythm quality affects delivery accuracy
}

void UCricketBowlingAbility::OnReleasePoint(const FVector& ReleaseLocation, const FRotator& ReleaseRotation, float Effort)
{
    CurrentBowlingInput.bReleaseTriggered = true;
}

void UCricketBowlingAbility::OnBallReleased(ACricketBall* Ball)
{
    ACricketBowlingPawn* BowlingPawn = GetBowlingPawn(GetActorInfo());
    if (BowlingPawn)
    {
        BowlingPawn->OnBowlingAbilityComplete(this, Ball);
    }
}

void UCricketBowlingAbility::SetBowlingInput(float RunupRhythm, float AimX, float AimY, float SeamAngle, float SpinAxisInput, float Effort)
{
    CurrentBowlingInput.RunupRhythm = RunupRhythm;
    CurrentBowlingInput.AimX = AimX;
    CurrentBowlingInput.AimY = AimY;
    CurrentBowlingInput.SeamAngle = SeamAngle;
    CurrentBowlingInput.SpinAxis = SpinAxisInput;
    CurrentBowlingInput.Effort = Effort;
}

UCricketPaceDeliveryAbility::UCricketPaceDeliveryAbility()
{
    DeliveryType = EDeliveryType::StockDelivery;
    PaceMultiplier = 1.0f;
    SwingAmount = 0.0f;
    SeamMovement = 1.0f;
}

UCricketSwingDeliveryAbility::UCricketSwingDeliveryAbility()
{
    DeliveryType = EDeliveryType::OutSwinger;
    PaceMultiplier = 0.95f;
    SwingAmount = 3.0f;
    SeamMovement = 0.5f;
}

UCricketSpinDeliveryAbility::UCricketSpinDeliveryAbility()
{
    DeliveryType = EDeliveryType::OffBreak;
    PaceMultiplier = 0.5f;
    SpinRPM = 2000.0f;
    SpinAxis = FVector(0.0f, 0.0f, 1.0f);
}

UCricketVariationDeliveryAbility::UCricketVariationDeliveryAbility()
{
    DeliveryType = EDeliveryType::SlowerBall;
    PaceMultiplier = 0.7f;
    ControlDifficulty = 0.7f;
}

UCricketRunupRhythmAbility::UCricketRunupRhythmAbility()
{
    TotalSteps = 15;
    StepInterval = 0.4f;
    PerfectWindow = 0.05f;
    GoodWindow = 0.1f;
    CurrentStep = 0;
}

void UCricketRunupRhythmAbility::OnStepInput(float Timing)
{
    if (CurrentStep < TotalSteps)
    {
        StepTimings.Add(Timing);
        CurrentStep++;
    }
}

float UCricketRunupRhythmAbility::GetRhythmQuality() const
{
    if (StepTimings.Num() == 0)
    {
        return 0.0f;
    }

    float TotalError = 0.0f;
    float ExpectedTime = 0.0f;

    for (float Timing : StepTimings)
    {
        TotalError += FMath::Abs(Timing - ExpectedTime);
        ExpectedTime += StepInterval;
    }

    float AvgError = TotalError / StepTimings.Num();
    float Quality = 1.0f - FMath::Clamp(AvgError / GoodWindow, 0.0f, 1.0f);

    return Quality;
}

// ============================================================================
// FIELDING ABILITIES
// ============================================================================

UCricketFieldingAbility::UCricketFieldingAbility()
{
    ActionType = EFieldingActionType::Move;
    MoveSpeed = 5.0f;
    Acceleration = 10.0f;
    TurnRate = 360.0f;
    PredictionTime = 2.0f;
    InterceptionRadius = 2.0f;
    ReactionTime = 0.2f;
    DiveReach = 1.5f;
    DiveCooldown = 2.0f;
    MaxThrowSpeed = 30.0f;
    ThrowAccuracyBase = 0.7f;
    ThrowChargeTime = 1.0f;
    CatchRadius = 0.5f;
    CatchProbabilityBase = 0.8f;
}

void UCricketFieldingAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    ACricketFieldingPawn* FieldingPawn = GetFieldingPawn(ActorInfo);
    if (FieldingPawn)
    {
        CurrentFieldingInput = FFieldingInputState();
        FieldingPawn->OnFieldingAbilityActivated(this);
    }
}

void UCricketFieldingAbility::OnBallInPlay(ACricketBall* Ball, const FVector& PredictedLanding)
{
    // Calculate interception point
}

void UCricketFieldingAbility::OnInterceptionPointCalculated(const FVector& InterceptionPoint, float TimeToArrive)
{
    // Move to interception point
}

void UCricketFieldingAbility::OnMovementStarted(const FVector& TargetLocation)
{
    // Start movement animation
}

void UCricketFieldingAbility::OnDiveTriggered(const FVector& DiveDirection)
{
    CurrentFieldingInput.bDiveTriggered = true;
    CurrentFieldingInput.DiveDirection = DiveDirection;
}

void UCricketFieldingAbility::OnThrowTriggered(const FVector& TargetLocation, float Power)
{
    CurrentFieldingInput.bThrowTriggered = true;
    CurrentFieldingInput.ThrowTarget = TargetLocation;
    CurrentFieldingInput.ThrowPower = Power;
}

void UCricketFieldingAbility::OnCatchAttempt(const FVector& BallLocation, const FVector& BallVelocity)
{
    // Attempt catch
}

void UCricketFieldingAbility::OnFieldingActionComplete(EFieldingOutcome Outcome)
{
    ACricketFieldingPawn* FieldingPawn = GetFieldingPawn(GetActorInfo());
    if (FieldingPawn)
    {
        FieldingPawn->OnFieldingAbilityComplete(this, Outcome);
    }
}

void UCricketFieldingAbility::SetFieldingInput(const FVector2D& MoveInput, bool bDive, bool bThrow, const FVector& ThrowTarget, float ThrowPower)
{
    CurrentFieldingInput.MoveInput = MoveInput;
    CurrentFieldingInput.bDiveTriggered = bDive;
    CurrentFieldingInput.bThrowTriggered = bThrow;
    CurrentFieldingInput.ThrowTarget = ThrowTarget;
    CurrentFieldingInput.ThrowPower = ThrowPower;
}

EFieldingActionType UCricketFieldingAbility::GetFieldingActionType() const
{
    return ActionType;
}

void UCricketFieldingAbility::SetEffectivenessModifier(float Modifier)
{
    // Store modifier for use in ability calculations
    // Could apply to movement speed, catch probability, throw accuracy, etc.
    EffectivenessModifier = Modifier;
}

UCricketInterceptionAbility::UCricketInterceptionAbility()
{
    ActionType = EFieldingActionType::GroundFielding;
}

UCricketDiveAbility::UCricketDiveAbility()
{
    ActionType = EFieldingActionType::Dive;
}

UCricketThrowAbility::UCricketThrowAbility()
{
    ActionType = EFieldingActionType::Throw;
}

UCricketCatchAbility::UCricketCatchAbility()
{
    ActionType = EFieldingActionType::Catch;
}

UCricketRunOutAbility::UCricketRunOutAbility()
{
    ActionType = EFieldingActionType::RunOut;
}

// ============================================================================
// WICKETKEEPING ABILITY
// ============================================================================

UCricketWicketkeepingAbility::UCricketWicketkeepingAbility()
{
    Stance = EWicketkeeperStance::Crouching;
    StanceHeight = 1.0f;
    CollectionRadius = 0.8f;
    StumpingSpeed = 0.15f;
    StumpingAccuracy = 0.85f;
    ByesPredictionRadius = 1.0f;
}

void UCricketWicketkeepingAbility::OnStanceChanged(EWicketkeeperStance NewStance)
{
    Stance = NewStance;
}

void UCricketWicketkeepingAbility::OnBallApproaching(const FVector& BallLocation, const FVector& BallVelocity, float TimeToArrive)
{
    // Move hands to predicted contact point
}

void UCricketWicketkeepingAbility::OnCollectionAttempt(const FVector& BallLocation)
{
    // Attempt to collect ball
}

void UCricketWicketkeepingAbility::OnStumpingAttempt(const FVector& BallLocation, bool bBatterGrounded)
{
    // Break stumps
}

void UCricketWicketkeepingAbility::OnByesConceded(int32 Runs)
{
    // Handle byes
}

void UCricketWicketkeepingAbility::SetWicketkeepingInput(float StanceHeightInput, bool bMoveToBall, bool bStumpingAttempt)
{
    StanceHeight = StanceHeightInput;
}

// ============================================================================
// GAMEPLAY EFFECTS
// ============================================================================

UCricketStaminaCostEffect::UCricketStaminaCostEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    StaminaCost = 1.0f;
    bScaleWithEffort = true;
}

UCricketPressureEffect::UCricketPressureEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Duration = 30.0f;
    PressureLevel = 0.0f;
    TimingWindowReduction = 0.0f;
    ControlReduction = 0.0f;
}

UCricketFormEffect::UCricketFormEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FormLevel = 0.0f;
    TimingWindowBonus = 0.0f;
    PowerBonus = 0.0f;
    ControlBonus = 0.0f;
}

UCricketFatigueEffect::UCricketFatigueEffect()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    Duration = 60.0f;
    FatigueLevel = 0.0f;
    PaceReduction = 0.0f;
    AccuracyReduction = 0.0f;
    ReactionTimeIncrease = 0.0f;
}

// ============================================================================
// CRICKET ABILITY SYSTEM COMPONENT
// ============================================================================

UCricketAbilitySystemComponent::UCricketAbilitySystemComponent()
{
    // Default attributes will be set by AttributeSet
}

float UCricketAbilitySystemComponent::GetBattingAttribute(EBattingAttribute Attribute) const
{
    if (!CricketAttributes)
    {
        CricketAttributes = GetSet<UCricketAttributeSet>();
    }

    if (CricketAttributes)
    {
        switch (Attribute)
        {
            case EBattingAttribute::Technique: return CricketAttributes->GetTechnique();
            case EBattingAttribute::Power: return CricketAttributes->GetPower();
            case EBattingAttribute::Timing: return CricketAttributes->GetTiming();
            case EBattingAttribute::Footwork: return CricketAttributes->GetFootwork();
            case EBattingAttribute::Temperament: return CricketAttributes->GetTemperament();
            case EBattingAttribute::Concentration: return CricketAttributes->GetConcentration();
        }
    }
    return 50.0f; // Default
}

float UCricketAbilitySystemComponent::GetBowlingAttribute(EBowlingAttribute Attribute) const
{
    if (!CricketAttributes)
    {
        CricketAttributes = GetSet<UCricketAttributeSet>();
    }

    if (CricketAttributes)
    {
        switch (Attribute)
        {
            case EBowlingAttribute::Pace: return CricketAttributes->GetBowlingPace();
            case EBowlingAttribute::Swing: return CricketAttributes->GetBowlingSwing();
            case EBowlingAttribute::Seam: return CricketAttributes->GetBowlingSeam();
            case EBowlingAttribute::Spin: return CricketAttributes->GetBowlingSpin();
            case EBowlingAttribute::Control: return CricketAttributes->GetBowlingControl();
            case EBowlingAttribute::Stamina: return CricketAttributes->GetBowlingStamina();
            case EBowlingAttribute::Variations: return CricketAttributes->GetBowlingVariations();
        }
    }
    return 50.0f;
}

float UCricketAbilitySystemComponent::GetFieldingAttribute(EFieldingAttribute Attribute) const
{
    if (!CricketAttributes)
    {
        CricketAttributes = GetSet<UCricketAttributeSet>();
    }

    if (CricketAttributes)
    {
        switch (Attribute)
        {
            case EFieldingAttribute::Agility: return CricketAttributes->GetAgility();
            case EFieldingAttribute::Reflexes: return CricketAttributes->GetReflexes();
            case EFieldingAttribute::Throwing: return CricketAttributes->GetThrowing();
            case EFieldingAttribute::Catching: return CricketAttributes->GetCatching();
            case EFieldingAttribute::Anticipation: return CricketAttributes->GetAnticipation();
        }
    }
    return 50.0f;
}

float UCricketAbilitySystemComponent::GetCurrentStamina() const
{
    if (CricketAttributes)
    {
        return CricketAttributes->GetCurrentStamina();
    }
    return 100.0f;
}

float UCricketAbilitySystemComponent::GetMaxStamina() const
{
    if (CricketAttributes)
    {
        return CricketAttributes->GetMaxStamina();
    }
    return 100.0f;
}

void UCricketAbilitySystemComponent::ConsumeStamina(float Amount)
{
    if (CricketAttributes)
    {
        CricketAttributes->SetCurrentStamina(FMath::Max(0.0f, CricketAttributes->GetCurrentStamina() - Amount));
    }
}

void UCricketAbilitySystemComponent::RegenerateStamina(float DeltaTime)
{
    if (CricketAttributes)
    {
        float RegenRate = 5.0f; // Stamina per second
        CricketAttributes->SetCurrentStamina(FMath::Min(CricketAttributes->GetMaxStamina(), CricketAttributes->GetCurrentStamina() + RegenRate * DeltaTime));
    }
}

float UCricketAbilitySystemComponent::GetPressureLevel() const
{
    if (CricketAttributes)
    {
        return CricketAttributes->GetPressureLevel();
    }
    return 0.0f;
}

void UCricketAbilitySystemComponent::SetPressureLevel(float Level)
{
    if (CricketAttributes)
    {
        CricketAttributes->SetPressureLevel(FMath::Clamp(Level, 0.0f, 1.0f));
    }
}

float UCricketAbilitySystemComponent::GetFormLevel() const
{
    if (CricketAttributes)
    {
        return CricketAttributes->GetFormLevel();
    }
    return 0.0f;
}

void UCricketAbilitySystemComponent::UpdateForm(float PerformanceDelta)
{
    if (CricketAttributes)
    {
        float NewForm = FMath::Clamp(CricketAttributes->GetFormLevel() + PerformanceDelta * 0.1f, -1.0f, 1.0f);
        CricketAttributes->SetFormLevel(NewForm);
    }
}

float UCricketAbilitySystemComponent::GetFatigueLevel() const
{
    if (CricketAttributes)
    {
        return CricketAttributes->GetFatigueLevel();
    }
    return 0.0f;
}

void UCricketAbilitySystemComponent::AddFatigue(float Amount)
{
    if (CricketAttributes)
    {
        CricketAttributes->SetFatigueLevel(FMath::Clamp(CricketAttributes->GetFatigueLevel() + Amount, 0.0f, 1.0f));
    }
}

void UCricketAbilitySystemComponent::RecoverFatigue(float DeltaTime)
{
    if (CricketAttributes)
    {
        float RecoveryRate = 0.01f; // Per second
        CricketAttributes->SetFatigueLevel(FMath::Max(0.0f, CricketAttributes->GetFatigueLevel() - RecoveryRate * DeltaTime));
    }
}

void UCricketAbilitySystemComponent::GrantBattingAbilities(const TArray<TSubclassOf<UCricketShotAbility>>& Abilities)
{
    for (TSubclassOf<UCricketShotAbility> AbilityClass : Abilities)
    {
        if (AbilityClass)
        {
            GiveAbility(FGameplayAbilitySpec(AbilityClass.GetDefaultObject()));
        }
    }
}

void UCricketAbilitySystemComponent::GrantBowlingAbilities(const TArray<TSubclassOf<UCricketBowlingAbility>>& Abilities)
{
    for (TSubclassOf<UCricketBowlingAbility> AbilityClass : Abilities)
    {
        if (AbilityClass)
        {
            GiveAbility(FGameplayAbilitySpec(AbilityClass.GetDefaultObject()));
        }
    }
}

void UCricketAbilitySystemComponent::GrantFieldingAbilities(const TArray<TSubclassOf<UCricketFieldingAbility>>& Abilities)
{
    for (TSubclassOf<UCricketFieldingAbility> AbilityClass : Abilities)
    {
        if (AbilityClass)
        {
            GiveAbility(FGameplayAbilitySpec(AbilityClass.GetDefaultObject()));
        }
    }
}

UCricketShotAbility* UCricketAbilitySystemComponent::GetActiveShotAbility() const
{
    FGameplayTagContainer ShotTags;
    ShotTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cricket.Shot")));

    TArray<FGameplayAbilitySpec*> ActiveAbilities;
    GetActivatableAbilities(ShotTags, ActiveAbilities);

    for (FGameplayAbilitySpec* Spec : ActiveAbilities)
    {
        if (UCricketShotAbility* ShotAbility = Cast<UCricketShotAbility>(Spec->Ability))
        {
            return ShotAbility;
        }
    }
    return nullptr;
}

UCricketBowlingAbility* UCricketAbilitySystemComponent::GetActiveBowlingAbility() const
{
    FGameplayTagContainer BowlTags;
    BowlTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cricket.Delivery")));

    TArray<FGameplayAbilitySpec*> ActiveAbilities;
    GetActivatableAbilities(BowlTags, ActiveAbilities);

    for (FGameplayAbilitySpec* Spec : ActiveAbilities)
    {
        if (UCricketBowlingAbility* BowlAbility = Cast<UCricketBowlingAbility>(Spec->Ability))
        {
            return BowlAbility;
        }
    }
    return nullptr;
}

UCricketFieldingAbility* UCricketAbilitySystemComponent::GetActiveFieldingAbility() const
{
    FGameplayTagContainer FieldTags;
    FieldTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Cricket.Fielding")));

    TArray<FGameplayAbilitySpec*> ActiveAbilities;
    GetActivatableAbilities(FieldTags, ActiveAbilities);

    for (FGameplayAbilitySpec* Spec : ActiveAbilities)
    {
        if (UCricketFieldingAbility* FieldAbility = Cast<UCricketFieldingAbility>(Spec->Ability))
        {
            return FieldAbility;
        }
    }
    return nullptr;
}

// ============================================================================
// CRICKET ATTRIBUTE SET
// ============================================================================

UCricketAttributeSet::UCricketAttributeSet()
{
    // Initialize default values
    Technique.SetBaseValue(70.0f);
    Power.SetBaseValue(60.0f);
    Timing.SetBaseValue(70.0f);
    Footwork.SetBaseValue(65.0f);
    Temperament.SetBaseValue(70.0f);
    Concentration.SetBaseValue(70.0f);

    BowlingPace.SetBaseValue(70.0f);
    BowlingSwing.SetBaseValue(60.0f);
    BowlingSeam.SetBaseValue(60.0f);
    BowlingSpin.SetBaseValue(50.0f);
    BowlingControl.SetBaseValue(70.0f);
    BowlingStamina.SetBaseValue(70.0f);
    BowlingVariations.SetBaseValue(50.0f);

    Agility.SetBaseValue(70.0f);
    Reflexes.SetBaseValue(70.0f);
    Throwing.SetBaseValue(70.0f);
    Catching.SetBaseValue(70.0f);
    Anticipation.SetBaseValue(70.0f);

    CurrentStamina.SetBaseValue(100.0f);
    MaxStamina.SetBaseValue(100.0f);
    PressureLevel.SetBaseValue(0.0f);
    FormLevel.SetBaseValue(0.0f);
    FatigueLevel.SetBaseValue(0.0f);

    WKKeeping.SetBaseValue(50.0f);
    WKStumping.SetBaseValue(50.0f);
    WKReactions.SetBaseValue(50.0f);
}

// Batting attribute replication callbacks
void UCricketAttributeSet::OnRep_Technique(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Technique, OldValue); }
void UCricketAttributeSet::OnRep_Power(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Power, OldValue); }
void UCricketAttributeSet::OnRep_Timing(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Timing, OldValue); }
void UCricketAttributeSet::OnRep_Footwork(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Footwork, OldValue); }
void UCricketAttributeSet::OnRep_Temperament(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Temperament, OldValue); }
void UCricketAttributeSet::OnRep_Concentration(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Concentration, OldValue); }

// Bowling attribute replication callbacks
void UCricketAttributeSet::OnRep_BowlingPace(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingPace, OldValue); }
void UCricketAttributeSet::OnRep_BowlingSwing(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingSwing, OldValue); }
void UCricketAttributeSet::OnRep_BowlingSeam(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingSeam, OldValue); }
void UCricketAttributeSet::OnRep_BowlingSpin(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingSpin, OldValue); }
void UCricketAttributeSet::OnRep_BowlingControl(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingControl, OldValue); }
void UCricketAttributeSet::OnRep_BowlingStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingStamina, OldValue); }
void UCricketAttributeSet::OnRep_BowlingVariations(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, BowlingVariations, OldValue); }

// Fielding attribute replication callbacks
void UCricketAttributeSet::OnRep_Agility(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Agility, OldValue); }
void UCricketAttributeSet::OnRep_Reflexes(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Reflexes, OldValue); }
void UCricketAttributeSet::OnRep_Throwing(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Throwing, OldValue); }
void UCricketAttributeSet::OnRep_Catching(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Catching, OldValue); }
void UCricketAttributeSet::OnRep_Anticipation(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, Anticipation, OldValue); }

// Dynamic attribute replication callbacks
void UCricketAttributeSet::OnRep_CurrentStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, CurrentStamina, OldValue); }
void UCricketAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, MaxStamina, OldValue); }
void UCricketAttributeSet::OnRep_PressureLevel(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, PressureLevel, OldValue); }
void UCricketAttributeSet::OnRep_FormLevel(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, FormLevel, OldValue); }
void UCricketAttributeSet::OnRep_FatigueLevel(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, FatigueLevel, OldValue); }

// Wicketkeeping replication callbacks
void UCricketAttributeSet::OnRep_WKKeeping(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, WKKeeping, OldValue); }
void UCricketAttributeSet::OnRep_WKStumping(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, WKStumping, OldValue); }
void UCricketAttributeSet::OnRep_WKReactions(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UCricketAttributeSet, WKReactions, OldValue); }