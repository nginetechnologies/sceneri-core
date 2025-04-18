#pragma once

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Scene/SceneOctreeNode.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Time/FrameTime.h>
#include <Common/Memory/Allocators/Pool.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/UniquePtr.h>
#include <Renderer/Constants.h>

#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Asset
{
	struct Guid;
}

namespace ngine::Threading
{
	struct StageBase;
}

namespace ngine::IO
{
	struct LoadingThreadJob;
	struct StreamingSceneData;
}

namespace ngine::Entity
{
	struct DestroyEmptyOctreeNodesJob;

	namespace Data
	{
		struct Tags;
		struct OctreeNode;
	}

	struct RootSceneComponent final : public SceneComponent
	{
		using BaseType = Component3D;
		using ParentType = HierarchyComponentBase;
		using InstanceIdentifier = TIdentifier<uint32, 3, 4>;

		struct Initializer : public Reflection::TypeInitializer
		{
			Initializer(
				TypeInitializer&& initializer,
				const Optional<HierarchyComponentBase*> pParent,
				Scene3D& scene,
				SceneRegistry& sceneRegistry,
				const Math::Radius<Math::WorldCoordinateUnitType> radius
			)
				: TypeInitializer{Forward<TypeInitializer>(initializer)}
				, m_pParent(pParent)
				, m_scene(scene)
				, m_sceneRegistry(sceneRegistry)
				, m_radius(radius)
			{
			}

			[[nodiscard]] Optional<Component3D*> GetParent() const
			{
				return Invalid;
			}

			Optional<HierarchyComponentBase*> m_pParent;
			Scene3D& m_scene;
			SceneRegistry& m_sceneRegistry;
			Math::Radius<Math::WorldCoordinateUnitType> m_radius;
		};
		struct Deserializer : public Reflection::TypeDeserializer
		{
			Deserializer(const TypeDeserializer& deserializer, const Optional<HierarchyComponentBase*> pParent, Scene3D& scene)
				: TypeDeserializer{deserializer}
				, m_pParent(pParent)
				, m_scene(scene)
			{
			}

			[[nodiscard]] Scene3D& GetRootScene() const
			{
				return m_scene;
			}
			[[nodiscard]] Optional<Component3D*> GetParent() const
			{
				return Invalid;
			}

			Optional<HierarchyComponentBase*> m_pParent;
			Scene3D& m_scene;
		};

		// Root component creation
		RootSceneComponent(const Deserializer& deserializer);
		RootSceneComponent(Initializer&& initializer);
		RootSceneComponent(
			Scene3D& scene, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier, const Optional<HierarchyComponentBase*> pParent
		);
		virtual ~RootSceneComponent();

		void OnEnable();
		void OnDisable();
		bool Destroy(SceneRegistry& sceneRegistry);

		void SetInstanceGuid(const Guid instanceGuid);

		[[nodiscard]] const SceneOctreeNode& GetRootNode() const
		{
			return m_rootNode;
		}

		void OnComponentWorldLocationOrBoundsChanged(Entity::Component3D& component, Entity::SceneRegistry& sceneRegistry);
		void RemoveComponent(Entity::Component3D& component);

		[[nodiscard]] Math::Radius<Math::WorldCoordinateUnitType> GetRadius() const
		{
			return m_radius;
		}
		[[nodiscard]] Math::WorldBoundingBox GetDynamicOctreeWorldBoundingBox() const
		{
			return m_rootNode.GetChildBoundingBox() + GetWorldLocation();
		}

		[[nodiscard]] const Scene3D& GetScene() const
		{
			return m_scene;
		}
		[[nodiscard]] Scene3D& GetScene()
		{
			return m_scene;
		}

		[[nodiscard]] Threading::StageBase& GetOctreeCleanupJob();

		using Component3D::SerializeDataComponentsAndChildren;

		void SetRelativeTransform(const Math::LocalTransform transform) = delete;
		void SetRelativeRotation(const Math::Quaternionf rotation) = delete;
		void SetRelativeLocation(const Math::Vector3f location) = delete;
		void SetRelativeLocationAndRotation(const Math::Vector3f location, const Math::Quaternionf rotation) = delete;
		void SetRelativeScale(const Math::Vector3f location) = delete;

		void SetWorldTransform(const Math::WorldTransform transform);
		void SetWorldRotation(const Math::WorldQuaternion rotation);
		void SetWorldLocation(const Math::WorldCoordinate location);
		void SetWorldLocationAndRotation(const Math::WorldCoordinate location, const Math::WorldQuaternion rotation);
		void SetWorldScale(const Math::WorldScale location);
	private:
		friend Scene3D;
		friend Component3D;
		friend ComponentTypeSceneData<RootSceneComponent>;
		friend IO::LoadingThreadJob;

		Optional<SceneOctreeNode*> GetOrMakeIdealChildNode(
			SceneOctreeNode& node,
			const float renderItemRadiusSquared,
			const Math::WorldCoordinate itemLocation,
			const Math::WorldBoundingBox itemBounds
		);

		void AddComponent(Entity::Component3D& component);
	protected:
		ReferenceWrapper<Scene3D> m_scene;
		Math::Radiusf m_radius;

		friend struct Reflection::ReflectedType<Entity::RootSceneComponent>;
		SceneOctreeNode m_rootNode;
		inline static constexpr size PoolSize = sizeof(SceneOctreeNode) * 50000;
		using ChildNodePool = Memory::FixedPool<PoolSize, PoolSize / alignof(SceneOctreeNode), alignof(SceneOctreeNode)>;
		UniquePtr<ChildNodePool> m_pChildNodePool;
		Threading::Mutex m_poolMutex;

		friend DestroyEmptyOctreeNodesJob;
		UniqueRef<DestroyEmptyOctreeNodesJob> m_destroyEmptyOctreeNodesJob;

		ComponentTypeSceneData<Entity::Data::OctreeNode>& m_octreeNodeSceneData;
		ComponentTypeSceneData<Entity::Data::Tags>& m_tagComponentTypeSceneData;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RootSceneComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RootSceneComponent>(
			"2a2bce0a-6314-481b-a5a0-7d01e1c00b35"_guid,
			MAKE_UNICODE_LITERAL("Transform"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization,
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Radius"),
				"radius",
				"{E656F5C2-557C-4686-A008-B0F0CD72AC11}"_guid,
				MAKE_UNICODE_LITERAL("Octree"),
				Reflection::PropertyFlags{},
				&Entity::RootSceneComponent::m_radius
			}}
		);
	};
}
