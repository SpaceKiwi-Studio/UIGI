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
#include "IGICorePlatformRHI.h"
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
		const auto IGIPluginPathUTF8 = StringCast<UTF8CHAR>(*GIGI_PLUGIN_BINARIES_PATH);
		const char* IGIPluginPathCStr = reinterpret_cast<const char*>(IGIPluginPathUTF8.Get());
		UE_LOG(LogIGISDK, Log, TEXT("Base plugin path: %s"), *FString(IGIPluginPathUTF8.Get()));

		// Logs Path
		const auto IGILogsPathUTF8 = TCHAR_TO_UTF8(*FPaths::ProjectLogDir());
		const char* IGILogsPathCStr = reinterpret_cast<const char*>(IGILogsPathUTF8);

		nvigi::Preferences Prefs{};
		Prefs.showConsole = GIGI_SHOW_CONSOLE;			// Set with Dev Settings.
		Prefs.logLevel = nvigi::LogLevel::eVerbose;		// Set with Dev Settings.
		Prefs.utf8PathsToPlugins = &IGIPluginPathCStr;
		Prefs.numPathsToPlugins = 1u;	// TODO Make a Path list for additional models.
		Prefs.utf8PathToLogsAndData = IGILogsPathCStr;
		Prefs.logMessageCallback = IGILogCallback;

		if (NVIGI_FAILED(InitResult, Init(Prefs, IGIRequirements, nvigi::kSDKVersion)))
		{
			UE_LOG(LogIGISDK, Error, TEXT("Failed to initialize NVIGI SDK... Aborting."));
		}

		InitPostDevice();

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

	nvigi::Result LoadInterface(const nvigi::PluginID& Plugin, nvigi::InferenceInterface*& Interface)
	{
		if (!(bIsAPIStarted && (Ptr_nvigiLoadInterface != nullptr)))
		{
			return nvigi::kResultInvalidState;
		}

		// NVIDIA: Work around an IGI bug by explicitly adding IGI's own binary path since IGI can't find it 🙄
		for (const FString& IGIBinaryDirectory : IGIBinaryDirectories)
		{
			FPlatformProcess::PushDllDirectory(*IGIBinaryDirectory);
		}
		const nvigi::Result Result = nvigiGetInterfaceDynamic(Plugin, &Interface, Ptr_nvigiLoadInterface, nullptr);
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

	nvigi::Result LoadCiGInterface()
	{
		UE_LOG(LogIGISDK, Log, TEXT("Loading CiG..."));
		nvigi::Result Result = nvigiGetInterfaceDynamic(nvigi::plugin::hwi::cuda::kId, &Cig, Ptr_nvigiLoadInterface, nullptr);
		UE_LOG(LogIGISDK, Log, TEXT("%hs"), Result == nvigi::kResultOk ? "CiG was loaded successfully. Using a shared CUDA context." : "CiG was not loaded.");

		return Result;
	}

	bool InitPostDevice()
	{
		if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
		{
			LoadCiGInterface();
			
			// D3D12
			ID3D12DynamicRHI* D3D12RHI = static_cast<ID3D12DynamicRHI*>(GDynamicRHI);

			if (!D3D12RHI)
			{
				UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve RHI instance from UE."));
				return false;
			}
			UE_LOG(LogIGISDK, Log, TEXT("RHI parameters: %s"), D3D12RHI->GetName());

			ID3D12CommandQueue* CmdQ = D3D12RHI->RHIGetCommandQueue();
			constexpr uint32 RHI_DEVICE_INDEX = 0u;
			ID3D12Device* D3D12Device = D3D12RHI->RHIGetDevice(RHI_DEVICE_INDEX);

			if (!CmdQ || !D3D12Device)
			{
				UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve D3D12 device and command queue from UE."));
				return false;
			}

			D3d12Params->device = D3D12Device;
			D3d12Params->queue = CmdQ;

			return true;
		}
		else if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Vulkan)
		{
			// Vulkan
			IVulkanDynamicRHI* VKRHI = static_cast<IVulkanDynamicRHI*>(GDynamicRHI);

			if (!VKRHI)
			{
				UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve RHI instance from UE."));
				return false;
			}
			UE_LOG(LogIGISDK, Log, TEXT("RHI parameters: %s"), VKRHI->GetName());

			VkQueue VkQ = VKRHI->RHIGetGraphicsVkQueue();
			VkDevice VkDevice = VKRHI->RHIGetVkDevice();
			VkPhysicalDevice VkPhysicalDevice = VKRHI->RHIGetVkPhysicalDevice();
			VkInstance VkInstance = VKRHI->RHIGetVkInstance();

			if (!VkQ || !VkDevice)
			{
				UE_LOG(LogIGISDK, Error, TEXT("Unable to retrieve VULKAN device and command queue from UE."));
				return false;
			}

			VulkanParams->physicalDevice = VkPhysicalDevice;
			VulkanParams->device = VkDevice;
			VulkanParams->queue = VkQ;
			VulkanParams->instance = VkInstance;
			
			return true;
		}
		else
		{
			UE_LOG(LogIGISDK, Error, TEXT("Unable to chain RHI Info."));
			return false;
		}
	}

	// Get the D3D12 parameters
	nvigi::D3D12Parameters GetD3D12Parameters() const
	{
		return *D3d12Params;
	}

	// Get the Vulkan parameters
	nvigi::VulkanParameters GetVulkanParameters() const
	{
		return *VulkanParams;
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

	nvigi::D3D12Parameters* D3d12Params{};
	nvigi::VulkanParameters* VulkanParams{};
	nvigi::IHWICuda* Cig{};

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

nvigi::Result FIGIWrapperModule::LoadIGIFeature(const nvigi::PluginID& PluginID,
	nvigi::InferenceInterface*& Interface)
{
	return GetCore().LoadInterface(PluginID, Interface);
}

nvigi::Result FIGIWrapperModule::UnloadIGIFeature(const nvigi::PluginID& PluginID,
	nvigi::InferenceInterface*& Interface)
{
	return GetCore().UnloadInterface(PluginID, Interface);
}

nvigi::Result FIGIWrapperModule::CheckPluginCompatibility(const nvigi::PluginID& PluginID, const FString& Name)
{
	return {};
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FIGIWrapperModule, IGIWrapper)