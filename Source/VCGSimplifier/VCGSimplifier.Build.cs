using UnrealBuildTool;
using System.IO;

public class VCGSimplifier : ModuleRules
{
    // 模块路径
    private string ModulePath
    {
        get
        {
            return ModuleDirectory;
        }
    }
    // 第三方库路径
    private string ThirdPartyPath
    {
        get
        {
            return Path.GetFullPath(Path.Combine(ModulePath, "../../Source/ThirdParty"));
        }
    }
    public VCGSimplifier(ReadOnlyTargetRules Target) : base(Target)
	{
		bWarningsAsErrors = false;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicSystemIncludePaths.AddRange(
			new string[] {
				Path.Combine(ThirdPartyPath, "vcglib"),
				Path.Combine(ThirdPartyPath, "vcglib/eigenlib")
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

        // VCG needs some defines
        PublicDefinitions.Add("VCG_USE_EIGEN");
	}
}
