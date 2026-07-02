using UnrealBuildTool;

public class GalaPaint : ModuleRules
{
	public GalaPaint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
			{ "Core", "CoreUObject", "Engine", "GalaTelemetry" });
	}
}
