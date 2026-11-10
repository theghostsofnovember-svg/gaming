using UnrealBuildTool;

public class NawatAI : ModuleRules
{
	public NawatAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"AIModule",
			"NavigationSystem",
			"NawatCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTasks"
		});
	}
}
