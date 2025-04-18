#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Constants.h>
#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Animation/MeshSkinIdentifier.h>

#include <Common/Asset/Picker.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedSet.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Rendering
{
	struct VertexNormals;
}

namespace ngine::Animation
{
	struct MeshSkin;

	struct SkinnedMeshComponent : public Entity::StaticMeshComponent
	{
	public:
		static constexpr Guid TypeGuid = "e7a94bbc-9e75-47fe-8279-322b9f7a207f"_guid;

		using BaseType = StaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		struct Initializer : public StaticMeshComponent::Initializer
		{
			using BaseType = StaticMeshComponent::Initializer;
			using BaseType::BaseType;

			MeshSkinIdentifier m_meshSkinIdentifier;
		};

		SkinnedMeshComponent(Initializer&& initializer);
		SkinnedMeshComponent(const SkinnedMeshComponent& templateComponent, const Cloner& cloner);
		SkinnedMeshComponent(const Deserializer& deserializer);
		virtual ~SkinnedMeshComponent();

		void OnCreated();
		void OnDestroying();

		// Entity::Component3D
		virtual void OnAttachedToNewParent() override;
		virtual void OnBeforeDetachFromParent() override;

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;
		// ~Entity::Component3D

		void Update();
		void OnEnable();
		void OnDisable();
	protected:
		SkinnedMeshComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);

		friend struct Reflection::ReflectedType<ngine::Animation::SkinnedMeshComponent>;
		using SkinnedMeshPicker = Asset::Picker;
		void SetSkinnedMesh(const SkinnedMeshPicker asset);
		SkinnedMeshPicker GetSkinnedMesh() const;

		[[nodiscard]] bool CanEnableUpdate() const;
		void TryEnableUpdate();
		void LoadMeshSkin(Rendering::LogicalDevice& logicalDevice, const Threading::JobBatch& jobBatch);
		void UnloadMeshSkin();
	private:
		Rendering::StaticMeshIdentifier m_masterMeshIdentifier;
		MeshSkinIdentifier m_meshSkinIdentifier;
		MeshSkin* m_pMeshSkin = nullptr;
		Vector<Math::Matrix4x4f, uint16> m_skinningMatrices;

#define SUPPORT_SKINNED_MESH_STAGING_BUFFER (!RENDERER_WEBGPU)

		struct TargetVertices
		{
			Rendering::Index m_vertexCount = 0;
#if SUPPORT_SKINNED_MESH_STAGING_BUFFER
			Rendering::StagingBuffer m_stagingBuffer;
#else
			FixedSizeVector<ByteType> m_buffer;
#endif
			Rendering::BufferView m_targetBuffer;
			Rendering::VertexPosition* pPositions = nullptr;
			Rendering::VertexNormals* pNormals = nullptr;
		};

		UnorderedMap<Rendering::LogicalDeviceIdentifier, TargetVertices, Rendering::LogicalDeviceIdentifier::Hash> m_mappedTargetVertices;
		mutable Threading::SharedMutex m_mappedTargetVerticesMutex;
		Threading::Atomic<bool> m_isUpdateEnabled = false;
		Threading::Atomic<bool> m_canLoadSkin = true;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::SkinnedMeshComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::SkinnedMeshComponent>(
			Animation::SkinnedMeshComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Skinned Mesh"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Mesh Skin"),
				"skin",
				"{2A1844F1-81B8-4ACF-9BB1-BB3568A44B04}"_guid,
				MAKE_UNICODE_LITERAL("Mesh Skin"),
				&Animation::SkinnedMeshComponent::SetSkinnedMesh,
				&Animation::SkinnedMeshComponent::GetSkinnedMesh
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "abcf5a98-c556-41bb-b7ac-a5c6ea9dd85c"_asset, "9D186C9A-3D74-4E92-A5DD-A0F16AC9C138"_guid
			}}
		);
	};
}
