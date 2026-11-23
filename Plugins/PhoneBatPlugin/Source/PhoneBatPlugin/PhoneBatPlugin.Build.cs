using UnrealBuildTool;

public class PhoneBatPlugin : ModuleRules
{
    public PhoneBatPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsage.Default;

        bUseRTTI = true;
        bEnableExceptions = true;

        CppStandard = CppStandard.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "Networking",
                "Sockets",
                "Json",
                "GameplayAbilities",
                "PhysicsCore"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "GameThread",
                "RenderCore",
                "InputCore",
                "RHI",
                "HTTP",
                "AndroidHTTP",
                "AndroidMediaAssets",
                "MediaAssets"
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "OnlineSubsystems"
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.IOS)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Android"
                }
            );
        }
    }
}