using UnrealBuildTool;

public class CricketPhysics : ModuleRules
{
    public CricketPhysics(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "PhysicsCore",
            "Json",
            "JsonUtilities"
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