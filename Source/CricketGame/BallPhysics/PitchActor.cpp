#include "PitchActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "CricketDataAssets.h"

APitchActor::APitchActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Pitch mesh
    PitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PitchMesh"));
    PitchMesh->SetCollisionProfileName(TEXT("BlockAll"));
    PitchMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    RootComponent = PitchMesh;

    // Pitch bounds for queries
    PitchBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PitchBounds"));
    PitchBounds->SetupAttachment(RootComponent);
    PitchBounds->SetBoxExtent(FVector(PitchLength / 2.0f, PitchWidth / 2.0f, 50.0f));
    PitchBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PitchBounds->SetCollisionProfileName(TEXT("OverlapAll"));

    // Initialize zone grid
    InitializeZoneGrid();

    bReplicates = true;
}

void APitchActor::BeginPlay()
{
    Super::BeginPlay();

    // Apply default profile if not set
    if (CurrentProfile.ZoneProperties.Num() == 0)
    {
        FPitchProfileData DefaultProfile;
        DefaultProfile.PitchType = EPitchType::Standard;
        DefaultProfile.ProfileName = "Standard";
        InitializePitch(DefaultProfile);
    }
    else
    {
        InitializePitch(CurrentProfile);
    }

    UpdatePitchVisuals();
}

void APitchActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Natural deterioration over time (very slow)
    // SimulateInnings would be called at innings breaks instead
}

void APitchActor::InitializePitch(const FPitchProfileData& InProfile)
{
    CurrentProfile = InProfile;

    // Set default zone properties based on pitch type
    FPitchZoneProperties DefaultProps = GetDefaultZoneProperties();

    // Apply profile-specific modifications
    switch (CurrentProfile.PitchType)
    {
        case EPitchType::GreenTop:
            DefaultProps.Restitution = 0.65f;
            DefaultProps.Friction = 0.35f;
            DefaultProps.SeamDeviation = 0.15f;
            DefaultProps.SpinGrip = 0.2f;
            DefaultProps.WearLevel = 0.0f;
            break;
        case EPitchType::DryDusty:
            DefaultProps.Restitution = 0.5f;
            DefaultProps.Friction = 0.5f;
            DefaultProps.SeamDeviation = 0.05f;
            DefaultProps.SpinGrip = 0.6f;
            DefaultProps.WearLevel = 0.2f;
            DefaultProps.bHasCracks = true;
            DefaultProps.CrackSeverity = 0.3f;
            break;
        case EPitchType::FlatBatting:
            DefaultProps.Restitution = 0.55f;
            DefaultProps.Friction = 0.3f;
            DefaultProps.SeamDeviation = 0.03f;
            DefaultProps.SpinGrip = 0.15f;
            DefaultProps.WearLevel = 0.0f;
            break;
        case EPitchType::Cracked:
            DefaultProps.Restitution = 0.45f;
            DefaultProps.Friction = 0.45f;
            DefaultProps.SeamDeviation = 0.2f;
            DefaultProps.SpinGrip = 0.4f;
            DefaultProps.WearLevel = 0.4f;
            DefaultProps.bHasCracks = true;
            DefaultProps.CrackSeverity = 0.6f;
            break;
        case EPitchType::Synthetic:
            DefaultProps.Restitution = 0.7f;
            DefaultProps.Friction = 0.25f;
            DefaultProps.SeamDeviation = 0.01f;
            DefaultProps.SpinGrip = 0.1f;
            DefaultProps.WearLevel = 0.0f;
            break;
        case EPitchType::Standard:
        default:
            DefaultProps.Restitution = 0.6f;
            DefaultProps.Friction = 0.4f;
            DefaultProps.SeamDeviation = 0.1f;
            DefaultProps.SpinGrip = 0.3f;
            DefaultProps.WearLevel = 0.1f;
            break;
    }

    // Initialize all zones with default properties
    for (int32 i = 0; i < ZoneGrid.Num(); i++)
    {
        ZoneGrid[i] = DefaultProps;
    }

    // Override with profile-specific zone properties if provided
    if (CurrentProfile.ZoneProperties.Num() == ZoneGrid.Num())
    {
        ZoneGrid = CurrentProfile.ZoneProperties;
    }

    // Clear wear tracking
    BowlerFootmarks.Empty();
    BatterGuards.Empty();

    UpdatePitchVisuals();
}

void APitchActor::SetPitchProfile(const FPitchProfileData& InProfile)
{
    InitializePitch(InProfile);
}

void APitchActor::GetPitchPropertiesAtLocation(const FVector& Location, float& OutRestitution, float& OutFriction) const
{
    FPitchZoneProperties Props = GetZonePropertiesAtLocation(Location);
    OutRestitution = Props.Restitution;
    OutFriction = Props.Friction;
}

FPitchZoneProperties APitchActor::GetZonePropertiesAtLocation(const FVector& Location) const
{
    int32 ZoneIndex = GetZoneIndexAtLocation(Location);
    if (ZoneIndex >= 0 && ZoneIndex < ZoneGrid.Num())
    {
        return ZoneGrid[ZoneIndex];
    }

    return GetDefaultZoneProperties();
}

void APitchActor::ApplyWearAtLocation(const FVector& Location, float WearAmount, EWearSource Source)
{
    int32 ZoneIndex = GetZoneIndexAtLocation(Location);
    if (ZoneIndex < 0 || ZoneIndex >= ZoneGrid.Num())
    {
        return;
    }

    float OldWear = ZoneGrid[ZoneIndex].WearLevel;
    ApplyWearToZone(ZoneIndex, WearAmount);
    float NewWear = ZoneGrid[ZoneIndex].WearLevel;

    // Track footmarks and guards
    switch (Source)
    {
        case EWearSource::BowlerFootmark:
            BowlerFootmarks.Add(Location);
            break;
        case EWearSource::BatterGuard:
            BatterGuards.Add(Location);
            break;
        default:
            break;
    }

    // Broadcast event
    OnPitchWearUpdated.Broadcast(Location, OldWear, NewWear);

    // Update visuals
    UpdateZoneDecals();
    UpdateMaterialParameters();
}

void APitchActor::SimulateOver(int32 OverNumber, int32 BallsBowled, const TArray<FVector>& BounceLocations)
{
    // Apply wear from ball impacts
    for (const FVector& BounceLoc : BounceLocations)
    {
        ApplyWearAtLocation(BounceLoc, 0.005f, EWearSource::BallImpact);
    }

    // Bowler footmarks (more wear at bowling crease)
    FVector BowlingCreaseStart = GetActorLocation() + FVector(-PitchLength / 2.0f + 122.0f, 0.0f, 0.0f); // 1.22m from stumps
    FVector BowlingCreaseEnd = GetActorLocation() + FVector(PitchLength / 2.0f - 122.0f, 0.0f, 0.0f);

    // Add footmarks near bowling creases
    for (int32 i = 0; i < 2; i++) // Two bowlers per over (simplified)
    {
        FVector FootmarkLoc = (i == 0) ? BowlingCreaseStart : BowlingCreaseEnd;
        FootmarkLoc.Y += FMath::RandRange(-50.0f, 50.0f); // Some variation
        AddBowlerFootmark(FootmarkLoc, 0.02f);
    }
}

void APitchActor::SimulateInnings(int32 OversCompleted)
{
    // Gradual wear across the pitch
    float WearPerOver = 0.002f; // Base wear per over

    // Middle zones wear more (good length area)
    for (int32 i = 0; i < ZoneGrid.Num(); i++)
    {
        FVector ZoneCenter = GetZoneCenterLocation(i);
        float DistanceFromCenter = FMath::Abs(ZoneCenter.Y); // Distance from middle

        // More wear in middle zones
        float ZoneWearMultiplier = 1.0f - (DistanceFromCenter / (PitchWidth / 2.0f)) * 0.5f;

        // Length zones: middle overs (zones 5-9) wear most
        int32 LengthZone = i % 5; // 0-4 for 5 length divisions
        float LengthMultiplier = 1.0f;
        if (LengthZone >= 1 && LengthZone <= 3) // Middle three zones
        {
            LengthMultiplier = 1.5f;
        }

        float TotalWear = WearPerOver * OversCompleted * ZoneWearMultiplier * LengthMultiplier;
        ApplyWearToZone(i, TotalWear);
    }

    // Cracks develop over time
    if (CurrentProfile.PitchType == EPitchType::Cracked || CurrentProfile.PitchType == EPitchType::DryDusty)
    {
        for (int32 i = 0; i < ZoneGrid.Num(); i++)
        {
            if (ZoneGrid[i].WearLevel > 0.5f && !ZoneGrid[i].bHasCracks)
            {
                ZoneGrid[i].bHasCracks = true;
                ZoneGrid[i].CrackSeverity = (ZoneGrid[i].WearLevel - 0.5f) * 2.0f;
            }
            else if (ZoneGrid[i].bHasCracks)
            {
                ZoneGrid[i].CrackSeverity = FMath::Min(ZoneGrid[i].CrackSeverity + 0.05f, 1.0f);
            }
        }
    }

    // Uneven bounce increases
    CurrentProfile.Evenness = FMath::Max(CurrentProfile.Evenness - 0.02f * OversCompleted, 0.3f);

    UpdatePitchVisuals();
}

void APitchActor::AddBowlerFootmark(const FVector& Location, float Intensity)
{
    ApplyWearAtLocation(Location, Intensity, EWearSource::BowlerFootmark);
}

void APitchActor::AddBatterGuard(const FVector& Location, float Intensity)
{
    ApplyWearAtLocation(Location, Intensity, EWearSource::BatterGuard);
}

void APitchActor::UpdatePitchVisuals()
{
    UpdateZoneDecals();
    UpdateMaterialParameters();
}

void APitchActor::UpdateZoneDecals()
{
    // Remove old decals
    for (UDecalComponent* Decal : WearDecals)
    {
        if (Decal && Decal->IsRegistered())
        {
            Decal->DestroyComponent();
        }
    }
    WearDecals.Empty();

    // Create new decals for worn zones
    for (int32 i = 0; i < ZoneGrid.Num(); i++)
    {
        if (ZoneGrid[i].WearLevel > 0.1f)
        {
            FVector ZoneCenter = GetZoneCenterLocation(i);
            CreateWearDecal(ZoneCenter, ZoneGrid[i].WearLevel);
        }
    }
}

float APitchActor::GetAverageWear() const
{
    float TotalWear = 0.0f;
    for (const FPitchZoneProperties& Zone : ZoneGrid)
    {
        TotalWear += Zone.WearLevel;
    }
    return ZoneGrid.Num() > 0 ? TotalWear / ZoneGrid.Num() : 0.0f;
}

float APitchActor::GetWearAtZone(int32 ZoneIndex) const
{
    if (ZoneIndex >= 0 && ZoneIndex < ZoneGrid.Num())
    {
        return ZoneGrid[ZoneIndex].WearLevel;
    }
    return 0.0f;
}

void APitchActor::InitializeZoneGrid()
{
    ZoneGrid.SetNum(15); // 5 length x 3 width
    for (int32 i = 0; i < 15; i++)
    {
        ZoneGrid[i] = GetDefaultZoneProperties();
    }
}

int32 APitchActor::GetZoneIndexAtLocation(const FVector& Location) const
{
    // Convert world location to local pitch coordinates
    FVector LocalLoc = GetActorTransform().InverseTransformPosition(Location);

    // Check bounds
    if (FMath::Abs(LocalLoc.X) > PitchLength / 2.0f || FMath::Abs(LocalLoc.Y) > PitchWidth / 2.0f)
    {
        return -1;
    }

    // Calculate zone indices
    int32 LengthZone = FMath::FloorToInt((LocalLoc.X + PitchLength / 2.0f) / (PitchLength / 5.0f));
    int32 WidthZone = FMath::FloorToInt((LocalLoc.Y + PitchWidth / 2.0f) / (PitchWidth / 3.0f));

    LengthZone = FMath::Clamp(LengthZone, 0, 4);
    WidthZone = FMath::Clamp(WidthZone, 0, 2);

    return LengthZone * 3 + WidthZone;
}

FVector APitchActor::GetZoneCenterLocation(int32 ZoneIndex) const
{
    if (ZoneIndex < 0 || ZoneIndex >= 15)
    {
        return FVector::ZeroVector;
    }

    int32 LengthZone = ZoneIndex / 3;
    int32 WidthZone = ZoneIndex % 3;

    float ZoneLength = PitchLength / 5.0f;
    float ZoneWidth = PitchWidth / 3.0f;

    float X = -PitchLength / 2.0f + (LengthZone + 0.5f) * ZoneLength;
    float Y = -PitchWidth / 2.0f + (WidthZone + 0.5f) * ZoneWidth;

    return GetActorTransform().TransformPosition(FVector(X, Y, 1.0f));
}

void APitchActor::ApplyWearToZone(int32 ZoneIndex, float WearAmount)
{
    if (ZoneIndex < 0 || ZoneIndex >= ZoneGrid.Num())
    {
        return;
    }

    ZoneGrid[ZoneIndex].WearLevel = FMath::Clamp(ZoneGrid[ZoneIndex].WearLevel + WearAmount, 0.0f, 1.0f);

    // Update derived properties based on wear
    float Wear = ZoneGrid[ZoneIndex].WearLevel;

    // Restitution decreases with wear (softer surface)
    ZoneGrid[ZoneIndex].Restitution = FMath::Lerp(0.65f, 0.4f, Wear);

    // Friction increases with wear (rougher surface)
    ZoneGrid[ZoneIndex].Friction = FMath::Lerp(0.3f, 0.6f, Wear);

    // Seam deviation decreases (seam gets embedded)
    ZoneGrid[ZoneIndex].SeamDeviation = FMath::Lerp(0.15f, 0.02f, Wear);

    // Spin grip increases (rougher surface grips more)
    ZoneGrid[ZoneIndex].SpinGrip = FMath::Lerp(0.2f, 0.7f, Wear);
}

void APitchActor::UpdateZonePropertiesFromWear()
{
    for (int32 i = 0; i < ZoneGrid.Num(); i++)
    {
        ApplyWearToZone(i, 0.0f); // Recalculate derived properties
    }
}

void APitchActor::CreateWearDecal(const FVector& Location, float WearLevel)
{
    UDecalComponent* Decal = NewObject<UDecalComponent>(this);
    Decal->SetupAttachment(RootComponent);
    Decal->SetWorldLocation(Location);
    Decal->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
    Decal->DecalSize = FVector(50.0f, 100.0f, 100.0f); // X=depth, Y=width, Z=height

    // Create dynamic material for wear visualization
    // In practice, this would use a material parameter collection or MPC
    // Decal->SetDecalMaterial(WearDecalMaterial);

    Decal->RegisterComponent();
    WearDecals.Add(Decal);
}

FPitchZoneProperties APitchActor::GetDefaultZoneProperties() const
{
    FPitchZoneProperties Props;
    Props.Restitution = 0.6f;
    Props.Friction = 0.4f;
    Props.SeamDeviation = 0.1f;
    Props.SpinGrip = 0.3f;
    Props.WearLevel = 0.0f;
    Props.bHasCracks = false;
    Props.CrackSeverity = 0.0f;
    return Props;
}

void APitchActor::UpdateMaterialParameters()
{
    if (!PitchMesh || !PitchMesh->GetMaterial(0))
    {
        return;
    }

    UMaterialInstanceDynamic* DynamicMat = PitchMesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!DynamicMat)
    {
        return;
    }

    // Set global pitch parameters
    DynamicMat->SetScalarParameterValue("GrassLength", CurrentProfile.GrassLength * 100.0f);
    DynamicMat->SetScalarParameterValue("MoistureContent", CurrentProfile.MoistureContent);
    DynamicMat->SetScalarParameterValue("Hardness", CurrentProfile.Hardness);
    DynamicMat->SetScalarParameterValue("Evenness", CurrentProfile.Evenness);
    DynamicMat->SetScalarParameterValue("AverageWear", GetAverageWear());

    // Could pass zone-specific data via texture or parameter array
}