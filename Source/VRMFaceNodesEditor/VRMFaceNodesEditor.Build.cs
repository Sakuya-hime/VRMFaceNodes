using UnrealBuildTool;
public class VRMFaceNodesEditor : ModuleRules
{
    public VRMFaceNodesEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraph", "BlueprintGraph", "VRMFaceNodes" });
        PrivateDependencyModuleNames.AddRange(new[] { "LiveLinkGraphNode", "LiveLinkAnimationCore", "AnimGraphRuntime", "StaticMeshDescription" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Kismet", "KismetCompiler", "AssetRegistry", "AssetTools", "Projects", "Json", "JsonUtilities", "MeshDescription", "SkeletalMeshDescription", "Blutility", "IKRig", "IKRigEditor", "Slate", "SlateCore", "InputCore", "ToolMenus", "PropertyEditor", "ContentBrowser" });
    }
}
