using UnrealBuildTool;

public class PhoneBatLink : ModuleRules
{
    public PhoneBatLink(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "Networking",
            "Sockets",
            "Json",
            "JsonUtilities",
            "InputCore",
            "EnhancedInput"
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