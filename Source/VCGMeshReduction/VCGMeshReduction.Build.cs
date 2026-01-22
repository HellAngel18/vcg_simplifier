using UnrealBuildTool;
using System.IO;

public class VCGMeshReduction : ModuleRules
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
    public VCGMeshReduction(ReadOnlyTargetRules Target) : base(Target)
    {
        bWarningsAsErrors = false;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicSystemIncludePaths.AddRange(
            new string[] {
                Path.Combine(ThirdPartyPath, "vcglib"),
                Path.Combine(ThirdPartyPath, "vcglib/eigenlib")
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",
                "NaniteUtilities",
                "MeshUtilitiesCommon",
                "MeshDescription",
                "StaticMeshDescription",
            }
        );

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );

        // VCG needs some defines
        PublicDefinitions.Add("VCG_USE_EIGEN");
    }
}
