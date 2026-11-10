using UnrealBuildTool;

public class NawatPresentation : ModuleRules
{
	public NawatPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UMG",
			"EnhancedInput",
			"InputCore",
			"LevelSequence",
			"MovieScene",
			"DeveloperSettings",
			"NawatCore",
			"NawatNarrative"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
