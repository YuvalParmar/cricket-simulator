#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsThrusterComponent.h"
#include "CricketBall.generated.h"

class APitchActor;
class UPhysicalMaterial;

UENUM(BlueprintType)
enum class EBallType : uint8
{
    RedBall         UMETA(DisplayName = "Red Ball (Test)"),
    WhiteBall       UMETA(DisplayName = "White Ball (ODI/T20)"),
    PinkBall        UMETA(DisplayName = "Pink Ball (Day/Night)"),
    TrainingBall    UMETA(DisplayName = "Training Ball")
};

UENUM(BlueprintType)
enum class EBallState : uint8
{
    InHand          UMETA(DisplayName = "In Hand"),
    InRunUp         UMETA(DisplayName = "In Run-Up"),
    InFlight        UMETA(DisplayName = "In Flight"),
    Bouncing        UMETA(DisplayName = "Bouncing"),
    OnPitch         UMETA(DisplayName = "On Pitch"),
    HitBat          UMETA(DisplayName = "Hit Bat"),
    HitPad          UMETA(DisplayName = "Hit Pad"),
    Dead            UMETA(DisplayName = "Dead")
};

USTRUCT(BlueprintType)
struct FBallPhysicsState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector Position;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector Velocity;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector AngularVelocity; // Spin

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    FVector SeamOrientation; // Normalized seam direction

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float BallAge = 0.0f; // 0.0 = new, 1.0 = old

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float SeamWear = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float SurfaceRoughness = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    int32 Bounces = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Physics")
    float TimeSinceBounce = 0.0f;
};

USTRUCT(BlueprintType)
struct FBallPhysicsParams
{
    GENERATED_BODY()

    // Aerodynamics
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float Mass = 0.156f; // kg (standard cricket ball)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float Radius = 0.036f; // meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float DragCoefficientBase = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float DragCoefficientSeam = 0.1f; // Additional drag from seam

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float LiftCoefficientBase = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float MagnusCoefficient = 1.2e-4f; // Magnus effect strength

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aerodynamics")
    float AirDensity = 1.225f; // kg/m^3 at sea level

    // Pitch Interaction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    float CoefficientOfRestitution = 0.6f; // Bounciness

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    float FrictionCoefficient = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    float SeamDeviationMax = 0.15f; // Max sideways deviation from seam (meters)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
    float SpinGripCoefficient = 0.3f; // How much spin affects bounce angle

    // Bat Interaction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat")
    float BatCoefficientOfRestitution = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat")
    float SweetSpotRadius = 0.04f; // 4cm sweet spot

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat")
    float EdgeDeflectionAngleMax = 45.0f; // Max deflection off edge (degrees)

    // Environmental
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FVector WindVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    float Humidity = 0.5f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    float Temperature = 20.0f; // Celsius
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBallBounce, const FVector&, Location, const FVector&, VelocityAfter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBallHitBat, const FVector&, ContactPoint, const FVector&, BallVelocityBefore, const FVector&, BatVelocity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBallStateChanged, EBallState, NewState);

UCLASS(Blueprintable)
class CRICKETSIMULATOR_API ACricketBall : public AActor
{
    GENERATED_BODY()

public:
    ACricketBall();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Ball initialization
    UFUNCTION(BlueprintCallable, Category = "Ball")
    void Initialize(EBallType InBallType, const FBallPhysicsParams& InParams);

    UFUNCTION(BlueprintCallable, Category = "Ball")
    void SetPhysicsParams(const FBallPhysicsParams& InParams);

    // Delivery
    UFUNCTION(BlueprintCallable, Category = "Ball")
    void StartDelivery(const FVector& StartLocation, const FVector& InitialVelocity, const FVector& InitialSpin, const FVector& SeamOrientation);

    UFUNCTION(BlueprintCallable, Category = "Ball")
    void ReleaseFromHand(const FVector& ReleaseLocation, const FVector& ReleaseVelocity, const FVector& ReleaseSpin);

    // Physics simulation
    UFUNCTION(BlueprintCallable, Category = "Physics")
    void SimulatePhysics(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "Physics")
    void ApplyAerodynamicForces(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "Physics")
    void HandlePitchBounce(const FHitResult& HitResult);

    UFUNCTION(BlueprintCallable, Category = "Physics")
    void HandleBatCollision(const FVector& ContactPoint, const FVector& BatVelocity, const FVector& BatNormal, float BatMass);

    UFUNCTION(BlueprintCallable, Category = "Physics")
    void HandlePadCollision(const FVector& ContactPoint, const FVector& PadNormal);

    // State
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetBallState(EBallState NewState);

    UFUNCTION(BlueprintPure, Category = "State")
    EBallState GetBallState() const { return BallState; }

    UFUNCTION(BlueprintPure, Category = "State")
    const FBallPhysicsState& GetPhysicsState() const { return PhysicsState; }

    UFUNCTION(BlueprintPure, Category = "State")
    FVector GetPredictedPitchLocation(float TimeAhead) const;

    UFUNCTION(BlueprintPure, Category = "State")
    FVector GetPredictedBatterContactPoint(float BatterCreaseDistance) const;

    // Trajectory prediction for UI/DRS
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    TArray<FVector> PredictTrajectory(float TimeStep, int32 MaxSteps, bool bIncludeBounce) const;

    // Wear and tear
    UFUNCTION(BlueprintCallable, Category = "Wear")
    void ApplyWear(float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category = "Wear")
    float GetSwingPotential() const;

    UFUNCTION(BlueprintPure, Category = "Wear")
    float GetSpinPotential() const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBallBounce OnBallBounce;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBallHitBat OnBallHitBat;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBallStateChanged OnBallStateChanged;

protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USphereComponent> CollisionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> BallMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UPhysicsThrusterComponent> ThrusterComponent; // For aerodynamic forces

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    EBallType BallType = EBallType::RedBall;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FBallPhysicsParams PhysicsParams;

    // State
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    EBallState BallState = EBallState::InHand;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FBallPhysicsState PhysicsState;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    bool bIsSimulating = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    FVector LastBounceLocation;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "State")
    float TimeSinceRelease = 0.0f;

    // References
    UPROPERTY()
    APitchActor* PitchActor;

    // Internal
    void UpdateVisuals();
    void UpdateReplication();
    FVector CalculateMagnusForce(const FVector& Velocity, const FVector& AngularVelocity) const;
    FVector CalculateDragForce(const FVector& Velocity) const;
    FVector CalculateWindForce() const;
    void ProcessBouncePhysics(const FHitResult& HitResult, const FVector& PreBounceVelocity);
    FVector CalculateBounceVelocity(const FVector& InVelocity, const FVector& SurfaceNormal, float PitchFriction, float PitchRestitution) const;
    FVector CalculateSpinInducedDeviation(const FVector& SurfaceNormal) const;
    void ApplySeamMovement(float DeltaSeconds);
};