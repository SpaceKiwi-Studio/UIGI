/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

using EpicGames.Core;
using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

public sealed class nvigi : ModuleRules
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
	
	public nvigi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		Type = ModuleRules.ModuleType.External;

		if (!IsSupportedPlatform(Target))
			return;
		
#if UE_5_3_OR_LATER
			// UE 5.3 added the ability to override bWarningsAsErrors.
			// Treat all warnings as errors only on supported targets/engine versions >= 5.3.
			// Anyone else is in uncharted territory and should use their own judgment :-)
			bWarningsAsErrors = true;
#endif
		string IGIFolder = "";
		string IGITxtFile = Path.GetFullPath(Path.Combine(ModuleDirectory, "igi-folder-name.txt"));

		if (File.Exists(IGITxtFile))
		{
			using (StreamReader reader = new StreamReader(IGITxtFile))
			{
				string content = reader.ReadToEnd();
				IGIFolder = Path.Combine(ModuleDirectory, content, "nvigi_pack");
			}
		}
		
		bool bNvigiExists = Directory.Exists(IGIFolder);
			
		PublicDefinitions.Add(bNvigiExists ? "IGI_CORE_BINARY_EXISTS=1" : "IGI_CORE_BINARY_EXISTS=0");
		
		if (!bNvigiExists)
			return;

		// platform-specific binary locations
		string IGIModelPath = "";
		string IGICoreBinariesPath = "";
		string IGIPluginsBinariesPath = "";
		string LibRegex = "";
		
		// Headers
		if (Directory.Exists(Path.Combine(IGIFolder, "nvigi_core")) && Directory.Exists(Path.Combine(IGIFolder, "plugins")))
		{
			PublicIncludePaths.Add(Path.Combine(IGIFolder, "nvigi_core", "include"));
			PublicIncludePaths.Add(Path.Combine(IGIFolder, "plugins", "sdk", "include"));
		}
		
		// IGI models
		if (Directory.Exists(Path.Combine(IGIFolder, "nvigi.models")))
		{
			IGIModelPath = Path.Combine(IGIFolder, "nvigi.models");
			IEnumerable<string> IGIModels = Directory.EnumerateFiles(IGIModelPath, "*.*", SearchOption.AllDirectories);
			foreach (string IGIModel in IGIModels)
			{
				RuntimeDependencies.Add(IGIModel);
			}
		}
		
		if (IsSupportedWindowsPlatform(Target))
		{
			if (Target.Configuration == UnrealTargetConfiguration.Shipping)
			{
				System.Console.WriteLine("UE_BUILD_SHIPPING: YES");
				IGICoreBinariesPath = Path.Combine(IGIFolder, "nvigi_core", "bin", "Production_x64");
				IGIPluginsBinariesPath = Path.Combine(IGIFolder, "plugins", "sdk", "bin", "x64", "Production");
			}
			else
			{
				System.Console.WriteLine("UE_BUILD_SHIPPING: NO");
				IGICoreBinariesPath = Path.Combine(IGIFolder, "nvigi_core", "bin", "Debug_x64");
				IGIPluginsBinariesPath = Path.Combine(IGIFolder, "plugins", "sdk", "bin", "x64");
			}
			
			bool bNvigiCoreBinariyExists = File.Exists(Path.Combine(IGICoreBinariesPath, "nvigi.core.framework.dll"));
			
			PublicDefinitions.Add(bNvigiCoreBinariyExists ? "IGI_CORE_BINARY_EXISTS=1" : "IGI_CORE_BINARY_EXISTS=0");
			
			if (bNvigiCoreBinariyExists)
			{
				PublicDefinitions.Add($"IGI_CORE_BINARIES_PATH=TEXT(\"{IGICoreBinariesPath.Replace('\\', '/')}\")");
				PublicDefinitions.Add($"IGI_PLUGINS_BINARIES_PATH=TEXT(\"{IGIPluginsBinariesPath.Replace('\\', '/')}\")");
				PublicDefinitions.Add("IGI_CORE_BINARY_NAME=TEXT(\"nvigi.core.framework.dll\")");
				PublicDefinitions.Add($"IGI_CORE_MODELS_PATH=TEXT(\"{IGIModelPath.Replace('\\', '/')}\")");
			}
			
			LibRegex = "\\.dll$";
		}
		else
		{
			// unreachable code. if we ever reach this point we've messed up something with the supported platform logic
			throw new System.NotImplementedException();
		}
		
		//TODO: Include Linux in a future
		
		// IGI Core
		IEnumerable<string> IGICoreLibs = Directory.EnumerateFiles(IGICoreBinariesPath, "*.*", SearchOption.TopDirectoryOnly)
			.Where(f => Regex.IsMatch(f, LibRegex));
		
		foreach (string IGILib in IGICoreLibs)
		{
			RuntimeDependencies.Add(IGILib);
		}
		
		// IGI Plugin
		IEnumerable<string> IGIPluginsLibs = Directory.EnumerateFiles(IGIPluginsBinariesPath, "*.*", SearchOption.TopDirectoryOnly)
			.Where(f => Regex.IsMatch(f, LibRegex));
		
		foreach (string IGILib in IGIPluginsLibs)
		{
			RuntimeDependencies.Add(IGILib);
		}
	}
}
