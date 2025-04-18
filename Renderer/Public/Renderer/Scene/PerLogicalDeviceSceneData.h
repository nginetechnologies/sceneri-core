#pragma once

namespace ngine::Rendering
{
	struct PerLogicalDeviceSceneData
	{
		PerLogicalDeviceSceneData() = default;
		PerLogicalDeviceSceneData(const PerLogicalDeviceSceneData&) = delete;
		PerLogicalDeviceSceneData& operator=(const PerLogicalDeviceSceneData&) = delete;
		PerLogicalDeviceSceneData(PerLogicalDeviceSceneData&&) = delete;
		PerLogicalDeviceSceneData& operator=(PerLogicalDeviceSceneData&&) = delete;
		~PerLogicalDeviceSceneData() = default;
	};
}
