using UnrealBuildTool;

public class CricketSimulator : ModuleRules
{
    public CricketSimulator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
            "CricketPhysics",
            "PhoneBatLink",
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

        // Platform-specific settings
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add("XInput.lib");
            PublicDefinitions.Add("WITH_XINPUT=1");
        }

        /*
        if (Target.Platform == UnrealTargetPlatform.PS5)
        {
            PublicDefinitions.Add("WITH_DUAL_SENSE=1");
        }
        */
    }
}