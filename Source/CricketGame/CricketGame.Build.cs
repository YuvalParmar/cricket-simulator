using UnrealBuildTool;
using System.IO;

public class CricketGame : ModuleRules
{
    public CricketGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Add module root and subdirectories to public include paths
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "BatPhysics"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "BallPhysics"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Batting"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Bowling"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Fielding"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "GameModes"));

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "PhysicsCore",
            "Networking",
            "Sockets",
            "Json",
            "JsonUtilities",
            "CricketMotionTracking",
            "HeadMountedDisplay",
            "AnimationCore",
            "AnimGraphRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "UMG",
            "AIModule",
            "NavigationSystem"
        });

        // Enable C++20 features
        CppStandard = CppStandardVersion.Cpp20;

        // Optimize for shipping
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
    }
}