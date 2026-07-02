using UnrealBuildTool;

public class GalaTelemetry : ModuleRules
{
	public GalaTelemetry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
