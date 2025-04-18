#include "Scene/Scene.h"
#include "Entity/Component3D.h"
#include "Entity/Data/Tags.h"

#include <Engine/Entity/HierarchyComponent.inl>
#include "Engine/Entity/Scene/SceneComponent.h"
#include <Engine/Entity/Component3D.inl>
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/Serialization/ComponentValue.h"
#include "Engine/Entity/ComponentReference.h"
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Engine/Entity/Data/InstanceGuid.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Engine/Entity/Data/BoundingBox.h>
#include <Engine/Entity/Data/OctreeNode.h>
#include <Engine/Entity/Data/ParentComponent.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/MaterialAsset.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <Common/Serialization/Reader.h>
#include <Common/Math/Primitives/Transform/BoundingBox.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Serialization/ReferenceWrapper.h>
#include <Common/Serialization/Guid.h>
#include <Common/Math/Vector4.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>

#include <Common/Memory/OffsetOf.h>

namespace ngine::Entity
{
	Component3D::Component3D(const Component3D& templateComponent, const Cloner& cloner)
		: HierarchyComponent(templateComponent, cloner)
		, m_rootSceneComponent(cloner.GetParent()->GetRootSceneComponent())
	{
		Assert(GetFlags(cloner.GetSceneRegistry()).IsSet(Flags::Is3D));
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();

		{
			[[maybe_unused]] const Optional<Data::BoundingBox*> pBoundingBox =
				CreateDataComponent<Data::BoundingBox>(boundingBoxSceneData, templateComponent.GetBoundingBox());
			Assert(pBoundingBox.IsValid());
		}

		{
			const Math::LocalTransform localTransform = templateComponent.GetRelativeTransform();
			const Math::WorldTransform parentWorldTransform =
				worldTransformSceneData.GetComponentImplementationUnchecked(cloner.GetParent()->GetIdentifier());

			[[maybe_unused]] const Optional<Data::WorldTransform*> pWorldTransform =
				CreateDataComponent<Data::WorldTransform>(worldTransformSceneData, parentWorldTransform.Transform(localTransform));
			Assert(pWorldTransform.IsValid());

			[[maybe_unused]] const Optional<Data::LocalTransform3D*> pLocalTransform =
				CreateDataComponent<Data::LocalTransform3D>(localTransformSceneData, localTransform);
			Assert(pLocalTransform.IsValid());
		}
	}

	Component3D::Component3D(const DeserializerWithBounds& deserializer)
		: Component3D(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<Component3D>().ToString().GetView()))
	{
	}

	Component3D::Component3D(const DeserializerWithBounds& deserializer, const Optional<Serialization::Reader> transformSerializer)
		: Component3D(
				deserializer.GetParent()->GetRootScene(),
				*deserializer.GetParent(),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate()),
				transformSerializer.IsValid() ? transformSerializer->ReadInPlaceWithDefaultValue<Math::LocalTransform>(Math::Identity)
																			: Math::Identity,
				deserializer.GetFlags() | Flags::SaveToDisk |
					(transformSerializer.IsValid() ? transformSerializer.Get().ReadWithDefaultValue<Flags>("flags", Flags()) : Flags{}),
				deserializer.m_localBoundingBox
			)
	{
		Tag::Mask tagMask = Tag::Mask();
		deserializer.m_reader.Serialize("tags", tagMask, System::Get<Tag::Registry>());

		if (tagMask.AreAnySet())
		{
			Entity::ComponentTypeSceneData<Data::Tags>& tagsSceneData = deserializer.GetSceneRegistry().GetCachedSceneData<Data::Tags>();
			[[maybe_unused]] const Optional<Data::Tags*> pTags = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Data::Tags::BaseType::DynamicInitializer{*this, deserializer.GetSceneRegistry()}, tagMask}
			);
			Assert(pTags.IsValid());
		}
	}

	Component3D::Component3D(
		Scene&,
		SceneRegistry& sceneRegistry,
		const Optional<HierarchyComponentBase*> pParent,
		RootSceneComponent& octree,
		const EnumFlags<ComponentFlags> flags,
		const Guid instanceGuid,
		const Math::BoundingBox localBoundingBox
	)
		: HierarchyComponent(sceneRegistry.AcquireNewComponentIdentifier(), pParent, sceneRegistry, flags | Flags::Is3D, instanceGuid)
		, m_rootSceneComponent(octree)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();

		[[maybe_unused]] const Optional<Data::WorldTransform*> pWorldTransform =
			CreateDataComponent<Data::WorldTransform>(worldTransformSceneData, Math::Identity);
		Assert(pWorldTransform.IsValid());
		[[maybe_unused]] const Optional<Data::LocalTransform3D*> pLocalTransform =
			CreateDataComponent<Data::LocalTransform3D>(localTransformSceneData, Math::Identity);
		Assert(pLocalTransform.IsValid());

		[[maybe_unused]] const Optional<Data::BoundingBox*> pBoundingBox =
			CreateDataComponent<Data::BoundingBox>(boundingBoxSceneData, localBoundingBox);
		Assert(pBoundingBox.IsValid());
	}

	Component3D::Component3D(Initializer&& __restrict initializer)
		: Component3D(
				initializer.GetRootScene(),
				*initializer.GetParent(),
				initializer.GetInstanceGuid(),
				initializer.m_localTransform,
				initializer.GetFlags(),
				initializer.m_localBoundingBox
			)
	{
	}

	Component3D::Component3D(
		Scene& scene,
		Component3D& parent,
		const Guid instanceGuid,
		const Math::LocalTransform relativeToParentTransform,
		EnumFlags<Flags> flags,
		const Math::BoundingBox localBoundingBox
	)
		: HierarchyComponent(
				scene.GetEntitySceneRegistry().AcquireNewComponentIdentifier(),
				&parent,
				scene.GetEntitySceneRegistry(),
				flags | Flags::Is3D,
				instanceGuid
			)
		, m_rootSceneComponent(parent.GetRootSceneComponent())
	{
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();

		{
			const Math::WorldTransform parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parent.GetIdentifier());

			[[maybe_unused]] const Optional<Data::WorldTransform*> pWorldTransform =
				CreateDataComponent<Data::WorldTransform>(worldTransformSceneData, parentWorldTransform.Transform(relativeToParentTransform));
			Assert(pWorldTransform.IsValid());
			[[maybe_unused]] const Optional<Data::LocalTransform3D*> pLocalTransform =
				CreateDataComponent<Data::LocalTransform3D>(localTransformSceneData, relativeToParentTransform);
			Assert(pLocalTransform.IsValid());
		}

		[[maybe_unused]] const Optional<Data::BoundingBox*> pBoundingBox =
			CreateDataComponent<Data::BoundingBox>(boundingBoxSceneData, localBoundingBox);
		Assert(pBoundingBox.IsValid());
	}

	PURE_STATICS Entity::SceneRegistry& Component3D::GetSceneRegistry() const
	{
		return GetRootScene().GetEntitySceneRegistry();
	}

	void Component3D::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();

		AtomicEnumFlags<Flags>& flags = *FindDataComponentOfType<Data::Flags>(sceneRegistry.GetCachedSceneData<Data::Flags>());

		if (serializer.IsValid())
		{
			ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();

			const ComponentIdentifier identifier = GetIdentifier();
			Data::LocalTransform3D& __restrict relativeTransform = localTransformSceneData.GetComponentImplementationUnchecked(identifier);
			const Math::LocalTransform previousLocalTransform = relativeTransform;
			Math::LocalTransform newTransform = previousLocalTransform;
			if (serializer->HasSerializer("position") || serializer->HasSerializer("rotation") || serializer->HasSerializer("scale"))
			{
				serializer.Get().SerializeInPlace(newTransform);
			}
			if (!newTransform.IsEquivalentTo(previousLocalTransform))
			{
				if (flags.IsSet(ComponentFlags::IsRootScene))
					;
				if (flags.AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource))
				{
					relativeTransform = newTransform;

					ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
					ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

					Data::WorldTransform& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(identifier);

					const Entity::ComponentIdentifier parentIdentifier =
						Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
					const Math::WorldTransform parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

					worldTransform = parentWorldTransform.Transform(newTransform);
					OnWorldTransformChanged(EnumFlags<TransformChangeFlags>{});
				}
				else
				{
					SetRelativeTransform(newTransform, sceneRegistry);
				}
			}
		}

		BaseType::DeserializeCustomData(serializer);
	}

	bool Component3D::SerializeCustomData(Serialization::Writer serializer) const
	{
		return serializer.SerializeInPlace(GetRelativeTransform()) | BaseType::SerializeCustomData(serializer);
	}

#if PLATFORM_WEB
	// Workaround for bad math code generation
	_Pragma("clang optimize off");
#endif

	void Component3D::SetRelativeTransform(const Math::LocalTransform relativeTransform, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		localTransform = relativeTransform;

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;
		SetWorldTransformInternal(
			worldTransformSceneData,
			localTransformSceneData,
			flagsSceneData,
			parentWorldTransform.Transform(relativeTransform)
		);
	}

	void Component3D::SetRelativeTransform(const Math::LocalTransform relativeTransform)
	{
		SetRelativeTransform(relativeTransform, GetSceneRegistry());
	}

	void Component3D::SetRelativeRotation(const Math::Quaternionf rotation, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetRotation(rotation);
		localTransform = newLocalTransform;

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		SetWorldRotationInternal(
			worldTransformSceneData,
			localTransformSceneData,
			flagsSceneData,
			parentWorldTransform.TransformRotation(rotation)
		);
	}

	void Component3D::SetRelativeRotation(const Math::Quaternionf rotation)
	{
		SetRelativeRotation(rotation, GetSceneRegistry());
	}

	void Component3D::SetRelativeLocation(const Math::Vector3f location, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetLocation(location);
		localTransform = newLocalTransform;

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		SetWorldLocationInternal(
			worldTransformSceneData,
			localTransformSceneData,
			flagsSceneData,
			parentWorldTransform.TransformLocation(location)
		);
	}

	void Component3D::SetRelativeLocation(const Math::Vector3f location)
	{
		SetRelativeLocation(location, GetSceneRegistry());
	}

	void Component3D::SetRelativeLocationAndRotation(
		const Math::Vector3f location, const Math::Quaternionf rotation, Entity::SceneRegistry& sceneRegistry
	)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& __restrict localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetLocation(location);
		newLocalTransform.SetRotation(rotation);
		localTransform = newLocalTransform;

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		SetWorldLocationAndRotationInternal(
			worldTransformSceneData,
			localTransformSceneData,
			flagsSceneData,
			parentWorldTransform.TransformLocation(location),
			parentWorldTransform.TransformRotation(rotation)
		);
	}

	void Component3D::SetRelativeLocationAndRotation(const Math::Vector3f location, const Math::Quaternionf rotation)
	{
		SetRelativeLocationAndRotation(location, rotation, GetSceneRegistry());
	}

	void Component3D::SetRelativeScale(const Math::Vector3f scale, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetScale(scale);
		localTransform = newLocalTransform;

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		SetWorldScaleInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, scale * parentWorldTransform.GetScale());
	}

	void Component3D::SetRelativeScale(const Math::Vector3f scale)
	{
		SetRelativeScale(scale, GetSceneRegistry());
	}

	void Component3D::SetWorldTransform(const Math::WorldTransform transform, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		localTransform = parentWorldTransform.GetTransformRelativeToAsLocal(transform);
		SetWorldTransformInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, transform);
	}

	void Component3D::SetWorldTransform(const Math::WorldTransform transform)
	{
		SetWorldTransform(transform, GetSceneRegistry());
	}

	void Component3D::SetWorldRotation(const Math::WorldQuaternion rotation, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetRotation(parentWorldTransform.InverseTransformRotation(rotation));
		localTransform = newLocalTransform;
		SetWorldRotationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, rotation);
	}

	void Component3D::SetWorldRotation(const Math::WorldQuaternion rotation)
	{
		SetWorldRotation(rotation, GetSceneRegistry());
	}

	void Component3D::SetWorldLocation(const Math::WorldCoordinate location, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		Data::LocalTransform3D& localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetLocation(parentWorldTransform.InverseTransformLocation(location));
		localTransform = newLocalTransform;

		SetWorldLocationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, location);
	}

	void Component3D::SetWorldLocation(const Math::WorldCoordinate location)
	{
		SetWorldLocation(location, GetSceneRegistry());
	}

	void Component3D::SetWorldLocationAndRotation(
		const Math::WorldCoordinate location, const Math::WorldQuaternion rotation, Entity::SceneRegistry& sceneRegistry
	)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		Data::LocalTransform3D& __restrict localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetLocation(parentWorldTransform.InverseTransformLocation(location));
		newLocalTransform.SetRotation(parentWorldTransform.InverseTransformRotation(rotation));
		localTransform = newLocalTransform;

		SetWorldLocationAndRotationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, location, rotation);
	}

	void Component3D::SetWorldLocationAndRotation(const Math::WorldCoordinate location, const Math::WorldQuaternion rotation)
	{
		SetWorldLocationAndRotation(location, rotation, GetSceneRegistry());
	}

	void Component3D::SetWorldScale(const Math::WorldScale scale, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const Math::WorldTransform parentWorldTransform = flags.IsNotSet(ComponentFlags::IsRootScene)
		                                                    ? (Math::WorldTransform
		                                                      )worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier)
		                                                    : Math::Identity;

		Data::LocalTransform3D& __restrict localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Math::LocalTransform newLocalTransform = localTransform;
		newLocalTransform.SetScale(parentWorldTransform.InverseTransformScale(scale));
		localTransform = newLocalTransform;
		SetWorldScaleInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, scale);
	}

	void Component3D::SetWorldScale(const Math::WorldScale scale)
	{
		SetWorldScale(scale, GetSceneRegistry());
	}

	void Component3D::SetWorldTransformInternal(
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const Math::WorldTransform transform,
		const EnumFlags<TransformChangeFlags> flags
	)
	{
		Data::WorldTransform& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		worldTransform = transform;

		OnWorldTransformChangedInternal(transform, worldTransformSceneData, localTransformSceneData, flagsSceneData, flags);
	}

	void Component3D::SetWorldLocationInternal(
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const Math::WorldCoordinate location
	)
	{
		Data::WorldTransform& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::WorldTransform newWorldTransform = worldTransform;
		newWorldTransform.SetLocation(location);
		worldTransform = newWorldTransform;

		OnWorldTransformChangedInternal(newWorldTransform, worldTransformSceneData, localTransformSceneData, flagsSceneData);
	}

	void Component3D::SetWorldRotationInternal(
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const Math::WorldQuaternion rotation
	)
	{
		Data::WorldTransform& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::WorldTransform newWorldTransform = worldTransform;
		newWorldTransform.SetRotation(rotation);
		worldTransform = newWorldTransform;

		OnWorldTransformChangedInternal(newWorldTransform, worldTransformSceneData, localTransformSceneData, flagsSceneData);
	}

	void Component3D::SetWorldLocationAndRotationInternal(
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const Math::WorldCoordinate location,
		const Math::WorldQuaternion rotation
	)
	{
		Data::WorldTransform& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::WorldTransform newWorldTransform = worldTransform;
		newWorldTransform.SetLocation(location);
		newWorldTransform.SetRotation(rotation);
		worldTransform = newWorldTransform;

		OnWorldTransformChangedInternal(newWorldTransform, worldTransformSceneData, localTransformSceneData, flagsSceneData);
	}

	void Component3D::SetWorldScaleInternal(
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const Math::WorldScale scale
	)
	{
		Data::WorldTransform& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		Math::WorldTransform newWorldTransform = worldTransform;
		newWorldTransform.SetScale(scale);
		worldTransform = newWorldTransform;

		OnWorldTransformChangedInternal(newWorldTransform, worldTransformSceneData, localTransformSceneData, flagsSceneData);
	}

	void Component3D::OnParentWorldTransformChanged(
		const Math::WorldTransform parentWorldTransform,
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const EnumFlags<TransformChangeFlags> flags
	)
	{
		const Math::LocalTransform localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		SetWorldTransformInternal(
			worldTransformSceneData,
			localTransformSceneData,
			flagsSceneData,
			parentWorldTransform.Transform(localTransform),
			flags
		);
	}

	void Component3D::OnWorldTransformChangedInternal(
		const Math::WorldTransform worldTransform,
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData,
		ComponentTypeSceneData<Data::Flags>& flagsSceneData,
		const EnumFlags<TransformChangeFlags> transformChangeFlags
	)
	{
		const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		if (flags.IsNotSet(Flags::IsRootScene))
		{
			for (Component3D& child : GetChildren())
			{
				child.OnParentWorldTransformChanged(
					worldTransform,
					worldTransformSceneData,
					localTransformSceneData,
					flagsSceneData,
					transformChangeFlags
				);
			}
		}
		else
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			for (Component3D& child : GetChildren())
			{
				if (child.Implements<Entity::Component3D>(sceneRegistry))
				{
					child.OnParentWorldTransformChanged(
						worldTransform,
						worldTransformSceneData,
						localTransformSceneData,
						flagsSceneData,
						transformChangeFlags
					);
				}
			}
		}

		if (!flags.AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource | ComponentFlags::IsRootScene))
		{
			m_rootSceneComponent.OnComponentWorldLocationOrBoundsChanged(*this, worldTransformSceneData.GetSceneRegistry());
		}

		OnWorldTransformChanged(transformChangeFlags);
		OnWorldTransformChangedEvent(transformChangeFlags);
	}

	Math::WorldTransform Component3D::GetWorldTransform(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	Math::WorldTransform Component3D::GetWorldTransform() const
	{
		return GetWorldTransform(GetSceneRegistry());
	}

	Math::WorldCoordinate Component3D::GetWorldLocation(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetLocation();
	}

	Math::WorldCoordinate Component3D::GetWorldLocation() const
	{
		return GetWorldLocation(GetSceneRegistry());
	}

	Math::WorldTransform::QuaternionType Component3D::GetWorldRotation(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation();
	}

	Math::WorldTransform::QuaternionType Component3D::GetWorldRotation() const
	{
		return GetWorldRotation(GetSceneRegistry());
	}

	Math::WorldScale Component3D::GetWorldScale(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetScale();
	}

	Math::WorldScale Component3D::GetWorldScale() const
	{
		return GetWorldScale(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetWorldRightDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetRightColumn();
	}

	Math::Vector3f Component3D::GetWorldRightDirection() const
	{
		return GetWorldRightDirection(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetWorldForwardDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetForwardColumn();
	}

	Math::Vector3f Component3D::GetWorldForwardDirection() const
	{
		return GetWorldForwardDirection(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetWorldUpDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
		return worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetUpColumn();
	}

	Math::Vector3f Component3D::GetWorldUpDirection() const
	{
		return GetWorldUpDirection(GetSceneRegistry());
	}

	Math::LocalTransform Component3D::GetRelativeTransform(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	Math::LocalTransform Component3D::GetRelativeTransform() const
	{
		return GetRelativeTransform(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetRelativeLocation(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetLocation();
	}

	Math::Vector3f Component3D::GetRelativeLocation() const
	{
		return GetRelativeLocation(GetSceneRegistry());
	}

	Math::Quaternionf Component3D::GetRelativeRotation(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation();
	}

	Math::Quaternionf Component3D::GetRelativeRotation() const
	{
		return GetRelativeRotation(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetRelativeScale(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetScale();
	}

	Math::Vector3f Component3D::GetRelativeScale() const
	{
		return GetRelativeScale(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetRelativeRightDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetRightColumn();
	}

	Math::Vector3f Component3D::GetRelativeRightDirection() const
	{
		return GetRelativeRightDirection(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetRelativeForwardDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetForwardColumn();
	}

	Math::Vector3f Component3D::GetRelativeForwardDirection() const
	{
		return GetRelativeForwardDirection(GetSceneRegistry());
	}

	Math::Vector3f Component3D::GetRelativeUpDirection(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		return localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()).GetRotation().GetUpColumn();
	}

	Math::Vector3f Component3D::GetRelativeUpDirection() const
	{
		return GetRelativeUpDirection(GetSceneRegistry());
	}

	void Component3D::SetBoundingBox(const Math::BoundingBox newBoundingBox, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
		Data::BoundingBox& boundingBox = boundingBoxSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		boundingBox.Set(*this, sceneRegistry, newBoundingBox);
	}

	void Component3D::SetBoundingBox(const Math::BoundingBox newBoundingBox)
	{
		SetBoundingBox(newBoundingBox, GetSceneRegistry());
	}

	Math::BoundingBox Component3D::GetBoundingBox(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
		return boundingBoxSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	Math::BoundingBox Component3D::GetBoundingBox() const
	{
		return GetBoundingBox(GetSceneRegistry());
	}

	Math::BoundingBox Component3D::GetRelativeBoundingBox(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
		const Data::BoundingBox& boundingBoxComponent = boundingBoxSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Math::BoundingBox boundingBox = boundingBoxComponent;
		return Math::Transform(GetRelativeTransform(sceneRegistry), boundingBox);
	}

	Math::BoundingBox Component3D::GetRelativeBoundingBox() const
	{
		return GetRelativeBoundingBox(GetSceneRegistry());
	}

	Math::WorldBoundingBox Component3D::GetWorldBoundingBox(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
		const Data::BoundingBox& boundingBoxComponent = boundingBoxSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Math::BoundingBox boundingBox = boundingBoxComponent;
		return Math::Transform(GetWorldTransform(sceneRegistry), boundingBox);
	}

	Math::WorldBoundingBox Component3D::GetWorldBoundingBox() const
	{
		return GetWorldBoundingBox(GetSceneRegistry());
	}

	PURE_STATICS Math::BoundingBox Component3D::GetChildBoundingBox(const Entity::SceneRegistry& sceneRegistry) const
	{
		using ExpandBoundingBox = void (*)(
			const Entity::Component3D& component,
			const Entity::SceneRegistry& sceneRegistry,
			Math::BoundingBox& boundingBox,
			const Math::WorldTransform transform,
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
			ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData,
			ComponentTypeSceneData<Data::Flags>& flagsSceneData,
			ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData
		);
		static ExpandBoundingBox expandBoundingBox =
			[](
				const Entity::Component3D& component,
				const Entity::SceneRegistry& sceneRegistry,
				Math::BoundingBox& boundingBox,
				const Math::WorldTransform transform,
				ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData,
				ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData,
				ComponentTypeSceneData<Data::Flags>& flagsSceneData,
				ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData
			)
		{
			const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
			const EnumFlags<Flags> flags = flagsSceneData.GetComponentImplementationUnchecked(componentIdentifier);
			if (flags.AreAnySet(Flags::IsMeshScene) || component.HasDataComponentOfType(sceneRegistry, staticMeshIdentifierSceneData.GetIdentifier()))
			{
				const Math::WorldTransform componentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier
				);
				const Data::BoundingBox& boundingBoxComponent = boundingBoxSceneData.GetComponentImplementationUnchecked(componentIdentifier);

				const Math::BoundingBox componentBoundingBox = boundingBoxComponent;

				const Array<Math::Vector3f, 4> corners = componentBoundingBox.GetCorners();

				const Array<const Math::WorldCoordinate, 4> transformedCorners = {
					componentWorldTransform.TransformLocation(corners[0]),
					componentWorldTransform.TransformLocation(corners[1]),
					componentWorldTransform.TransformLocation(corners[2]),
					componentWorldTransform.TransformLocation(corners[3]),
				};

				for (const Math::WorldCoordinate transformedCorner : transformedCorners)
				{
					boundingBox.Expand(transform.InverseTransformLocation(transformedCorner));
				}
			}

			if (component.GetChildren().HasElements())
			{
				for (Entity::Component3D& child : component.GetChildren())
				{
					expandBoundingBox(
						child,
						sceneRegistry,
						boundingBox,
						transform,
						worldTransformSceneData,
						boundingBoxSceneData,
						flagsSceneData,
						staticMeshIdentifierSceneData
					);
				}
			}
		};

		Math::BoundingBox boundingBox{Math::Zero};
		if (GetChildren().HasElements())
		{
			ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();
			ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
			ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
			ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Data::RenderItem::StaticMeshIdentifier>();

			const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
			for (Entity::Component3D& child : GetChildren())
			{
				expandBoundingBox(
					child,
					sceneRegistry,
					boundingBox,
					worldTransform,
					worldTransformSceneData,
					boundingBoxSceneData,
					flagsSceneData,
					staticMeshIdentifierSceneData
				);
			}
		}
		return boundingBox;
	}

	PURE_STATICS Math::BoundingBox Component3D::GetChildBoundingBox() const
	{
		return GetChildBoundingBox(GetSceneRegistry());
	}

#if PLATFORM_WEB
	// Workaround for bad math code generation
	_Pragma("clang optimize on");
#endif

	bool Component3D::Destroy(SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		const Optional<Data::Flags*> pFlags = flagsSceneData.GetComponentImplementation(GetIdentifier());
		if (LIKELY(pFlags.IsValid()))
		{
			AtomicEnumFlags<Flags>& flags = *pFlags;

			if (flags.TrySetFlags(Flags::IsDestroying))
			{
				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDisabledWithChildren);
					if (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource))
					{
						DisableInternal(sceneRegistry);
					}
				}

				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDetachedFromTree);

					if (!previousFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource))
					{
						Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
						pSceneData->DetachInstanceFromTree(*this, GetParentSafe());
					}

					if (const Optional<Component3D*> pParent = GetParentSafe(); pParent.IsValid())
					{
						if (previousFlags.IsNotSet(ComponentFlags::IsDetachedFromTree))
						{
							OnBeforeDetachFromParent();
							pParent->RemoveChildAndClearParent(*this, sceneRegistry);
							pParent->OnChildDetached(*this);
						}
						Assert(!pParent->GetChildren().Contains(*this));
					}
				}

				while (HasChildren())
				{
					Component3D& child = GetChild(0);
					Assert(child.GetParentSafe() == this);
					child.Destroy(sceneRegistry);
				}

				GetRootScene().QueueComponentDestruction(*this);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	void Component3D::Destroy()
	{
		Destroy(GetSceneRegistry());
	}

	void Component3D::OnAttachedToNewParent()
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();

		HierarchyComponentBase& newParent = GetParent();

		ComponentTypeSceneData<Data::LocalTransform3D>& localTransformSceneData = sceneRegistry.GetCachedSceneData<Data::LocalTransform3D>();
		ComponentTypeSceneData<Data::WorldTransform>& worldTransformSceneData = sceneRegistry.GetCachedSceneData<Data::WorldTransform>();

		const Math::WorldTransform newParentWorldTransform =
			worldTransformSceneData.GetComponentImplementationUnchecked(newParent.GetIdentifier());
		const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		Data::LocalTransform3D& __restrict relativeTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		relativeTransform = newParentWorldTransform.GetTransformRelativeToAsLocal(worldTransform);
	}

	void Component3D::OnAttachedToTree([[maybe_unused]] const Optional<Component3D*> pParent)
	{
		GetRootSceneComponent().AddComponent(*this);
	}

	void Component3D::OnDetachedFromTree([[maybe_unused]] const Optional<Component3D*> pParent)
	{
		GetRootSceneComponent().RemoveComponent(*this);
	}

	PURE_STATICS Scene& Component3D::GetRootScene() const
	{
		return GetRootSceneComponent().GetScene();
	}

	FrameTime Component3D::GetCurrentFrameTime() const
	{
		return GetRootScene().GetCurrentFrameTime();
	}

	PURE_STATICS Optional<SceneComponent*> Component3D::GetParentSceneComponent() const
	{
		Optional<HierarchyComponentBase*> pParentSceneComponent = BaseType::GetParentSceneComponent();
		return static_cast<SceneComponent*>(pParentSceneComponent.Get());
	}

	Any Component3D::GetAsset(const ngine::ArrayView<const Guid> assetTypeGuids)
	{
		Array<Reflection::TypeDefinition, 1> typeDefinitions = {Reflection::TypeDefinition::Get<Asset::Reference>()};

		Any result;
		IterateAttachedItems(
			typeDefinitions.GetView(),
			[assetTypeGuids, &result](ConstAnyView view) -> Memory::CallbackResult
			{
				const Asset::Reference assetReference = view.GetExpected<Asset::Reference>();
				for (const Guid assetTypeGuid : assetTypeGuids)
				{
					if (assetReference.GetTypeGuid() == assetTypeGuid)
					{
						result = assetReference;
						return Memory::CallbackResult::Break;
					}
				}
				return Memory::CallbackResult::Continue;
			}
		);

		return result;
	}

	void Component3D::IterateAttachedItems(
		[[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes,
		const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			{
				ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::StaticMeshIdentifier>();
				if (const Optional<Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifier = staticMeshIdentifierSceneData.GetComponentImplementation(GetIdentifier()))
				{
					Asset::Picker mesh = pStaticMeshIdentifier->GetFromProperty(*this).m_asset;
					if (callback(mesh) == Memory::CallbackResult::Break)
					{
						return;
					}

					ComponentTypeSceneData<Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
						sceneRegistry.GetCachedSceneData<Data::RenderItem::MaterialInstanceIdentifier>();
					Data::RenderItem::MaterialInstanceIdentifier& materialInstanceIdentifier =
						materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
					Asset::Picker materialInstance = materialInstanceIdentifier.GetFromProperty(*this).m_asset;
					if (callback(materialInstance) == Memory::CallbackResult::Break)
					{
						return;
					}

					// Indicate texture
					// TODO: This should figure out what asset is rendered at what location
					// Will require a more advanced setup with rendering to a render target of identifiers
					Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
					const Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
					const Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

					if (const Optional<Rendering::RuntimeMaterialInstance*> pInstance = materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier))
					{
						const Rendering::RuntimeMaterial& runtimeMaterial = *materialCache.GetAssetData(pInstance->GetMaterialIdentifier()).m_pMaterial;
						const Rendering::MaterialAsset& materialAsset = *runtimeMaterial.GetAsset();
						const ArrayView<const Rendering::MaterialAsset::DescriptorBinding> descriptorBindings = materialAsset.GetDescriptorBindings();
						const ArrayView<const Rendering::RuntimeDescriptorContent> descriptorContents = pInstance->GetDescriptorContents();

						const Rendering::TextureCache& textureCache = renderer.GetTextureCache();

						for (uint32 descriptorIndex = 0; descriptorIndex < descriptorBindings.GetSize(); ++descriptorIndex)
						{
							switch (descriptorBindings[descriptorIndex].m_type)
							{
								case Rendering::DescriptorContentType::Texture:
								{
									const Asset::Reference textureAsset = {
										textureCache.GetAssetGuid(descriptorContents[descriptorIndex].m_textureData.m_textureIdentifier),
										TextureAssetType::AssetFormat.assetTypeGuid
									};
									if (callback(textureAsset) == Memory::CallbackResult::Break)
									{
										return;
									}
								}
								break;
								default:
									break;
							}
						}
					}
				}
			}
		}
	}

	bool Component3D::CanApplyAtPoint(
		const ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<ApplyAssetFlags> applyFlags
	) const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

			ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Data::RenderItem::StaticMeshIdentifier>();
			if (const Optional<Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifier = staticMeshIdentifierSceneData.GetComponentImplementation(GetIdentifier()))
			{
				if (applyFlags.IsSet(Entity::ApplyAssetFlags::Deep))
				{
					static constexpr Array<const Guid, 2> compatibleAssetTypes = {
						MaterialInstanceAssetType::AssetFormat.assetTypeGuid,
						MeshPartAssetType::AssetFormat.assetTypeGuid
					};
					return compatibleAssetTypes.GetView().Contains(pAssetReference->GetTypeGuid());
				}
				else
				{
					static constexpr Array<const Guid, 1> compatibleAssetTypes = {MaterialInstanceAssetType::AssetFormat.assetTypeGuid};
					return compatibleAssetTypes.GetView().Contains(pAssetReference->GetTypeGuid());
				}
			}
		}
		return false;
	}

	bool
	Component3D::ApplyAtPoint(const ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<ApplyAssetFlags> applyFlags)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

			ComponentTypeSceneData<Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Data::RenderItem::StaticMeshIdentifier>();
			if (const Optional<Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifier = staticMeshIdentifierSceneData.GetComponentImplementation(GetIdentifier()))
			{
				if (pAssetReference->GetTypeGuid() == MaterialInstanceAssetType::AssetFormat.assetTypeGuid)
				{
					ComponentTypeSceneData<Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
						sceneRegistry.GetCachedSceneData<Data::RenderItem::MaterialInstanceIdentifier>();
					Data::RenderItem::MaterialInstanceIdentifier& materialInstanceIdentifier =
						materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());

					materialInstanceIdentifier.SetFromProperty(*this, *pAssetReference);
					return true;
				}
				else if (applyFlags.IsSet(Entity::ApplyAssetFlags::Deep))
				{
					if (pAssetReference->GetTypeGuid() == MeshPartAssetType::AssetFormat.assetTypeGuid)
					{
						pStaticMeshIdentifier->SetFromProperty(*this, *pAssetReference);
						return true;
					}
				}
				// TODO: Detect a texture asset and make a new material instance copy that assigns the texture to the slot at the coordinate?
				// At the same time would need to make the coordinate optional
			}
		}
		return false;
	}

	Optional<Data::Component*> Component3D::FindDataComponentOfType(const Guid typeGuid) const
	{
		return DataComponentOwner::FindDataComponentOfType(GetSceneRegistry(), typeGuid);
	}

	bool Component3D::HasDataComponentOfType(const Guid typeGuid) const
	{
		return DataComponentOwner::HasDataComponentOfType(GetSceneRegistry(), typeGuid);
	}

	Optional<Data::Component*> Component3D::FindFirstDataComponentImplementingType(const Guid typeGuid) const
	{
		return DataComponentOwner::FindFirstDataComponentImplementingType(GetSceneRegistry(), typeGuid);
	}

	bool Component3D::HasAnyDataComponentsImplementingType(const Guid typeGuid) const
	{
		return DataComponentOwner::HasAnyDataComponentsImplementingType(GetSceneRegistry(), typeGuid);
	}

	template struct HierarchyComponent<Component3D>;

	[[maybe_unused]] const bool wasComponent3DTypeRegistered = Reflection::Registry::RegisterType<Component3D>();
	[[maybe_unused]] const bool wasComponent3DRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Component3D>>::Make());
}

namespace ngine::Entity::Data
{
	void BoundingBox::Set(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Math::BoundingBox newBoundingBox)
	{
		m_bounds = newBoundingBox;
		const EnumFlags<ComponentFlags> flags = owner.GetFlags(sceneRegistry);
		const bool isRegisteredInTree = !flags.AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource | ComponentFlags::IsRootScene);
		if (isRegisteredInTree)
		{
			if (flags.IsSet(ComponentFlags::Is3D))
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				owner3D.GetRootSceneComponent().OnComponentWorldLocationOrBoundsChanged(owner3D, sceneRegistry);
			}
		}
	}

	[[maybe_unused]] const bool wasInstanceGuidTypeRegistered = Reflection::Registry::RegisterType<InstanceGuid>();
	[[maybe_unused]] const bool wasInstanceGuidRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<InstanceGuid>>::Make()
	);

	[[maybe_unused]] const bool wasWorldTransformTypeRegistered = Reflection::Registry::RegisterType<WorldTransform>();
	[[maybe_unused]] const bool wasWorldTransformRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<WorldTransform>>::Make());

	[[maybe_unused]] const bool wasLocalTransformTypeRegistered = Reflection::Registry::RegisterType<LocalTransform3D>();
	[[maybe_unused]] const bool wasLocalTransformRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<LocalTransform3D>>::Make());

	[[maybe_unused]] const bool wasBoundingBoxTypeRegistered = Reflection::Registry::RegisterType<BoundingBox>();
	[[maybe_unused]] const bool wasBoundingBoxRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<BoundingBox>>::Make());

	[[maybe_unused]] const bool wasOctreeNodeTypeRegistered = Reflection::Registry::RegisterType<OctreeNode>();
	[[maybe_unused]] const bool wasOctreeNodeRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<OctreeNode>>::Make());

	[[maybe_unused]] const bool wasParentTypeRegistered = Reflection::Registry::RegisterType<Parent>();
	[[maybe_unused]] const bool wasParentRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Parent>>::Make());
}
