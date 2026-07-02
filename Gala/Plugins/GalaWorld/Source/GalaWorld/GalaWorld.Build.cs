using UnrealBuildTool;

public class GalaWorld : ModuleRules
{
	public GalaWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
			{ "Core", "CoreUObject", "Engine", "GalaTelemetry" });
	}
}
