using UnrealBuildTool;

public class Gala : ModuleRules
{
	public Gala(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
			{ "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
