using UnrealBuildTool;

public class GalaSeedImport : ModuleRules
{
	public GalaSeedImport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
			{ "Core", "CoreUObject", "Engine", "Json", "JsonUtilities" });
	}
}
