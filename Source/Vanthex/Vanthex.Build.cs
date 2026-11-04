using UnrealBuildTool;

public class Vanthex : ModuleRules
{
	public Vanthex(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Keep commandlet-local helpers isolated in clean checkouts as well as
        // adaptive editor builds; unity amalgamation merges anonymous namespaces.
        bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "Voice",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput", "Slate", "SlateCore", "GameplayTags", "DeveloperSettings", "PhysicsCore", "AudioMixer", "AnimGraphRuntime", "AnimationCore"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"AssetRegistry", "AudioEditor", "UnrealEd", "Json"
			});
		}
	}
}
