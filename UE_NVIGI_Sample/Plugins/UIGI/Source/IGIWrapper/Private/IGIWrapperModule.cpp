/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A0000 // 0x0A0000 == Windows 10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000004 // Windows 10 Creators Update (15063)
#endif

// Plugin includes
#include "IGIWrapperModule.h"
#include "IGIStructs.h"

// Unreal Engine includes
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"

// Third-party NVIGI includes
#include <nvigi.h>
#include <nvigi_ai.h>
#include <nvigi_struct.h>

// Platform and build configuration-specific includes
#if PLATFORM_WINDOWS
	// Windows headers required for nvigi_types/nvigi_security
	#include "Windows/AllowWindowsPlatformTypes.h"
	#if !UE_BUILD_SHIPPING
		#include <nvigi_types.h>
	#else
		#include <nvigi_security.h>
	#endif
	#include "Windows/HideWindowsPlatformTypes.h"
#else
	#if !UE_BUILD_SHIPPING
		#include <nvigi_types.h>
	#endif
#endif


#define LOCTEXT_NAMESPACE "FIGIWrapperModule"

static std::atomic<bool> GShushIGILog{ false };

// RAII for IGI core framework loading/initialization and shutdown/unloading
class FIGICore
{
public:
	FIGICore(const FString& IGICoreBinaryDirectory)
		: bIsAPIStarted(false)
	{
		IGICoreDLLPath = FPaths::Combine(IGICoreBinaryDirectory, GIGI_CORE_DLL_NAME);

#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
		if (GIGI_USE_DLL_SIG) {
			UE_LOG(LogIGISDK, Log, TEXT("Checking NVIGI core DLL signature"));
			if (!nvigi::security::verifyEmbeddedSignature(*IGICoreDLLPath)) {
				UE_LOG(LogIGISDK, Log, TEXT("NVIGI core DLL is not signed - disable signature checking with -noSigCheck or use a signed NVIGI core DLL"));
				return;
			}
			
			IGICoreDLL = LoadLibraryW(*IGICoreDLLPath);
		}
#else
		IGICoreDLL = FPlatformProcess::GetDllHandle(*IGICoreDLLPath);
#endif
		
		if (!IGICoreDLL)
		{
			UE_LOG(LogIGISDK, Error, TEXT("Failed to load IGI core library... Aborting."));
			return;
		}

		UE_LOG(LogIGISDK, Log, TEXT("Loaded IGI core DLL from %s"), *IGICoreDLLPath);

		// Load the functions
		Ptr_nvigiInit = (PFun_nvigiInit*)FPlatformProcess::GetDllExport(IGICoreDLL, TEXT("nvigiInit"));
		Ptr_nvigiShutdown = (PFun_nvigiShutdown*)FPlatformProcess::GetDllExport(IGICoreDLL, TEXT("nvigiShutdown"));
		Ptr_nvigiLoadInterface = (PFun_nvigiLoadInterface*)FPlatformProcess::GetDllExport(IGICoreDLL, TEXT("nvigiLoadInterface"));
		Ptr_nvigiUnloadInterface = (PFun_nvigiUnloadInterface*)FPlatformProcess::GetDllExport(IGICoreDLL, TEXT("nvigiUnloadInterface"));

		if (!(Ptr_nvigiInit && Ptr_nvigiShutdown && Ptr_nvigiLoadInterface && Ptr_nvigiUnloadInterface))
		{
			UE_LOG(LogIGISDK, Fatal, TEXT("Failed to load IGI core library functions... Aborting."));
			bIsAPIStarted = false;
			return;
		}

		// Base plugins path
		const auto IGIPluginPathUTF8 = StringCast<UTF8CHAR>(*GIGI_CORE_BINARIES_PATH);

		// convert IGI binary paths to UTF8
		TArray<TArray<UTF8CHAR>> IGIBinaryDirectoriesUTF8;
		IGIBinaryDirectoriesUTF8.Emplace(IGIPluginPathUTF8.Get(), IGIPluginPathUTF8.Length() + 1);
		for (const FString& IGIBinaryDirectory : IGIBinaryDirectories)
		{
			const auto ConvertedUTF8 = StringCast<UTF8CHAR>(*IGIBinaryDirectory);
			IGIBinaryDirectoriesUTF8.Emplace(ConvertedUTF8.Get(), ConvertedUTF8.Length() + 1);
		}
		// convert IGI binary paths to CHAR*
		TArray<const char*> IGIPluginPaths;
		for (const TArray<UTF8CHAR>& IGIBinaryDirectoryUTF8 : IGIBinaryDirectoriesUTF8)
		{
			// IGI expects its UTF8-encoded strings as const char*
			IGIPluginPaths.Add(reinterpret_cast<const char*>(IGIBinaryDirectoryUTF8.GetData()));
		}

		// Logs Path
		const auto IGILogsPathUTF8 = TCHAR_TO_UTF8(*FPaths::ProjectLogDir());
		const char* IGILogsPathCStr = reinterpret_cast<const char*>(IGILogsPathUTF8);

		nvigi::Preferences Prefs{};
		Prefs.showConsole = GIGI_SHOW_CONSOLE;			// Set with Dev Settings.
		Prefs.logLevel = nvigi::LogLevel::eVerbose;		// Set with Dev Settings.
		Prefs.utf8PathsToPlugins = IGIPluginPaths.GetData();
		Prefs.numPathsToPlugins = IGIPluginPaths.Num();	// TODO Make a Path list for additional models.
		Prefs.utf8PathToLogsAndData = IGILogsPathCStr;
		Prefs.logMessageCallback = IGILogCallback;

		nvigi::PluginAndSystemInformation* Info{};

		if (NVIGI_FAILED(InitResult, Init(Prefs, Info, nvigi::kSDKVersion)))
		{
			UE_LOG(LogIGISDK, Error, TEXT("Failed to initialize NVIGI SDK... Aborting."));
		}

		UE_LOG(LogIGISDK, Log, TEXT("NVIGI SDK initialized successfully."));
	}
	
	virtual ~FIGICore()
	{
		Shutdown();
		if (IGICoreDLL)
		{
			FPlatformProcess::FreeDllHandle(IGICoreDLL);
			UE_LOG(LogIGISDK, Log, TEXT("Unloaded IGI core DLL from %s"), *IGICoreDLLPath);
		}
	}

	nvigi::Result LoadInterface(const StageInfo& Plugin, nvigi::InferenceInterface*& Interface)
	{
		const PluginModelInfo* PluginModelInfo = Plugin.Info;
		
		if (!(bIsAPIStarted && (Ptr_nvigiLoadInterface != nullptr)))
		{
			return nvigi::kResultInvalidState;
		}

		// NVIDIA: Work around an IGI bug by explicitly adding IGI's own binary path since IGI can't find it 🙄
		for (const FString& IGIBinaryDirectory : IGIBinaryDirectories)
		{
			FPlatformProcess::PushDllDirectory(*IGIBinaryDirectory);
		}
		nvigi::Result Result = nvigiGetInterfaceDynamic(*PluginModelInfo->FeatureID, &Interface, Ptr_nvigiLoadInterface, nullptr);
		for (const FString& IGIBinaryDirectory : ReverseIterate(IGIBinaryDirectories))
		{
			FPlatformProcess::PopDllDirectory(*IGIBinaryDirectory);
		}
		return Result;
	}

	nvigi::Result UnloadInterface(const nvigi::PluginID& Feature, nvigi::InferenceInterface*& Interface)
	{
		if (!(bIsAPIStarted && (Ptr_nvigiUnloadInterface != nullptr)))
		{
			return nvigi::kResultInvalidState;
		}
		return (*Ptr_nvigiUnloadInterface)(Feature, Interface);
	}

private:
	nvigi::Result Init(const nvigi::Preferences& Prefs, nvigi::PluginAndSystemInformation*& Info, const uint64_t& SDKVersion)
	{
		if (!Ptr_nvigiInit)
		{
			return nvigi::kResultInvalidState;
		}
		return (*Ptr_nvigiInit)(Prefs, &Info, SDKVersion);
	}

	nvigi::Result Shutdown()
	{
		if (!Ptr_nvigiShutdown)
		{
			return nvigi::kResultInvalidState;
		}
		bIsAPIStarted = false;
		return (*Ptr_nvigiShutdown)();
	}

private:
	FString IGICoreDLLPath{};
	uint8 bIsAPIStarted : 1 = false;
	TArray<FString> IGIBinaryDirectories;
#if PLATFORM_WINDOWS && UE_BUILD_SHIPPING
	HMODULE IGICoreDLL{};
#else
	void* IGICoreDLL{};
#endif
	

	PFun_nvigiInit* Ptr_nvigiInit{};
	PFun_nvigiShutdown* Ptr_nvigiShutdown{};
	PFun_nvigiLoadInterface* Ptr_nvigiLoadInterface{};
	PFun_nvigiUnloadInterface* Ptr_nvigiUnloadInterface{};

	nvigi::PluginAndSystemInformation* IGIRequirements = nullptr;
};

void FIGIWrapperModule::StartupModule()
{
	IGICore = MakePimpl<FIGICore, EPimplPtrMode::NoCopy>(GIGI_CORE_BINARIES_PATH);
}

void FIGIWrapperModule::ShutdownModule()
{
	IGICore.Reset();
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FIGIWrapperModule, IGIWrapper)