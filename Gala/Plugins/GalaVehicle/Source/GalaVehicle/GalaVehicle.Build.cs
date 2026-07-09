using UnrealBuildTool;

public class GalaVehicle : ModuleRules
{
	public GalaVehicle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
			{ "Core", "CoreUObject", "Engine", "InputCore", "GalaTelemetry", "GalaSeedImport" });
	}
}
