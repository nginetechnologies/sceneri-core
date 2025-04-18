#pragma once

#include <Engine/Entity/HierarchyComponent.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentInstanceIdentifier.h>
#include <Engine/Entity/TransformChangeFlags.h>
#include <Engine/Entity/ApplicableData.h>
#include <Engine/Entity/ApplyAssetFlags.h>

#include <Common/Guid.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Math/Transform.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Primitives/WorldBoundingBox.h>
#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Function/Event.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Storage/Identifier.h>
#include <Common/Reflection/Type.h>
#include <Common/Platform/Pure.h>

namespace ngine
{
	struct Scene3D;
	struct FrameTime;

	namespace Asset
	{
		struct Asset;
		struct Reference;
	}

	namespace Physics
	{
		namespace Data
		{
			struct Body;
		}
	}

	namespace Audio
	{
		struct SoundListenerComponent;
	}

	namespace Threading
	{
		struct Job;
		struct JobBatch;
	}
}

namespace ngine::GameFramework
{
	struct ResetSystem;
}

namespace ngine::Entity
{
	struct RootSceneComponent;
	struct SceneComponent;
	struct RenderItemComponent;

	struct ComponentTypeSceneDataInterface;

	template<typename Type>
	struct ComponentType;

	namespace Data
	{
		struct Component;
		struct WorldTransform;
		struct LocalTransform3D;
		struct Flags;
	}

	struct Component3D;
	extern template struct HierarchyComponent<Component3D>;

	struct Component3D : public HierarchyComponent<Component3D>
	{
		using BaseType = HierarchyComponent<Component3D>;
		using RootType = Component3D;
		using ParentType = Component3D;
		using RootParentType = RootSceneComponent;
		using InstanceIdentifier = ComponentInstanceIdentifier;

		struct Initializer : public HierarchyComponentBase::Initializer
		{
			using BaseType = HierarchyComponentBase::Initializer;

			Initializer(
				Component3D& parent,
				const Math::LocalTransform localTransform = Math::Identity,
				const Math::BoundingBox localBoundingBox = {0.001_meters},
				const EnumFlags<Flags> flags = {},
				const Guid instanceGuid = Guid::Generate()

			)
				: BaseType(Entity::HierarchyComponentBase::Initializer{
						Reflection::TypeInitializer{}, parent, parent.GetSceneRegistry(), flags, instanceGuid
					})
				, m_localTransform(localTransform)
				, m_localBoundingBox(localBoundingBox)
			{
			}

			Initializer(
				Component3D& parent,
				const Math::WorldTransform worldTransform,
				const Math::BoundingBox localBoundingBox = {0.001_meters},
				const EnumFlags<Flags> flags = {},
				const Guid instanceGuid = Guid::Generate()
			)
				: BaseType(Entity::HierarchyComponentBase::Initializer{
						Reflection::TypeInitializer{}, parent, parent.GetSceneRegistry(), flags, instanceGuid
					})
				, m_localTransform(parent.GetWorldTransform().GetTransformRelativeToAsLocal(worldTransform))
				, m_localBoundingBox(localBoundingBox)
			{
			}

			Initializer(DynamicInitializer&& dynamicInitializer)
				: BaseType(Forward<DynamicInitializer>(dynamicInitializer))
			{
			}

			[[nodiscard]] Initializer operator|(const EnumFlags<Flags> flags) const
			{
				Initializer initializer = *this;
				initializer.m_flags |= flags;
				return initializer;
			}

			[[nodiscard]] Initializer operator|(const Math::BoundingBox boundingBox) const
			{
				Initializer initializer = *this;
				initializer.m_localBoundingBox = boundingBox;
				return initializer;
			}

			[[nodiscard]] PURE_STATICS Optional<Component3D*> GetParent() const
			{
				return static_cast<Component3D*>(BaseType::GetParent().Get());
			}
			[[nodiscard]] PURE_STATICS Scene3D& GetRootScene() const
			{
				return GetParent()->GetRootScene();
			}

			Math::LocalTransform m_localTransform = Math::Identity;
			Math::BoundingBox m_localBoundingBox{0.001_meters};
		};
		// This Initializer is the initializer needed to instantiate transformed components at runtime when type info is unknown
		using DynamicInitializer = BaseType::DynamicInitializer;

		struct Deserializer : public HierarchyComponentBase::Deserializer
		{
			using BaseType = HierarchyComponentBase::Deserializer;

			Deserializer(TypeDeserializer&& deserializer, SceneRegistry& sceneRegistry, Component3D& parent, const EnumFlags<Flags> flags = {})
				: BaseType{Forward<TypeDeserializer>(deserializer), sceneRegistry, parent, flags}
			{
			}

			[[nodiscard]] Deserializer operator|(const EnumFlags<Flags> flags) const
			{
				Deserializer deserializer = *this;
				deserializer.m_flags |= flags;
				return deserializer;
			}

			[[nodiscard]] Optional<Component3D*> GetParent() const
			{
				return static_cast<Component3D*>(BaseType::GetParent().Get());
			}
			[[nodiscard]] PURE_STATICS Scene3D& GetRootScene() const
			{
				return GetParent()->GetRootScene();
			}
		};
		struct DeserializerWithBounds : public Deserializer
		{
			using BaseType = Deserializer;

			DeserializerWithBounds(const BaseType& deserializer)
				: BaseType(deserializer)
			{
			}
			DeserializerWithBounds(
				TypeDeserializer&& deserializer,
				SceneRegistry& sceneRegistry,
				Component3D& parent,
				const EnumFlags<Flags> flags = {},
				const Math::BoundingBox localBoundingBox = {0.001_meters}
			)
				: BaseType{Forward<TypeDeserializer>(deserializer), sceneRegistry, parent, flags}
				, m_localBoundingBox(localBoundingBox)
			{
			}

			[[nodiscard]] DeserializerWithBounds operator|(const Math::BoundingBox boundingBox) const
			{
				DeserializerWithBounds deserializer = *this;
				deserializer.m_localBoundingBox = boundingBox;
				return deserializer;
			}

			Math::BoundingBox m_localBoundingBox{0.001_meters};
		};

		struct Cloner : public HierarchyComponentBase::Cloner
		{
			using BaseType = HierarchyComponentBase::Cloner;

			using BaseType::BaseType;
			Cloner(
				Threading::JobBatch& jobBatch,
				Component3D& parent,
				SceneRegistry& sceneRegistry,
				const SceneRegistry& templateSceneRegistry,
				const Guid instanceGuid = Guid::Generate(),
				const Optional<ChildIndex> preferredChildIndex = Invalid
			)
				: BaseType{jobBatch, parent, sceneRegistry, templateSceneRegistry, instanceGuid, preferredChildIndex}
			{
			}

			[[nodiscard]] Optional<Component3D*> GetParent() const
			{
				return static_cast<Component3D*>(BaseType::GetParent().Get());
			}
		};

		Component3D(const DeserializerWithBounds& deserializer);
		Component3D(const Component3D& templateComponent, const Cloner& cloner);
		Component3D(Initializer&& initializer);
		virtual ~Component3D() = default;

		[[nodiscard]] virtual PURE_STATICS Entity::SceneRegistry& GetSceneRegistry() const override final;

		void SetRelativeTransform(const Math::LocalTransform transform, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeTransform(const Math::LocalTransform transform);
		void SetRelativeRotation(const Math::Quaternionf rotation, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeRotation(const Math::Quaternionf rotation);
		void SetRelativeLocation(Math::Vector3f location, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeLocation(Math::Vector3f location);
		void SetRelativeLocationAndRotation(Math::Vector3f location, const Math::Quaternionf rotation, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeLocationAndRotation(Math::Vector3f location, const Math::Quaternionf rotation);
		void SetRelativeScale(Math::Vector3f location, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeScale(Math::Vector3f location);

		void SetWorldTransform(Math::WorldTransform transform, Entity::SceneRegistry& sceneRegistry);
		void SetWorldTransform(Math::WorldTransform transform);
		void SetWorldRotation(const Math::WorldQuaternion rotation, Entity::SceneRegistry& sceneRegistry);
		void SetWorldRotation(const Math::WorldQuaternion rotation);
		void SetWorldLocation(Math::WorldCoordinate location, Entity::SceneRegistry& sceneRegistry);
		void SetWorldLocation(Math::WorldCoordinate location);
		void
		SetWorldLocationAndRotation(Math::WorldCoordinate location, const Math::WorldQuaternion rotation, Entity::SceneRegistry& sceneRegistry);
		void SetWorldLocationAndRotation(Math::WorldCoordinate location, const Math::WorldQuaternion rotation);
		void SetWorldScale(Math::WorldScale location, Entity::SceneRegistry& sceneRegistry);
		void SetWorldScale(Math::WorldScale location);

		[[nodiscard]] Math::WorldTransform GetWorldTransform(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::WorldTransform GetWorldTransform() const;
		[[nodiscard]] Math::WorldCoordinate GetWorldLocation(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::WorldCoordinate GetWorldLocation() const;
		[[nodiscard]] Math::WorldTransform::QuaternionType GetWorldRotation(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::WorldTransform::QuaternionType GetWorldRotation() const;
		[[nodiscard]] Math::WorldScale GetWorldScale(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::WorldScale GetWorldScale() const;
		[[nodiscard]] Math::Vector3f GetWorldRightDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetWorldRightDirection() const;
		[[nodiscard]] Math::Vector3f GetWorldForwardDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetWorldForwardDirection() const;
		[[nodiscard]] Math::Vector3f GetWorldUpDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetWorldUpDirection() const;

		[[nodiscard]] Math::LocalTransform GetRelativeTransform(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::LocalTransform GetRelativeTransform() const;
		[[nodiscard]] Math::Vector3f GetRelativeLocation(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetRelativeLocation() const;
		[[nodiscard]] Math::LocalTransform::QuaternionType GetRelativeRotation(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::LocalTransform::QuaternionType GetRelativeRotation() const;
		[[nodiscard]] Math::Vector3f GetRelativeScale(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetRelativeScale() const;

		[[nodiscard]] Math::Vector3f GetRelativeRightDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetRelativeRightDirection() const;
		[[nodiscard]] Math::Vector3f GetRelativeForwardDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetRelativeForwardDirection() const;
		[[nodiscard]] Math::Vector3f GetRelativeUpDirection(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector3f GetRelativeUpDirection() const;

		[[nodiscard]] PURE_STATICS Math::BoundingBox GetBoundingBox(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Math::BoundingBox GetBoundingBox() const;
		[[nodiscard]] PURE_STATICS Math::BoundingBox GetRelativeBoundingBox(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Math::BoundingBox GetRelativeBoundingBox() const;
		[[nodiscard]] PURE_STATICS Math::WorldBoundingBox GetWorldBoundingBox(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Math::WorldBoundingBox GetWorldBoundingBox() const;
		// Returns bounding box encompassing all children, doesn't include own bounding box
		[[nodiscard]] PURE_STATICS Math::BoundingBox GetChildBoundingBox(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Math::BoundingBox GetChildBoundingBox() const;

		[[nodiscard]] Any GetAsset(const ArrayView<const Guid> assetTypeGuids);

		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&);
		[[nodiscard]] virtual bool
		CanApplyAtPoint(const ApplicableData& data, const Math::WorldCoordinate, const EnumFlags<ApplyAssetFlags> applyFlags) const;
		[[nodiscard]] virtual bool
		ApplyAtPoint(const ApplicableData& data, const Math::WorldCoordinate, const EnumFlags<ApplyAssetFlags> applyFlags);

		[[nodiscard]] Component3D& GetParent() const
		{
			Assert(!IsRootSceneComponent());
			return static_cast<Component3D&>(HierarchyComponentBase::GetParent());
		}
		[[nodiscard]] Optional<Component3D*> GetParentSafe() const
		{
			if (!IsRootSceneComponent())
			{
				return static_cast<Component3D*>(HierarchyComponentBase::GetParentSafe().Get());
			}
			else
			{
				return Invalid;
			}
		}

		template<typename DataComponentType, typename... Args>
		Optional<DataComponentType*> CreateDataComponent(Args&&... args);
		using DataComponentOwner::CreateDataComponent;
		template<typename DataComponentType>
		bool RemoveDataComponentOfType();
		template<typename DataComponentType>
		bool RemoveFirstDataComponentImplementingType();
		using DataComponentOwner::RemoveDataComponentOfType;
		using DataComponentOwner::RemoveFirstDataComponentImplementingType;
		[[nodiscard]] bool HasDataComponentOfType(const Guid typeGuid) const;
		template<typename DataComponentType>
		[[nodiscard]] bool HasDataComponentOfType() const;
		using DataComponentOwner::HasDataComponentOfType;
		[[nodiscard]] Optional<Data::Component*> FindDataComponentOfType(const Guid typeGuid) const;
		template<typename DataComponentType>
		[[nodiscard]] Optional<DataComponentType*> FindDataComponentOfType() const;
		using DataComponentOwner::FindDataComponentOfType;
		[[nodiscard]] Optional<Data::Component*> FindFirstDataComponentImplementingType(const Guid typeGuid) const;
		template<typename DataComponentType>
		[[nodiscard]] Optional<DataComponentType*> FindFirstDataComponentImplementingType() const;

		using DataComponentOwner::FindFirstDataComponentImplementingType;
		[[nodiscard]] bool HasAnyDataComponentsImplementingType(const Guid typeGuid) const;
		template<typename DataComponentType>
		[[nodiscard]] bool HasAnyDataComponentsImplementingType() const;
		using DataComponentOwner::HasAnyDataComponentsImplementingType;
		template<typename Callback>
		void IterateDataComponents(Callback&& callback) const;
		using DataComponentOwner::IterateDataComponents;

		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInChildrenRecursive() const;
		using BaseType::FindFirstDataComponentOfTypeInChildrenRecursive;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInSelfAndChildrenRecursive() const;
		using BaseType::FindFirstDataComponentOfTypeInSelfAndChildrenRecursive;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInChildrenRecursive() const;
		using BaseType::FindFirstDataComponentImplementingTypeInChildrenRecursive;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInSelfAndChildrenRecursive() const;
		using BaseType::FindFirstDataComponentImplementingTypeInSelfAndChildrenRecursive;

		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInParents() const;
		using BaseType::FindFirstDataComponentOfTypeInParents;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInParents() const;
		using BaseType::FindFirstDataComponentImplementingTypeInParents;

		template<typename ChildComponentType, typename Callback>
		void IterateDataComponentsOfTypeInChildrenRecursive(Callback&& callback) const;
		using BaseType::IterateDataComponentsOfTypeInChildrenRecursive;
		template<typename ChildComponentType, typename Callback>
		void IterateDataComponentsImplementingTypeInChildrenRecursive(Callback&& callback) const;
		using BaseType::IterateDataComponentsImplementingTypeInChildrenRecursive;

		template<typename DataComponentType, typename Callback>
		void IterateDataComponentsOfTypeInParentsRecursive(Callback&& callback) const;
		using BaseType::IterateDataComponentsOfTypeInParentsRecursive;
		template<typename DataComponentType, typename Callback>
		void IterateDataComponentsImplementingTypeInParentsRecursive(Callback&& callback) const;
		using BaseType::IterateDataComponentsImplementingTypeInParentsRecursive;

		//! Gets the root scene component on the top level of the scene this component belongs to
		[[nodiscard]] RootSceneComponent& GetRootSceneComponent() const
		{
			return m_rootSceneComponent;
		}

		//! Gets the top level scene that this component directly or indirectly belongs to
		[[nodiscard]] PURE_STATICS Scene3D& GetRootScene() const;
		//! Gets the scene component that this component directly belongs to
		//! For example, a player spawned as a scene will have the player scene component returned, and not the top level root scene.
		//! Note that if we are a scene component, we return the next parent and not ourselves.
		[[nodiscard]] PURE_STATICS Optional<SceneComponent*> GetParentSceneComponent() const;
		[[nodiscard]] FrameTime GetCurrentFrameTime() const;

		void DeserializeCustomData(const Optional<Serialization::Reader> serializer);
		bool SerializeCustomData(Serialization::Writer serializer) const;
		bool Destroy(SceneRegistry& sceneRegistry);
		void Destroy();

		void OnAttachedToTree(const Optional<Component3D*> pParent);
		void OnDetachedFromTree(const Optional<Component3D*> pParent);

		void OnParentWorldTransformChanged(
			const Math::WorldTransform parentWorldTransform,
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			const EnumFlags<TransformChangeFlags> flags = {}
		);

		virtual void OnWorldTransformChanged([[maybe_unused]] const EnumFlags<TransformChangeFlags> flags = {})
		{
		}
		Event<void(void*, EnumFlags<TransformChangeFlags> flags), 24> OnWorldTransformChangedEvent;
	protected:
		Component3D(
			Scene3D& scene,
			SceneRegistry& sceneRegistry,
			const Optional<HierarchyComponentBase*> pParent,
			RootSceneComponent& rootSceneComponent,
			const EnumFlags<ComponentFlags> flags,
			const Guid instanceGuid,
			const Math::BoundingBox localBoundingBox
		);

		virtual void OnAttachedToNewParent() override;

		void SetBoundingBox(Math::BoundingBox boundingBox, Entity::SceneRegistry& sceneRegistry);
		void SetBoundingBox(Math::BoundingBox boundingBox);
	private:
		Component3D(
			Scene3D& scene,
			Component3D& parent,
			const Guid guid,
			const Math::LocalTransform worldTransform,
			const EnumFlags<ComponentFlags> flags,
			const Math::BoundingBox localBoundingBox
		);
		Component3D(const DeserializerWithBounds& deserializer, const Optional<Serialization::Reader> transformSerializer);

		friend SceneComponent;

		void SetWorldTransformFromProperty(Math::WorldTransform transform)
		{
			SetWorldTransform(transform);
		}
		void SetLocalTransformFromProperty(Math::LocalTransform transform)
		{
			SetRelativeTransform(transform);
		}

		void SetWorldTransformInternal(
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			Math::WorldTransform transform,
			const EnumFlags<TransformChangeFlags> flags = {}
		);
		void SetWorldRotationInternal(
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			const Math::WorldQuaternion rotation
		);
		void SetWorldLocationInternal(
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			Math::WorldCoordinate location
		);
		void SetWorldLocationAndRotationInternal(
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			Math::WorldCoordinate location,
			const Math::WorldQuaternion rotation
		);
		void SetWorldScaleInternal(
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			Math::WorldScale location
		);

		void OnWorldTransformChangedInternal(
			const Math::WorldTransform worldTransform,
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			const EnumFlags<TransformChangeFlags> flags = {}
		);
	private:
		friend struct Reflection::ReflectedType<Entity::Component3D>;
		friend RenderItemComponent;
		friend RootSceneComponent;
		friend Scene3D;

		// These components need to subscribe to the OnTransformChange event.
		// TODO: Replace the event with scaling system for components in general.
		friend Physics::Data::Body;
		friend Audio::SoundListenerComponent;
		friend GameFramework::ResetSystem;

		template<typename T>
		friend struct ComponentTypeSceneData;
		template<typename T>
		friend struct ComponentType;

		RootSceneComponent& m_rootSceneComponent;
	};

	namespace Data
	{
		[[nodiscard]] inline Entity::Component3D& Component3D::DynamicInitializer::GetParent() const
		{
			return static_cast<Entity::Component3D&>(BaseType::GetParent());
		}
		[[nodiscard]] inline Entity::Component3D& Component3D::Cloner::GetParent() const
		{
			return static_cast<Entity::Component3D&>(BaseType::GetParent());
		}
		[[nodiscard]] inline const Entity::Component3D& Component3D::Cloner::GetTemplateParent() const
		{
			return static_cast<const Entity::Component3D&>(BaseType::GetTemplateParent());
		}
		[[nodiscard]] inline Entity::Component3D& Component3D::Deserializer::GetParent() const
		{
			return static_cast<Entity::Component3D&>(BaseType::GetParent());
		}
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Component3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Component3D>(
			"de8f8da2-d37f-41da-bc6e-580435fe605f"_guid,
			MAKE_UNICODE_LITERAL("3D Component"),
			TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Transform"),
				"Transform",
				"{A4046F07-CC84-4020-B39E-5D2A20CA17A1}"_guid,
				MAKE_UNICODE_LITERAL("Transform"),
				Reflection::PropertyFlags::Transient,
				&Entity::Component3D::SetLocalTransformFromProperty,
				(Math::LocalTransform(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeTransform
			)},
			Reflection::Functions{
				Function{
					"8615c575-4e57-4619-a2e4-56465cd77aa2"_guid,
					MAKE_UNICODE_LITERAL("Get Root Scene"),
					&Entity::Component3D::GetRootScene,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"3830ca94-f2a0-4eaf-b58b-7500250b5b68"_guid, MAKE_UNICODE_LITERAL("3D Scene")}
				},
				Function{
					"6bfcea1f-6c3c-4143-8161-4f75883ed750"_guid,
					MAKE_UNICODE_LITERAL("Get Root Component"),
					&Entity::Component3D::GetRootSceneComponent,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"5236f302-4a58-40b6-b589-f9e282f26200"_guid, MAKE_UNICODE_LITERAL("3D Root Component")}
				},
				Function{
					"755d1fb8-e645-4d45-8db9-dd96f7ef353f"_guid,
					MAKE_UNICODE_LITERAL("Get Parent"),
					&Entity::Component3D::GetParentSafe,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"372c3995-1b23-4476-b0f0-1dde32b78c75"_guid, MAKE_UNICODE_LITERAL("3D Component")}
				},
				Function{
					"ccae3495-ac96-425a-a1f0-f8358aec630d"_guid,
					MAKE_UNICODE_LITERAL("Get Owner Component"),
					&Entity::Component3D::GetParentSceneComponent,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"e0ecdcc1-442e-427d-90be-25679bac62d7"_guid, MAKE_UNICODE_LITERAL("3D Component")}
				},
				Function{
					"148b161c-051b-4b68-ba70-f37e0c9e11c7"_guid,
					MAKE_UNICODE_LITERAL("Remove"),
					(void(Entity::Component3D::*)()) & Entity::Component3D::Destroy,
					FunctionFlags::VisibleToUI,
					Argument{}
				},

				// World transform setters
				Function{
					"923df518-1d3e-43d1-814f-614c274c395d"_guid,
					MAKE_UNICODE_LITERAL("Set Transform"),
					(void(Entity::Component3D::*)(Math::WorldTransform)) & Entity::Component3D::SetWorldTransform,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"d90f72f0-f64a-4b70-b2ab-7b917ea35820"_guid, MAKE_UNICODE_LITERAL("Transform")}
				},
				Function{
					"1dd2ac19-8578-40a0-b735-7063ced03803"_guid,
					MAKE_UNICODE_LITERAL("Set Location"),
					(void(Entity::Component3D::*)(Math::WorldCoordinate)) & Entity::Component3D::SetWorldLocation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"86d90b24-f11f-4c50-a45c-9873218bfa41"_guid, MAKE_UNICODE_LITERAL("Location")}
				},
				Function{
					"c747faf4-51a3-462c-9958-733cdb7fa46c"_guid,
					MAKE_UNICODE_LITERAL("Set Rotation"),
					(void(Entity::Component3D::*)(Math::WorldQuaternion)) & Entity::Component3D::SetWorldRotation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"a573a7d2-a8a0-46d9-9c51-fa3d7d75dc56"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"dcd505f2-2efb-469a-a8a2-aede671ff274"_guid,
					MAKE_UNICODE_LITERAL("Set Location & Rotation"),
					(void(Entity::Component3D::*)(Math::WorldCoordinate, Math::WorldQuaternion)) & Entity::Component3D::SetWorldLocationAndRotation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"0fe0e3e1-d419-4546-9947-74241057b00f"_guid, MAKE_UNICODE_LITERAL("Location")},
					Argument{"f1b7995e-3785-4a78-a366-0e0f288aed3c"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"e6757c1c-c0d3-4b62-b3b4-7059f199c1f9"_guid,
					MAKE_UNICODE_LITERAL("Set Scale"),
					(void(Entity::Component3D::*)(Math::WorldScale)) & Entity::Component3D::SetWorldScale,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"03c9644d-0d56-4693-b41a-8cd36ab7dae8"_guid, MAKE_UNICODE_LITERAL("Scale")}
				},

				// World transform getters
				Function{
					"7fa8573e-0a20-4600-8e9f-cd9e15f2dc03"_guid,
					MAKE_UNICODE_LITERAL("Get Transform"),
					(Math::WorldTransform(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldTransform,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"8e929f77-0177-45f8-943a-bfd940b55a37"_guid, MAKE_UNICODE_LITERAL("Transform")}
				},
				Function{
					"32307872-1133-4199-84e1-1a398f90060f"_guid,
					MAKE_UNICODE_LITERAL("Get Location"),
					(Math::WorldCoordinate(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldLocation,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"92ec3d73-9fb5-4c15-9a01-443c140d43d6"_guid, MAKE_UNICODE_LITERAL("Location")}
				},
				Function{
					"f246fa4d-4177-4870-9fb6-a27674a734be"_guid,
					MAKE_UNICODE_LITERAL("Get Rotation"),
					(Math::WorldRotation(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldRotation,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"2e420dfb-9a19-4d48-9a5d-fb65df452a90"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"621f6d5a-1bbe-475c-b353-fc7bc64dc25f"_guid,
					MAKE_UNICODE_LITERAL("Get Scale"),
					(Math::WorldScale(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldScale,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"1c922e93-da0b-454f-aeb0-4b49ba2ecbb5"_guid, MAKE_UNICODE_LITERAL("Scale")}
				},
				Function{
					"02443b5b-340a-4482-af29-31b010804874"_guid,
					MAKE_UNICODE_LITERAL("Get Right Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldRightDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"fcca8ff1-49c9-411f-a6f9-5972b6cb45ef"_guid, MAKE_UNICODE_LITERAL("Direction")}
				},
				Function{
					"05eed3cd-b70c-4e92-9a96-09516713a55c"_guid,
					MAKE_UNICODE_LITERAL("Get Forward Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldForwardDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"67a5cc4a-0486-4b63-b7b0-1aac097bc74f"_guid, MAKE_UNICODE_LITERAL("Direction")}
				},
				Function{
					"65e06ec1-df21-4169-ab21-cb138cad3049"_guid,
					MAKE_UNICODE_LITERAL("Get Up Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetWorldUpDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"6cf554a7-f671-4918-af0d-a680790dc16c"_guid, MAKE_UNICODE_LITERAL("Direction")}
				},

				// Relative transform setters
				Function{
					"784305f2-94fc-4d14-80e7-abda674849c6"_guid,
					MAKE_UNICODE_LITERAL("Set Relative Transform"),
					(void(Entity::Component3D::*)(Math::LocalTransform)) & Entity::Component3D::SetRelativeTransform,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"c985db0d-4f6b-417f-8554-601612822968"_guid, MAKE_UNICODE_LITERAL("Transform")}
				},
				Function{
					"17dfc725-7510-449a-b3aa-13c746a29e57"_guid,
					MAKE_UNICODE_LITERAL("Set Relative Location"),
					(void(Entity::Component3D::*)(Math::Vector3f)) & Entity::Component3D::SetRelativeLocation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"68832849-3c2a-4449-8626-bb684ab0f210"_guid, MAKE_UNICODE_LITERAL("Location")}
				},
				Function{
					"531011f3-1140-4116-a678-a8ff1a245b5f"_guid,
					MAKE_UNICODE_LITERAL("Set Relative Rotation"),
					(void(Entity::Component3D::*)(Math::Quaternionf)) & Entity::Component3D::SetRelativeRotation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"3b25d708-a335-4965-8981-d8afc58ae50d"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"cb0dd19a-3001-4919-8aea-f8469c0eb136"_guid,
					MAKE_UNICODE_LITERAL("Set Relative Location & Rotation"),
					(void(Entity::Component3D::*)(Math::Vector3f, Math::Quaternionf)) & Entity::Component3D::SetRelativeLocationAndRotation,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"80b04d4f-dfc8-46ff-9207-d43561907c1c"_guid, MAKE_UNICODE_LITERAL("Location")},
					Argument{"1053ed6b-974d-48d8-a5b9-2f7e4f6e5adf"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"e3ce2144-3f72-4c45-91a1-68295ab4f8f8"_guid,
					MAKE_UNICODE_LITERAL("Set Relative Scale"),
					(void(Entity::Component3D::*)(Math::Vector3f)) & Entity::Component3D::SetRelativeScale,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"ff78c6ef-6fef-489b-8a74-82ad23949da8"_guid, MAKE_UNICODE_LITERAL("Scale")}
				},

				// Relative transform getters
				Function{
					"a1e9ac29-0144-4ae9-95cb-45c599b90dcc"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Transform"),
					(Math::LocalTransform(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeTransform,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"af5dac6b-13a5-4cdb-a6d7-fd9e6f06543b"_guid, MAKE_UNICODE_LITERAL("Transform")}
				},
				Function{
					"66538763-128a-4d54-b141-d3489572eeae"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Location"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeLocation,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"c4845a3c-ba11-4c84-b508-ece96b2bcb63"_guid, MAKE_UNICODE_LITERAL("Location")}
				},
				Function{
					"9f1bbcd8-2031-47fc-ab49-420383ba2096"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Rotation"),
					(Math::Quaternionf(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeRotation,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"87cf58c5-a767-4679-b317-cace61d4af69"_guid, MAKE_UNICODE_LITERAL("Rotation")}
				},
				Function{
					"ca90a822-4bda-445f-8767-d442e2d707f4"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Scale"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeScale,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"382343d1-86a7-448c-a8a8-18789b79625a"_guid, MAKE_UNICODE_LITERAL("Scale")}
				},
				Function{
					"a371c205-1420-4d45-ab0b-ea1bbc971659"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Right Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeRightDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"f95e7a6c-e39e-4e16-8426-ac6981e39a10"_guid, MAKE_UNICODE_LITERAL("Direction")}
				},
				Function{
					"73ac94ab-f3c7-45af-b78a-ebc8759fab12"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Forward Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeForwardDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"3dd90c7c-b700-4150-81d5-d224e1f33dcd"_guid, MAKE_UNICODE_LITERAL("Direction")}
				},
				Function{
					"c978d149-f285-49e3-87e6-c537eea5a72e"_guid,
					MAKE_UNICODE_LITERAL("Get Relative Up Direction"),
					(Math::Vector3f(Entity::Component3D::*)() const) & Entity::Component3D::GetRelativeUpDirection,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"30776cfd-d9ac-450e-af73-b20a713794e8"_guid, MAKE_UNICODE_LITERAL("Direction")}
				}
			},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "23387b29-bf51-9c63-b48b-88d7d20d4f3e"_asset, "9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid
			}}
		);
	};
}
