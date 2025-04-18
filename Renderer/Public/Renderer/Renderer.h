#pragma once

#include "Instance.h"
#include "Devices/PhysicalDevices.h"
#include "Devices/LogicalDevice.h"
#include "Assets/StaticMesh/MeshCache.h"
#include "Assets/Texture/TextureCache.h"
#include "Assets/Material/MaterialCache.h"
#include "Assets/Stage/StageCache.h"

#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Function/Event.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>

#include <Common/System/SystemType.h>

#include <Renderer/Window/WindowHandle.h>

namespace ngine
{
	struct Scene3D;
}

namespace ngine::Rendering
{
	struct Texture;
	struct Window;

	struct Renderer final
	{
	public:
		inline static constexpr System::Type SystemType = System::Type::Renderer;

		Renderer();
		~Renderer();

		[[nodiscard]] InstanceView GetInstance() const
		{
			return m_instance;
		}

		[[nodiscard]] PhysicalDevices& GetPhysicalDevices()
		{
			return m_physicalDevices;
		}
		[[nodiscard]] const PhysicalDevices& GetPhysicalDevices() const
		{
			return m_physicalDevices;
		}

		using LogicalDeviceView = IdentifierArrayView<const Optional<LogicalDevice*>, LogicalDeviceIdentifier>;
		[[nodiscard]] LogicalDeviceView GetLogicalDevices() const
		{
			const IdentifierArrayView<const UniquePtr<LogicalDevice>, LogicalDeviceIdentifier> logicalDevices =
				m_logicalDeviceIdentifiers.GetValidElementView(m_logicalDevices.GetView());
			return reinterpret_cast<const IdentifierArrayView<const Optional<LogicalDevice*>, LogicalDeviceIdentifier>&>(logicalDevices);
		}
		[[nodiscard]] Optional<LogicalDevice*> GetLogicalDevice(const LogicalDeviceIdentifier identifier) const
		{
			return m_logicalDevices[identifier];
		}

		Optional<LogicalDevice*> CreateLogicalDeviceFromPhysicalDevice(
			const PhysicalDevice& physicalDevice,
			const Optional<uint8> presentQueueIndex = {},
			const ArrayView<const ConstZeroTerminatedStringView, uint8> enabledExtensions = {}
		);
		void DestroyLogicalDevice(LogicalDevice& logicalDevice);

		[[nodiscard]] const MeshCache& GetMeshCache() const
		{
			return m_meshCache;
		}
		[[nodiscard]] MeshCache& GetMeshCache()
		{
			return m_meshCache;
		}
		[[nodiscard]] const TextureCache& GetTextureCache() const
		{
			return m_textureCache;
		}
		[[nodiscard]] TextureCache& GetTextureCache()
		{
			return m_textureCache;
		}
		[[nodiscard]] const MaterialCache& GetMaterialCache() const
		{
			return m_materialCache;
		}
		[[nodiscard]] MaterialCache& GetMaterialCache()
		{
			return m_materialCache;
		}
		[[nodiscard]] const StageCache& GetStageCache() const
		{
			return m_stageCache;
		}
		[[nodiscard]] StageCache& GetStageCache()
		{
			return m_stageCache;
		}

		Event<void(void* pEventIdentifier, LogicalDevice& logicalDevice), 24> OnLogicalDeviceCreated;
	protected:
	protected:
		friend LogicalDevice;
		friend Window;

		Instance m_instance;

		Rendering::PhysicalDevices m_physicalDevices;
		TSaltedIdentifierStorage<LogicalDeviceIdentifier> m_logicalDeviceIdentifiers;
		TIdentifierArray<UniquePtr<LogicalDevice>, LogicalDeviceIdentifier> m_logicalDevices;

		friend MeshCache;
		MeshCache m_meshCache;
		friend TextureCache;
		TextureCache m_textureCache;
		friend MaterialCache;
		MaterialCache m_materialCache;
		friend StageCache;
		StageCache m_stageCache;
	};
}
