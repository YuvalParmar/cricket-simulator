using UnrealBuildTool;
using System.Collections.Generic;

public class CricketSimulatorEditorTarget : TargetRules
{
    public CricketSimulatorEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("CricketSimulator");
    }
}