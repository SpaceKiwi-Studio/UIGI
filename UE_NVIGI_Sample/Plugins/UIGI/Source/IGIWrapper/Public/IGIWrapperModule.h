/*
 * SPDX-FileCopyrightText: Copyright (c) Space Kiwi Studio, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IGICoreGlobals.h"
#include "IGICoreLog.h"

using namespace IGI;

namespace nvigi
{
    using Result = uint32_t;
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
    struct alignas(8) InferenceInterface;
    struct alignas(8) InferenceInstance;
    struct alignas(8) PluginAndSystemInformation;
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

    /** NVIDIA: You may use bShushIGILog to downgrade AIM log errors and warnings to normal log messages during loading */
    nvigi::Result LoadIGIFeature(const nvigi::PluginID& PluginID, nvigi::InferenceInterface*& Interface);
    nvigi::Result UnloadIGIFeature(const nvigi::PluginID& PluginID, nvigi::InferenceInterface*& Interface);

private:
    TPimplPtr<FIGICore> IGICore;

    int m_adapter = -1;
    nvigi::PluginAndSystemInformation* m_pluginInfo{};
    
    nvigi::Result CheckPluginCompatibility(const nvigi::PluginID& PluginID, const FString& Name);
};
