using UnrealBuildTool;

public class UEBlueprintsToHtmlPlugin : ModuleRules
{
    public UEBlueprintsToHtmlPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ToolMenus"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate",
            "SlateCore",
            "EditorStyle",
            "UnrealEd",
            "BlueprintGraph"
        });
    }
}
