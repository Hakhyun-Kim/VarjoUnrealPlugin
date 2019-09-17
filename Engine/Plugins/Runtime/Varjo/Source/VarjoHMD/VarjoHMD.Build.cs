// Copyright 6/4/2019 Varjo Technologies Oy. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VarjoHMD : ModuleRules
	{
		public VarjoHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.bIsEngineInstalled)
                PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;

            // Libs (TODO why both dll and lib loaded here?)
            string path0lib = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty", Target.Platform.ToString(), "VarjoLib.lib");
			PublicDelayLoadDLLs.Add("VarjoLib.dll");
			if(Target.bIsEngineInstalled)
				RuntimeDependencies.Add("$(ProjectDir)/Plugins/Varjo/ThirdParty/" + Target.Platform.ToString() + "/VarjoLib.dll");
			else
				RuntimeDependencies.Add("$(EngineDir)/Plugins/Runtime/Varjo/ThirdParty/" + Target.Platform.ToString() + "/VarjoLib.dll");

			PublicAdditionalLibraries.Add(path0lib);

			// Includes
			string pathInclude0 = System.IO.Path.Combine(ModuleDirectory, "../../ThirdParty", Target.Platform.ToString());
			PublicIncludePaths.AddRange(
				new string[] {
					pathInclude0
				}
				);

			// Dependencies
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);

			// Dependencies
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RHI",
					"RenderCore",
					"UtilityShaders",
					"Renderer",
					"InputCore",
					"HeadMountedDisplay",
					"D3D11RHI"
				}
				);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"D3D12RHI",
					});

				string engine_path = System.IO.Path.GetFullPath(Target.RelativeEnginePath);
				string srcrt_path = engine_path + "Source/Runtime/";

				PrivateIncludePaths.AddRange(
					new string[]
					{
							srcrt_path + "Windows/D3D11RHI/Private",
							srcrt_path + "Windows/D3D11RHI/Private/Windows",
							srcrt_path + "D3D12RHI/Private",
							srcrt_path + "D3D12RHI/Private/Windows",
					});

				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenVR");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
			}
		}
	}
}
