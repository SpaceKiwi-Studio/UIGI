/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IGICoreGlobals.h"
#include "IGICorePlatformRHI.h"
#include "IGICoreLog.h"

using namespace IGI;

namespace nvigi
{
    using Result = uint32_t;
    struct InferenceInterface;
    struct alignas(8) PluginID;
    struct alignas(8) IHWICuda;
    struct alignas(8) IHWICommon;
    struct alignas(8) D3D12Parameters;
    struct alignas(8) VulkanParameters;
    struct alignas(8) CudaParameters;
    struct alignas(8) CPUParameters;
    struct alignas(8) RESTParameters;
    struct alignas(8) RPCParameters;

    using InferenceExecutionState = uint32_t;
    struct alignas(8) InferenceInstance;
}

class FIGICore;
class FIGIFeatureRegistry;

class FIGIWrapperModule : public IModuleInterface
{
public:

    static FIGIWrapperModule& GetModule()
    {
        return FModuleManager::GetModuleChecked<FIGIWrapperModule>(FName("IGIWrapper"));
    }

    static FIGICore& GetCore()
    {
        return *GetModule().IGICore;
    }

    static FString GetModelDirectory() { return GIGI_BASE_MODELS_PATH; }

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Call this function before loading any IGI feature */
    void RegisterIGIFeature(nvigi::PluginID Feature, const TArray<FString>& AIMBinaryDirectories, const TArray<nvigi::PluginID>& IncompatibleFeatures);

    /** NVIDIA:  Will check whether an IGI feature is available, without logging any errors or warnings if it isn't available */
    bool IsAIMFeatureAvailable(nvigi::PluginID Feature);

    /** NVIDIA: You may use bShushIGILog to downgrade AIM log errors and warnings to normal log messages during loading */
    nvigi::Result LoadIGIFeature(const nvigi::PluginID& PluginID, const nvigi::InferenceInterface*& Interface, bool bShushAIMLog = false) const;
    nvigi::Result UnloadIGIFeature(const nvigi::PluginID& PluginID, const nvigi::InferenceInterface*& Interface) const;
    nvigi::Result CheckPluginCompatibility(const nvigi::PluginID& PluginID, const FString& Name);

    bool Initialize_preDeviceManager(const FDynamicRHI& API) const;

    /** Get the D3D12 parameters	*/
#ifdef IGI_USE_GRAPHICS_API_D3D12
    nvigi::D3D12Parameters		GetD3D12Parameters() const;
#endif
    /** Get the Vulkan parameters	*/
#ifdef IGI_USE_GRAPHICS_API_VULKAN
    nvigi::VulkanParameters		GetVulkanParameters() const;
#endif
    /** Get the CUDA parameters		*/
    nvigi::CudaParameters		GetCudaParameters() const;
    /** Get the CPU parameters		*/
    nvigi::CPUParameters		GetCPUParameters() const;
    /** Get the REST parameters		*/
    nvigi::RESTParameters		GetRESTParameters() const;
    /** Get the RPC parameters		*/
    nvigi::RPCParameters		GetRPCParameters() const;

private:
    TPimplPtr<FIGICore> IGICore;
};
