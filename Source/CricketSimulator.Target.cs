using UnrealBuildTool;
using System.Collections.Generic;

public class CricketSimulatorTarget : TargetRules
{
    public CricketSimulatorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("CricketSimulator");
    }
}