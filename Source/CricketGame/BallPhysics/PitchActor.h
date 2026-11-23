#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "CricketDataAssets.h"
#include "PitchActor.generated.h"

UENUM(BlueprintType)
enum class EWearSource : uint8
{
    BallImpact,
    BowlerFootmark,
    BatterGuard,
    NaturalDeterioration
};

USTRUCT(BlueprintType)
struct FPitchZoneProperties
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float Restitution = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float Friction = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float SeamDeviation = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float SpinGrip = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float WearLevel = 0.0f; // 0.0 = fresh, 1.0 = worn

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    bool bHasCracks = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch Properties")
    float CrackSeverity = 0.0f;
};

USTRUCT(BlueprintType)
struct FPitchProfileData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    EPitchType PitchType = EPitchType::Standard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FString ProfileName;

    // Default properties for each zone (5x3 grid: length x width)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    TArray<FPitchZoneProperties> ZoneProperties; // 15 zones

    // Overall pitch properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    float GrassLength = 0.008f; // meters

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    float MoistureContent = 0.3f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    float Hardness = 0.7f; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    float Evenness = 0.9f; // 0-1, lower = more variable bounce
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPitchWearUpdated, FVector, Location, float, OldWear, float, NewWear);

UCLASS(Blueprintable)
class CRICKETGAME_API APitchActor : public AActor
{
    GENERATED_BODY()

public:
    APitchActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Pitch initialization
    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void InitializePitch(const FPitchProfileData& InProfile);

    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void SetPitchProfile(const FPitchProfileData& InProfile);

    // Query pitch properties at location
    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void GetPitchPropertiesAtLocation(const FVector& Location, float& OutRestitution, float& OutFriction) const;

    UFUNCTION(BlueprintPure, Category = "Pitch")
    FPitchZoneProperties GetZonePropertiesAtLocation(const FVector& Location) const;

    // Wear simulation
    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void ApplyWearAtLocation(const FVector& Location, float WearAmount, EWearSource Source);

    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void SimulateOver(int32 OverNumber, int32 BallsBowled, const TArray<FVector>& BounceLocations);

    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void SimulateInnings(int32 OversCompleted);

    // Footmarks
    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void AddBowlerFootmark(const FVector& Location, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Pitch")
    void AddBatterGuard(const FVector& Location, float Intensity);

    // Visual updates
    UFUNCTION(BlueprintCallable, Category = "Visual")
    void UpdatePitchVisuals();

    UFUNCTION(BlueprintCallable, Category = "Visual")
    void UpdateZoneDecals();

    // Getters
    UFUNCTION(BlueprintPure, Category = "Pitch")
    const FPitchProfileData& GetPitchProfile() const { return CurrentProfile; }

    UFUNCTION(BlueprintPure, Category = "Pitch")
    float GetAverageWear() const;

    UFUNCTION(BlueprintPure, Category = "Pitch")
    float GetWearAtZone(int32 ZoneIndex) const;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPitchWearUpdated OnPitchWearUpdated;

protected:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> PitchMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> PitchBounds;

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FPitchProfileData CurrentProfile;

    // Pitch dimensions (standard: 20.12m x 3.05m)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions")
    float PitchLength = 2012.0f; // cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dimensions")
    float PitchWidth = 305.0f; // cm

    // Zone grid (5 length x 3 width = 15 zones)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zones")
    TArray<FPitchZoneProperties> ZoneGrid;

    // Footmarks and wear tracking
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wear")
    TArray<FVector> BowlerFootmarks;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wear")
    TArray<FVector> BatterGuards;

    // Decals for visual wear
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visual")
    TArray<TObjectPtr<class UDecalComponent>> WearDecals;

    // Internal
    void InitializeZoneGrid();
    int32 GetZoneIndexAtLocation(const FVector& Location) const;
    FVector GetZoneCenterLocation(int32 ZoneIndex) const;
    void ApplyWearToZone(int32 ZoneIndex, float WearAmount);
    void UpdateZonePropertiesFromWear();
    void CreateWearDecal(const FVector& Location, float WearLevel);
    FPitchZoneProperties GetDefaultZoneProperties() const;
    void UpdateMaterialParameters();
};