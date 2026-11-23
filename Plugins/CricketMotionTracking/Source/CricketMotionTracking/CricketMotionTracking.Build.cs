using System.IO;
using UnrealBuildTool;

public class CricketMotionTracking : ModuleRules
{
    public CricketMotionTracking(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] {
            Path.Combine(ModuleDirectory, "Networking"),
            Path.Combine(ModuleDirectory, "SensorFusion"),
            Path.Combine(ModuleDirectory, "Calibration"),
            Path.Combine(ModuleDirectory, "MotionProcessing"),
            Path.Combine(ModuleDirectory, "BatTracking")
        });

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "Networking",
            "Sockets",
            "Json",
            "JsonUtilities",
            "InputCore",
            "EnhancedInput",
            "HeadMountedDisplay",
            "AnimationCore",
            "AnimGraphRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
        });

        CppStandard = CppStandardVersion.Cpp20;

        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            OptimizeCode = CodeOptimization.InShippingBuildsOnly;
        }
    }
}