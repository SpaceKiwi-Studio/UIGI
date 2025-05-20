
#pragma once

#include <mutex>

namespace nvigi
{
	using Result = uint32_t;
	struct alignas(8) PluginID;

	using InferenceExecutionState = uint32_t;
	struct alignas(8) InferenceInstance;
}

namespace IGI
{
	enum ModelStatus {
		AVAILABLE_LOCALLY,
		AVAILABLE_CLOUD,
		AVAILABLE_DOWNLOADER, // Not yet supported
		AVAILABLE_DOWNLOADING, // Not yet supported
		AVAILABLE_MANUAL_DOWNLOAD,
		UNAVAILABLE
	};

	struct PluginModelInfo
	{
		std::string ModelName;
		std::string PluginName;
		std::string Caption; // plugin AND model
		std::string Guid;
		std::string ModelRoot;
		std::string Url;
		size_t Vram;
		nvigi::PluginID* FeatureID;
		ModelStatus ModelStatus;
	};

	struct PluginBackendChoices
	{
		nvigi::PluginID* NvidiaFeatureID;
		nvigi::PluginID* GpuFeatureID;
		nvigi::PluginID* CloudFeatureID;
		nvigi::PluginID* CpuFeatureID;
	};

	struct StageInfo
	{
		PluginModelInfo* Info{};
		nvigi::InferenceInstance* Inst{};
		// Model GUID to info maps (maps model GUIDs to a list of plugins that run it)
		TMap<FString, PluginModelInfo*> PluginModelsMap{};
		PluginBackendChoices Choices{};
		std::atomic<bool> bIsReady = false;
		std::atomic<bool> bIsRunning = false;
		std::mutex CallbackMutex;
		std::condition_variable CallbackCV;
		std::atomic<nvigi::InferenceExecutionState> CallbackState;
		size_t VramBudget{};
	};
}