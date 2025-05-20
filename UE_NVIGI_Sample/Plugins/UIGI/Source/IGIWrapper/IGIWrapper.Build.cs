/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

using System.IO;
using UnrealBuildTool;

public class IGIWrapper : ModuleRules
{
	private static bool IsSupportedWindowsPlatform(ReadOnlyTargetRules Target)
	{
		return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows);
	}

	private static bool IsSupportedLinuxPlatform(ReadOnlyTargetRules Target)
	{
		return Target.Platform.IsInGroup(UnrealPlatformGroup.Linux);
	}

	private static bool IsSupportedPlatform(ReadOnlyTargetRules Target)
	{
		return IsSupportedWindowsPlatform(Target) || Target.Platform == UnrealTargetPlatform.Linux;
	}

    public IGIWrapper(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Projects",
                "HTTP",
                "RHI",
                
                // Plugins
                "nvigi"
            }
        );
        
        if (IsSupportedWindowsPlatform(Target))
        {
	        PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    // RHI
                    "D3D12RHI",
                    "VulkanRHI",
                }
            );

            // those come from the D3D12RHI and VulkanRHI
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
        } else if (IsSupportedLinuxPlatform(Target))
        {
	        PublicDependencyModuleNames.AddRange(
		        new string[]
		        {
			        // RHI
			        "VulkanRHI"
		        }
	        );

	        // those come from the VulkanRHI
	        AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
        }
    }
}