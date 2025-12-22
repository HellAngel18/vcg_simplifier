using UnrealBuildTool;
using System.IO;

public class VCGSimplifier : ModuleRules
{
	public VCGSimplifier(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"MeshDescription",
				"StaticMeshDescription",
				"RenderCore",
                "EditorFramework",
                "UnrealEd",
                "RawMesh"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

        // Add VCGLib and Eigen paths
        string PluginExtrasPath = Path.Combine(ModuleDirectory, "../Extras");
        PublicIncludePaths.Add(Path.Combine(PluginExtrasPath, "vcglib"));
        PublicIncludePaths.Add(Path.Combine(PluginExtrasPath, "vcglib/eigenlib"));
        
        // VCG needs some defines
        PublicDefinitions.Add("VCG_USE_EIGEN");
	}
}
