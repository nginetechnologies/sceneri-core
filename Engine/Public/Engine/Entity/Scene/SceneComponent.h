#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>

#include <Common/Asset/Picker.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Threading
{
	struct StageBase;
}

namespace ngine::Entity
{
	struct RootSceneComponent;

	struct SceneComponent : public Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Component3D::Initializer;
			Initializer(
				const BaseType& initializer, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = {}, const bool load = true
			)
				: BaseType(initializer)
				, m_sceneTemplateIdentifier(sceneTemplateIdentifier)
				, m_load(load)
			{
				Assert(initializer.m_pJobBatch.IsValid());
			}

			Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
			bool m_load{true};
		};

		SceneComponent(const SceneComponent& templateComponent, const Cloner& cloner);
		SceneComponent(const Deserializer& deserializer);
		SceneComponent(Initializer&& initializer);
		virtual ~SceneComponent();

		[[nodiscard]] Threading::JobBatch DeserializeDataComponentsAndChildren(const Serialization::Reader serializer);
		bool SerializeDataComponentsAndChildren(Serialization::Writer serializer) const;

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		[[nodiscard]] Asset::Guid GetAssetGuid() const;

		void OnDisable();

		void SetSceneTemplateIdentifier(const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier);
		[[nodiscard]] Entity::ComponentTemplateIdentifier GetSceneTemplateIdentifier() const;
	protected:
		SceneComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);
		SceneComponent(const Deserializer& deserializer, const Asset::Guid sceneAssetGuid);
		SceneComponent(
			Scene3D& scene,
			SceneRegistry& sceneRegistry,
			const Optional<HierarchyComponentBase*> pParent,
			RootSceneComponent& rootSceneComponent,
			const EnumFlags<ComponentFlags> flags,
			const Guid instanceGuid,
			const Math::BoundingBox localBoundingBox,
			const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier
		);

		void SpawnAssetChildren(const Optional<const Entity::Component3D*> templateSceneComponent);
	protected:
		friend Scene3D;
		friend struct Reflection::ReflectedType<Entity::SceneComponent>;

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		void SetSceneGuid(const Asset::Guid guid);
		Threading::JobBatch
		SetDeseralizedSceneAsset(const ScenePicker asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader);
		ScenePicker GetSceneAsset() const;

		Threading::Mutex m_loadSceneMutex;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::SceneComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::SceneComponent>(
			"0c3f4f29-0d53-4878-922a-cca7d6af7bb6"_guid,
			MAKE_UNICODE_LITERAL("3D Scene"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Scene"),
				"scene",
				"{EC927454-1041-41F9-8CBE-4B167253B32A}"_guid,
				MAKE_UNICODE_LITERAL("Scene"),
				&Entity::SceneComponent::SetSceneAsset,
				&Entity::SceneComponent::GetSceneAsset,
				&Entity::SceneComponent::SetDeseralizedSceneAsset,
				Reflection::PropertyFlags::HideFromUI
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "20ecc097-a90f-4f39-ada0-3493d9b1efcc"_asset, "9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid
			}}
		);
	};
}
