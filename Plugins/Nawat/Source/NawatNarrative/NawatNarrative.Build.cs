using UnrealBuildTool;

public class NawatNarrative : ModuleRules
{
	public NawatNarrative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NawatCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});
	}
}
