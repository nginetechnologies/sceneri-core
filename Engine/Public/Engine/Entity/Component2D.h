#pragma once

#include <Engine/Entity/HierarchyComponent.h>
#include <Engine/Entity/Data/Component2D.h>
#include <Engine/Entity/ApplicableData.h>
#include <Engine/Entity/ApplyAssetFlags.h>

#include <Common/Storage/ForwardDeclarations/Identifier.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/Transform2D.h>
#include <Common/EnumFlags.h>

namespace ngine
{
	struct Scene2D;
	struct FrameTime;
}

namespace ngine::Asset
{
	struct Reference;
}

namespace ngine::Entity
{
	namespace Data
	{
		struct WorldTransform2D;
		struct LocalTransform2D;
	}

	struct RootSceneComponent2D;

	struct Component2D;
	extern template struct HierarchyComponent<Component2D>;

	struct Component2D : public Entity::HierarchyComponent<Component2D>
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		using BaseType = HierarchyComponent;
		using RootType = Component2D;
		using ParentType = Component2D;

		struct Initializer : public HierarchyComponentBase::Initializer
		{
			using BaseType = HierarchyComponentBase::Initializer;

			Initializer(
				Component2D& parent,
				const Math::WorldTransform2D worldTransform = Math::Identity,
				const EnumFlags<Flags> flags = {},
				const Guid instanceGuid = Guid::Generate()
			)
				: BaseType(Entity::HierarchyComponentBase::Initializer{
						Reflection::TypeInitializer{}, parent, parent.GetSceneRegistry(), flags, instanceGuid
					})
				, m_worldTransform(worldTransform)
			{
			}
			Initializer(DynamicInitializer&& dynamicInitializer)
				: BaseType(Forward<DynamicInitializer>(dynamicInitializer))
			{
			}

			[[nodiscard]] PURE_STATICS Optional<Component2D*> GetParent() const
			{
				return static_cast<Component2D*>(BaseType::GetParent().Get());
			}

			Math::WorldTransform2D m_worldTransform{Math::Identity};
		};
		// This Initializer is the initializer needed to instantiate transformed components at runtime when type info is unknown
		using DynamicInitializer = BaseType::DynamicInitializer;

		struct Deserializer : public HierarchyComponentBase::Deserializer
		{
			using BaseType = HierarchyComponentBase::Deserializer;

			Deserializer(
				TypeDeserializer&& deserializer, Entity::SceneRegistry& sceneRegistry, Component2D& parent, const EnumFlags<Flags> flags = {}
			)
				: BaseType{Forward<TypeDeserializer>(deserializer), sceneRegistry, parent, flags}
			{
			}

			[[nodiscard]] Optional<Component2D*> GetParent() const
			{
				return static_cast<Component2D*>(BaseType::GetParent().Get());
			}
		};
		struct Cloner : public HierarchyComponentBase::Cloner
		{
			using BaseType = HierarchyComponentBase::Cloner;

			Cloner(
				Threading::JobBatch& jobBatch,
				Component2D& parent,
				SceneRegistry& sceneRegistry,
				const SceneRegistry& templateSceneRegistry,
				const Guid instanceGuid = Guid::Generate(),
				const Optional<ChildIndex> preferredChildIndex = Invalid
			)
				: BaseType{jobBatch, parent, sceneRegistry, templateSceneRegistry, instanceGuid, preferredChildIndex}
			{
			}

			[[nodiscard]] Optional<Component2D*> GetParent() const
			{
				return static_cast<Component2D*>(BaseType::GetParent().Get());
			}
		};

		Component2D(const Deserializer& deserializer);
		Component2D(const Component2D& templateComponent, const Cloner& cloner);
		Component2D(Initializer&& initializer);
		Component2D(
			const ComponentIdentifier identifier,
			HierarchyComponentBase& parent,
			RootSceneComponent2D& rootSceneComponent,
			SceneRegistry& sceneRegistry,
			const Math::WorldTransform2D worldTransform,
			const Guid instanceGuid,
			const EnumFlags<Flags> flags = {}
		);
		enum class RootSceneComponentType
		{
			RootSceneComponent
		};
		Component2D(
			RootSceneComponentType,
			const ComponentIdentifier identifier,
			const Optional<HierarchyComponentBase*> pParent,
			SceneRegistry& sceneRegistry,
			const Guid instanceGuid,
			const EnumFlags<Flags> flags = {}
		);

		bool Destroy(SceneRegistry& sceneRegistry);

		void OnAttachedToTree(const Optional<Component2D*> pParent);
		void OnDetachedFromTree(const Optional<Component2D*> pParent);

		[[nodiscard]] virtual PURE_STATICS Entity::SceneRegistry& GetSceneRegistry() const override final;

		[[nodiscard]] Component2D& GetParent() const
		{
			return static_cast<Component2D&>(BaseType::GetParent());
		}
		[[nodiscard]] Optional<Component2D*> GetParentSafe() const
		{
			return static_cast<Component2D*>(BaseType::GetParentSafe().Get());
		}
		[[nodiscard]] Component2D& GetChild(const ChildIndex index) const LIFETIME_BOUND
		{
			return static_cast<Component2D&>(BaseType::GetChild(index));
		}

		//! Gets the root scene component on the top level of the scene this component belongs to
		[[nodiscard]] RootSceneComponent2D& GetRootSceneComponent() const
		{
			return m_rootSceneComponent;
		}
		//! Gets the top level scene that this component directly or indirectly belongs to
		[[nodiscard]] PURE_STATICS Scene2D& GetRootScene() const;
		[[nodiscard]] FrameTime GetCurrentFrameTime() const;

		[[nodiscard]] Math::WorldTransform2D GetWorldTransform(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Transform2Df GetRelativeTransform(const SceneRegistry& sceneRegistry) const;
		void SetWorldTransform(const Math::WorldTransform2D transform, const SceneRegistry& sceneRegistry);
		void SetRelativeTransform(const Math::Transform2Df transform, const SceneRegistry& sceneRegistry);
		[[nodiscard]] Math::WorldCoordinate2D GetWorldLocation(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector2f GetRelativeLocation(const SceneRegistry& sceneRegistry) const;
		void SetWorldLocation(const Math::WorldCoordinate2D location, const SceneRegistry& sceneRegistry);
		void SetRelativeLocation(const Math::Vector2f location, const SceneRegistry& sceneRegistry);
		[[nodiscard]] Math::WorldScale2D GetWorldScale(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector2f GetRelativeScale(const SceneRegistry& sceneRegistry) const;
		void SetWorldScale(const Math::WorldScale2D scale, const SceneRegistry& sceneRegistry);
		void SetRelativeScale(const Math::Vector2f scale, const SceneRegistry& sceneRegistry);
		[[nodiscard]] Math::WorldRotation2D GetWorldRotation(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Rotation2Df GetRelativeRotation(const SceneRegistry& sceneRegistry) const;
		void SetWorldRotation(const Math::WorldRotation2D rotation, const SceneRegistry& sceneRegistry);
		void SetRelativeRotation(const Math::Rotation2Df rotation, const SceneRegistry& sceneRegistry);

		[[nodiscard]] Math::Vector2f GetWorldForwardDirection(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector2f GetWorldUpDirection(const SceneRegistry& sceneRegistry) const;

		void DeserializeCustomData(const Optional<Serialization::Reader> serializer);
		bool SerializeCustomData(Serialization::Writer serializer) const;

		[[nodiscard]] virtual bool
		CanApplyAtPoint(const ApplicableData&, const Math::WorldCoordinate2D, const EnumFlags<ApplyAssetFlags> applyFlags) const;
		[[nodiscard]] virtual bool
		ApplyAtPoint(const ApplicableData&, const Math::WorldCoordinate2D, const EnumFlags<ApplyAssetFlags> applyFlags);
	protected:
		friend struct Reflection::ReflectedType<Component2D>;

		void OnWorldTransformChangedInternal(
			const Math::WorldTransform2D worldTransform,
			ComponentTypeSceneData<Data::WorldTransform2D>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform2D>& localTransformSceneData
		);

		void SetWorldTransformFromProperty(Math::WorldTransform2D transform)
		{
			SetWorldTransform(transform, GetSceneRegistry());
		}
		[[nodiscard]] Math::WorldTransform2D GetWorldTransformFromProperty() const
		{
			return GetWorldTransform(GetSceneRegistry());
		}

		RootSceneComponent2D& m_rootSceneComponent;
	};

	namespace Data
	{
		[[nodiscard]] inline Entity::Component2D& Component2D::DynamicInitializer::GetParent() const
		{
			return static_cast<Entity::Component2D&>(BaseType::GetParent());
		}
		[[nodiscard]] inline Entity::Component2D& Component2D::Cloner::GetParent() const
		{
			return static_cast<Entity::Component2D&>(BaseType::GetParent());
		}
		[[nodiscard]] inline Entity::Component2D& Component2D::Deserializer::GetParent() const
		{
			return static_cast<Entity::Component2D&>(BaseType::GetParent());
		}
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Component2D>
	{
		inline static constexpr auto Type =
			Reflection::Reflect<Entity::Component2D>("dfe82463-a657-44ac-ba04-0a95618f8dd5"_guid, MAKE_UNICODE_LITERAL("2D Component"));
	};
}
