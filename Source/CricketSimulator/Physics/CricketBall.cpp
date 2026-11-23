#include "CricketBall.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsThrusterComponent.h"
#include "PitchActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/ProjectileMovementComponent.h"

ACricketBall::ACricketBall()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f; // Every frame for physics

    // Collision
    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    CollisionComponent->InitSphereRadius(3.6f); // 3.6cm radius in cm
    CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionComponent->SetSimulatePhysics(false); // We handle physics manually
    CollisionComponent->SetNotifyRigidBodyCollision(true);
    CollisionComponent->OnComponentHit.AddDynamic(this, &ACricketBall::OnHit);
    RootComponent = CollisionComponent;

    // Visual mesh
    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
    BallMesh->SetupAttachment(RootComponent);
    BallMesh->SetRelativeScale3D(FVector(0.072f)); // 7.2cm diameter
    BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BallMesh->SetCastShadow(true);

    // Thruster for aerodynamic forces (alternative: manual force application)
    ThrusterComponent = CreateDefaultSubobject<UPhysicsThrusterComponent>(TEXT("Thruster"));
    ThrusterComponent->SetupAttachment(RootComponent);
    ThrusterComponent->SetAutoActivate(false);

    // Replication
    bReplicates = true;
    SetReplicateMovement(false); // We replicate state manually

    // Defaults
    BallState = EBallState::InHand;
    bIsSimulating = false;
    TimeSinceRelease = 0.0f;
}

void ACricketBall::BeginPlay()
{
    Super::BeginPlay();

    // Find pitch actor
    TArray<AActor*> PitchActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APitchActor::StaticClass(), PitchActors);
    if (PitchActors.Num() > 0)
    {
        PitchActor = Cast<APitchActor>(PitchActors[0]);
    }

    // Initialize physics state
    PhysicsState.Position = GetActorLocation();
    PhysicsState.Velocity = FVector::ZeroVector;
    PhysicsState.AngularVelocity = FVector::ZeroVector;
    PhysicsState.SeamOrientation = FVector::ForwardVector;
    PhysicsState.BallAge = 0.0f;
    PhysicsState.SeamWear = 0.0f;
    PhysicsState.SurfaceRoughness = 0.0f;
    PhysicsState.Bounces = 0;
    PhysicsState.TimeSinceBounce = 0.0f;
}

void ACricketBall::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsSimulating)
    {
        SimulatePhysics(DeltaSeconds);
        ApplyWear(DeltaSeconds);
        TimeSinceRelease += DeltaSeconds;
        PhysicsState.TimeSinceBounce += DeltaSeconds;
    }

    UpdateVisuals();
}

void ACricketBall::Initialize(EBallType InBallType, const FBallPhysicsParams& InParams)
{
    BallType = InBallType;
    PhysicsParams = InParams;

    // Adjust parameters based on ball type
    switch (BallType)
    {
        case EBallType::RedBall:
            PhysicsParams.DragCoefficientBase = 0.5f;
            PhysicsParams.MagnusCoefficient = 1.2e-4f;
            PhysicsParams.CoefficientOfRestitution = 0.6f;
            break;
        case EBallType::WhiteBall:
            PhysicsParams.DragCoefficientBase = 0.48f; // Slightly less drag
            PhysicsParams.MagnusCoefficient = 1.15e-4f;
            PhysicsParams.CoefficientOfRestitution = 0.58f;
            break;
        case EBallType::PinkBall:
            PhysicsParams.DragCoefficientBase = 0.5f;
            PhysicsParams.MagnusCoefficient = 1.25e-4f; // More swing under lights
            PhysicsParams.CoefficientOfRestitution = 0.62f;
            break;
        case EBallType::TrainingBall:
            PhysicsParams.DragCoefficientBase = 0.55f;
            PhysicsParams.MagnusCoefficient = 1.0e-4f;
            PhysicsParams.CoefficientOfRestitution = 0.5f;
            break;
    }

    // Set visual material based on ball type
    UpdateVisuals();
}

void ACricketBall::SetPhysicsParams(const FBallPhysicsParams& InParams)
{
    PhysicsParams = InParams;
}

void ACricketBall::StartDelivery(const FVector& StartLocation, const FVector& InitialVelocity, const FVector& InitialSpin, const FVector& InSeamOrientation)
{
    SetActorLocation(StartLocation);
    PhysicsState.Position = StartLocation;
    PhysicsState.Velocity = InitialVelocity;
    PhysicsState.AngularVelocity = InitialSpin;
    PhysicsState.SeamOrientation = InSeamOrientation.GetSafeNormal();
    PhysicsState.Bounces = 0;
    PhysicsState.TimeSinceBounce = 0.0f;
    TimeSinceRelease = 0.0f;

    SetBallState(EBallState::InFlight);
    bIsSimulating = true;
}

void ACricketBall::ReleaseFromHand(const FVector& ReleaseLocation, const FVector& ReleaseVelocity, const FVector& ReleaseSpin)
{
    StartDelivery(ReleaseLocation, ReleaseVelocity, ReleaseSpin, PhysicsState.SeamOrientation);
}

void ACricketBall::SimulatePhysics(float DeltaSeconds)
{
    if (!bIsSimulating)
    {
        return;
    }

    // Apply aerodynamic forces
    ApplyAerodynamicForces(DeltaSeconds);

    // Apply seam movement
    ApplySeamMovement(DeltaSeconds);

    // Apply wind
    FVector WindForce = CalculateWindForce();
    PhysicsState.Velocity += WindForce * DeltaSeconds / PhysicsParams.Mass;

    // Integrate position
    PhysicsState.Position += PhysicsState.Velocity * DeltaSeconds;

    // Update actor location
    SetActorLocation(PhysicsState.Position);

    // Check for pitch collision (ground plane at Z=0, or pitch actor)
    if (PhysicsState.Position.Z <= PhysicsParams.Radius && PhysicsState.Velocity.Z < 0)
    {
        // Simulate bounce
        FHitResult HitResult;
        HitResult.Location = FVector(PhysicsState.Position.X, PhysicsState.Position.Y, PhysicsParams.Radius);
        HitResult.Normal = FVector::UpVector;
        HitResult.ImpactPoint = HitResult.Location;
        HandlePitchBounce(HitResult);
    }

    // Check for stumps collision (simplified)
    // Would check against stump actors in real implementation
}

void ACricketBall::ApplyAerodynamicForces(float DeltaSeconds)
{
    FVector Velocity = PhysicsState.Velocity;
    float Speed = Velocity.Size();

    if (Speed < 1.0f)
    {
        return;
    }

    FVector DragForce = CalculateDragForce(Velocity);
    FVector MagnusForce = CalculateMagnusForce(Velocity, PhysicsState.AngularVelocity);

    FVector TotalForce = DragForce + MagnusForce;
    FVector Acceleration = TotalForce / PhysicsParams.Mass;

    PhysicsState.Velocity += Acceleration * DeltaSeconds;

    // Gravity
    PhysicsState.Velocity.Z -= 9.81f * DeltaSeconds;
}

FVector ACricketBall::CalculateDragForce(const FVector& Velocity) const
{
    float Speed = Velocity.Size();
    if (Speed < 0.01f) return FVector::ZeroVector;

    FVector Direction = Velocity.GetSafeNormal();

    // Base drag coefficient varies with ball age and seam orientation
    float Cd = PhysicsParams.DragCoefficientBase;

    // Seam adds turbulence, increasing drag
    float SeamAlignment = FMath::Abs(FVector::DotProduct(PhysicsState.SeamOrientation, Direction));
    Cd += PhysicsParams.DragCoefficientSeam * SeamAlignment * (1.0f + PhysicsState.SeamWear);

    // Ball age increases drag
    Cd *= (1.0f + PhysicsState.BallAge * 0.2f);

    // Surface roughness
    Cd *= (1.0f + PhysicsState.SurfaceRoughness * 0.1f);

    float DynamicPressure = 0.5f * PhysicsParams.AirDensity * Speed * Speed;
    float CrossSectionalArea = PI * PhysicsParams.Radius * PhysicsParams.Radius;

    float DragMagnitude = DynamicPressure * CrossSectionalArea * Cd;

    return -Direction * DragMagnitude;
}

FVector ACricketBall::CalculateMagnusForce(const FVector& Velocity, const FVector& AngularVelocity) const
{
    // Magnus force = 0.5 * rho * v^2 * A * Cl * (omega x v) / |omega x v|
    // Simplified: F = S * (omega x v) where S is Magnus coefficient

    FVector OmegaCrossV = FVector::CrossProduct(AngularVelocity, Velocity);
    float OmegaCrossVMag = OmegaCrossV.Size();

    if (OmegaCrossVMag < 0.01f)
    {
        return FVector::ZeroVector;
    }

    float Speed = Velocity.Size();
    float SpinRate = AngularVelocity.Size();

    // Magnus coefficient varies with Reynolds number (speed) and ball condition
    float Cl = PhysicsParams.MagnusCoefficient;

    // New ball swings more
    Cl *= (1.0f - PhysicsState.BallAge * 0.3f);

    // Seam orientation affects swing
    float SeamEffect = FVector::DotProduct(PhysicsState.SeamOrientation, Velocity.GetSafeNormal());
    Cl *= (1.0f + SeamEffect * 0.5f * (1.0f - PhysicsState.SeamWear));

    float ForceMagnitude = 0.5f * PhysicsParams.AirDensity * Speed * SpinRate * PI * PhysicsParams.Radius * PhysicsParams.Radius * Cl;

    return OmegaCrossV.GetSafeNormal() * ForceMagnitude;
}

FVector ACricketBall::CalculateWindForce() const
{
    if (PhysicsParams.WindVelocity.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }

    FVector RelativeWind = PhysicsParams.WindVelocity - PhysicsState.Velocity;
    float WindSpeed = RelativeWind.Size();

    if (WindSpeed < 0.1f)
    {
        return FVector::ZeroVector;
    }

    float Cd = PhysicsParams.DragCoefficientBase * 0.5f; // Reduced for wind
    float DynamicPressure = 0.5f * PhysicsParams.AirDensity * WindSpeed * WindSpeed;
    float CrossSectionalArea = PI * PhysicsParams.Radius * PhysicsParams.Radius;

    return RelativeWind.GetSafeNormal() * DynamicPressure * CrossSectionalArea * Cd;
}

void ACricketBall::ApplySeamMovement(float DeltaSeconds)
{
    // Seam movement causes lateral deviation, especially when seam is upright
    // This is separate from Magnus effect (swing)

    float Speed = PhysicsState.Velocity.Size();
    if (Speed < 1000.0f) // Only at higher speeds (cm/s = 10 m/s)
    {
        return;
    }

    // Seam must be roughly vertical and aligned with velocity
    float SeamVerticality = FMath::Abs(PhysicsState.SeamOrientation.Z);
    float SeamAlignment = FMath::Abs(FVector::DotProduct(
        FVector(PhysicsState.SeamOrientation.X, PhysicsState.SeamOrientation.Y, 0.0f).GetSafeNormal(),
        FVector(PhysicsState.Velocity.X, PhysicsState.Velocity.Y, 0.0f).GetSafeNormal()
    ));

    if (SeamVerticality > 0.7f && SeamAlignment > 0.7f && PhysicsState.SeamWear < 0.5f)
    {
        // Random lateral force (seam deviation)
        float DeviationStrength = PhysicsParams.SeamDeviationMax * (1.0f - PhysicsState.SeamWear) * (Speed / 3000.0f);
        float LateralForce = FMath::RandRange(-DeviationStrength, DeviationStrength);

        FVector LateralDir = FVector(-PhysicsState.Velocity.Y, PhysicsState.Velocity.X, 0.0f).GetSafeNormal();
        PhysicsState.Velocity += LateralDir * LateralForce * DeltaSeconds;
    }
}

void ACricketBall::HandlePitchBounce(const FHitResult& HitResult)
{
    if (BallState == EBallState::Dead)
    {
        return;
    }

    FVector PreBounceVelocity = PhysicsState.Velocity;
    FVector SurfaceNormal = HitResult.Normal;

    // Get pitch properties at bounce location
    float PitchRestitution = PhysicsParams.CoefficientOfRestitution;
    float PitchFriction = PhysicsParams.FrictionCoefficient;

    if (PitchActor)
    {
        PitchActor->GetPitchPropertiesAtLocation(HitResult.Location, PitchRestitution, PitchFriction);
    }

    // Calculate bounce velocity
    FVector PostBounceVelocity = CalculateBounceVelocity(PreBounceVelocity, SurfaceNormal, PitchFriction, PitchRestitution);

    // Add spin-induced deviation
    FVector SpinDeviation = CalculateSpinInducedDeviation(SurfaceNormal);
    PostBounceVelocity += SpinDeviation;

    PhysicsState.Velocity = PostBounceVelocity;
    PhysicsState.Bounces++;
    PhysicsState.TimeSinceBounce = 0.0f;
    PhysicsState.Position = HitResult.Location + SurfaceNormal * PhysicsParams.Radius;
    LastBounceLocation = HitResult.Location;

    SetActorLocation(PhysicsState.Position);

    SetBallState(EBallState::Bouncing);

    // Broadcast event
    OnBallBounce.Broadcast(HitResult.Location, PostBounceVelocity);

    // Transition to next state based on velocity
    if (PostBounceVelocity.Z > 50.0f)
    {
        SetBallState(EBallState::InFlight);
    }
    else
    {
        SetBallState(EBallState::OnPitch);
    }
}

FVector ACricketBall::CalculateBounceVelocity(const FVector& InVelocity, const FVector& SurfaceNormal, float PitchFriction, float PitchRestitution) const
{
    // Decompose velocity into normal and tangential components
    float NormalSpeed = FVector::DotProduct(InVelocity, SurfaceNormal);
    FVector NormalComponent = SurfaceNormal * NormalSpeed;
    FVector TangentialComponent = InVelocity - NormalComponent;

    // Reverse normal component with restitution
    FVector OutNormalComponent = -NormalComponent * PitchRestitution;

    // Reduce tangential component by friction
    float TangentialSpeed = TangentialComponent.Size();
    if (TangentialSpeed > 0.01f)
    {
        float FrictionImpulse = FMath::Min(PitchFriction * FMath::Abs(NormalSpeed), TangentialSpeed);
        FVector OutTangentialComponent = TangentialComponent.GetSafeNormal() * (TangentialSpeed - FrictionImpulse);
        return OutNormalComponent + OutTangentialComponent;
    }

    return OutNormalComponent;
}

FVector ACricketBall::CalculateSpinInducedDeviation(const FVector& SurfaceNormal) const
{
    // Spin causes the ball to deviate sideways on bounce
    // Topspin -> dips more, backspin -> carries more
    // Sidespin -> deviates left/right

    FVector Spin = PhysicsState.AngularVelocity;
    float SpinMagnitude = Spin.Size();

    if (SpinMagnitude < 10.0f) // rad/s
    {
        return FVector::ZeroVector;
    }

    // Project spin onto pitch plane
    FVector SpinInPlane = Spin - SurfaceNormal * FVector::DotProduct(Spin, SurfaceNormal);
    float SpinInPlaneMagnitude = SpinInPlane.Size();

    if (SpinInPlaneMagnitude < 0.01f)
    {
        return FVector::ZeroVector;
    }

    // Deviation proportional to spin component in plane
    float DeviationFactor = PhysicsParams.SpinGripCoefficient * (SpinInPlaneMagnitude / 100.0f);
    DeviationFactor = FMath::Clamp(DeviationFactor, -1.0f, 1.0f);

    FVector DeviationDir = FVector::CrossProduct(SurfaceNormal, SpinInPlane).GetSafeNormal();
    float DeviationSpeed = PhysicsState.Velocity.Size() * DeviationFactor * 0.1f;

    return DeviationDir * DeviationSpeed;
}

void ACricketBall::HandleBatCollision(const FVector& ContactPoint, const FVector& BatVelocity, const FVector& BatNormal, float BatMass)
{
    if (BallState == EBallState::Dead)
    {
        return;
    }

    FVector BallVelocityBefore = PhysicsState.Velocity;
    FVector RelativeVelocity = BallVelocityBefore - BatVelocity;

    // Coefficient of restitution for bat-ball collision
    float e = PhysicsParams.BatCoefficientOfRestitution;

    // Effective mass (simplified)
    float BallMass = PhysicsParams.Mass;
    float EffectiveMass = (BallMass * BatMass) / (BallMass + BatMass);

    // Impulse
    float NormalSpeed = FVector::DotProduct(RelativeVelocity, BatNormal);
    if (NormalSpeed > 0) // Ball moving toward bat
    {
        float ImpulseMagnitude = -(1.0f + e) * EffectiveMass * NormalSpeed;
        FVector Impulse = BatNormal * ImpulseMagnitude;

        // Apply impulse to ball
        PhysicsState.Velocity += Impulse / BallMass;

        // Add spin from bat friction
        FVector TangentialRelativeVel = RelativeVelocity - BatNormal * NormalSpeed;
        float SpinImpulse = FMath::Min(TangentialRelativeVel.Size() * 0.1f, 50.0f);
        if (SpinImpulse > 0)
        {
            FVector SpinAxis = FVector::CrossProduct(BatNormal, TangentialRelativeVel.GetSafeNormal());
            PhysicsState.AngularVelocity += SpinAxis * SpinImpulse / (0.4f * BallMass * PhysicsParams.Radius * PhysicsParams.Radius);
        }
    }

    PhysicsState.Position = ContactPoint;
    SetActorLocation(PhysicsState.Position);

    SetBallState(EBallState::HitBat);

    // Broadcast event
    OnBallHitBat.Broadcast(ContactPoint, BallVelocityBefore, BatVelocity);

    // Ball is now in flight again
    SetBallState(EBallState::InFlight);
}

void ACricketBall::HandlePadCollision(const FVector& ContactPoint, const FVector& PadNormal)
{
    if (BallState == EBallState::Dead)
    {
        return;
    }

    // Pad is soft - high energy absorption
    float PadRestitution = 0.1f;
    FVector RelativeVelocity = PhysicsState.Velocity; // Pad is stationary
    float NormalSpeed = FVector::DotProduct(RelativeVelocity, PadNormal);

    if (NormalSpeed > 0)
    {
        FVector NormalComponent = PadNormal * NormalSpeed;
        FVector TangentialComponent = RelativeVelocity - NormalComponent;

        PhysicsState.Velocity = -NormalComponent * PadRestitution + TangentialComponent * 0.3f;
    }

    PhysicsState.Position = ContactPoint;
    SetActorLocation(PhysicsState.Position);

    SetBallState(EBallState::HitPad);
    SetBallState(EBallState::InFlight); // Continues after pad
}

void ACricketBall::SetBallState(EBallState NewState)
{
    if (BallState != NewState)
    {
        BallState = NewState;
        OnBallStateChanged.Broadcast(NewState);
        UE_LOG(LogTemp, Log, TEXT("Ball state changed to: %s"), *UEnum::GetValueAsString(NewState));
    }
}

FVector ACricketBall::GetPredictedPitchLocation(float TimeAhead) const
{
    // Simple projectile prediction ignoring aerodynamics for quick estimate
    FVector Pos = PhysicsState.Position;
    FVector Vel = PhysicsState.Velocity;

    float t = 0.0f;
    float dt = 0.01f;

    while (t < TimeAhead)
    {
        Vel.Z -= 9.81f * dt;
        Pos += Vel * dt;
        t += dt;

        if (Pos.Z <= PhysicsParams.Radius)
        {
            return FVector(Pos.X, Pos.Y, PhysicsParams.Radius);
        }
    }

    return Pos;
}

FVector ACricketBall::GetPredictedBatterContactPoint(float BatterCreaseDistance) const
{
    // Predict where ball will be at batter's crease distance
    // Assuming bowler at origin, batter at X = CreaseDistance
    FVector Pos = PhysicsState.Position;
    FVector Vel = PhysicsState.Velocity;

    if (FMath::Abs(Vel.X) < 1.0f)
    {
        return Pos;
    }

    float TimeToCrease = (BatterCreaseDistance - Pos.X) / Vel.X;
    if (TimeToCrease < 0) TimeToCrease = 0;

    return GetPredictedPitchLocation(TimeToCrease);
}

TArray<FVector> ACricketBall::PredictTrajectory(float TimeStep, int32 MaxSteps, bool bIncludeBounce) const
{
    TArray<FVector> Trajectory;
    Trajectory.Reserve(MaxSteps);

    FVector Pos = PhysicsState.Position;
    FVector Vel = PhysicsState.Velocity;
    FVector Spin = PhysicsState.AngularVelocity;
    FVector Seam = PhysicsState.SeamOrientation;

    for (int32 i = 0; i < MaxSteps; i++)
    {
        Trajectory.Add(Pos);

        // Apply forces
        FVector DragForce = CalculateDragForce(Vel);
        FVector MagnusForce = CalculateMagnusForce(Vel, Spin);
        FVector WindForce = CalculateWindForce();

        FVector Acceleration = (DragForce + MagnusForce + WindForce) / PhysicsParams.Mass;
        Acceleration.Z -= 9.81f;

        Vel += Acceleration * TimeStep;
        Pos += Vel * TimeStep;

        // Check for bounce
        if (Pos.Z <= PhysicsParams.Radius && Vel.Z < 0)
        {
            if (bIncludeBounce)
            {
                // Simulate bounce
                FVector SurfaceNormal = FVector::UpVector;
                float PitchRestitution = PhysicsParams.CoefficientOfRestitution;
                float PitchFriction = PhysicsParams.FrictionCoefficient;
                Vel = CalculateBounceVelocity(Vel, SurfaceNormal, PitchFriction, PitchRestitution);
                Pos.Z = PhysicsParams.Radius;
            }
            else
            {
                break; // Stop at bounce
            }
        }

        // Stop if ball is very slow
        if (Vel.Size() < 10.0f)
        {
            break;
        }
    }

    return Trajectory;
}

void ACricketBall::ApplyWear(float DeltaSeconds)
{
    // Ball ages over time
    PhysicsState.BallAge += DeltaSeconds / 3600.0f; // Full aging over 1 hour of play
    PhysicsState.BallAge = FMath::Clamp(PhysicsState.BallAge, 0.0f, 1.0f);

    // Seam wears down
    PhysicsState.SeamWear += DeltaSeconds / 7200.0f; // Seam lasts ~2 hours
    PhysicsState.SeamWear = FMath::Clamp(PhysicsState.SeamWear, 0.0f, 1.0f);

    // Surface roughens
    PhysicsState.SurfaceRoughness += DeltaSeconds / 10800.0f;
    PhysicsState.SurfaceRoughness = FMath::Clamp(PhysicsState.SurfaceRoughness, 0.0f, 1.0f);
}

float ACricketBall::GetSwingPotential() const
{
    // New ball with good seam swings more
    return (1.0f - PhysicsState.BallAge * 0.5f) * (1.0f - PhysicsState.SeamWear * 0.8f);
}

float ACricketBall::GetSpinPotential() const
{
    // Older, rougher ball spins more
    return (0.3f + PhysicsState.BallAge * 0.4f) * (1.0f + PhysicsState.SurfaceRoughness * 0.5f);
}

void ACricketBall::UpdateVisuals()
{
    // Update ball mesh rotation based on spin
    if (BallMesh && PhysicsState.AngularVelocity.Size() > 0.1f)
    {
        FQuat CurrentRot = BallMesh->GetComponentQuat();
        FQuat DeltaRot = FQuat(PhysicsState.AngularVelocity * GetWorld()->GetDeltaSeconds());
        BallMesh->SetWorldRotation(CurrentRot * DeltaRot);
    }

    // Could update material parameters for wear visualization
}

void ACricketBall::UpdateReplication()
{
    // Manual replication of physics state
    // In a real implementation, this would use a custom replication graph or
    // replicate the physics state struct on a fixed interval
}

void ACricketBall::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this)
    {
        // Check what we hit
        if (OtherActor->ActorHasTag("Bat"))
        {
            // Would get bat velocity from batting pawn
            HandleBatCollision(Hit.Location, FVector::ZeroVector, Hit.Normal, 1.2f);
        }
        else if (OtherActor->ActorHasTag("Pad"))
        {
            HandlePadCollision(Hit.Location, Hit.Normal);
        }
        else if (OtherActor->ActorHasTag("Stumps"))
        {
            SetBallState(EBallState::Dead);
        }
        else if (OtherActor->ActorHasTag("Pitch"))
        {
            HandlePitchBounce(Hit);
        }
    }
}