using UnrealBuildTool;

public class Vanthex : ModuleRules
{
	public Vanthex(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput", "Slate", "SlateCore", "GameplayTags", "DeveloperSettings", "PhysicsCore", "AudioMixer"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"MeshDescription",
				"StaticMeshDescription",
				"AssetRegistry", "AudioEditor", "UnrealEd"
			});
		}
	}
}
