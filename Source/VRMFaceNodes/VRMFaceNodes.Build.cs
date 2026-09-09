using UnrealBuildTool;
public class VRMFaceNodes : ModuleRules
{
    public VRMFaceNodes(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraphRuntime", "LiveLinkInterface", "LiveLinkAnimationCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "InputCore", "Json", "JsonUtilities" });
    }
}
