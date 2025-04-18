#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/ForwardDeclarations/UnorderedMap.h>
#include <Common/Serialization/ForwardDeclarations/SerializedData.h>

namespace ngine::Entity
{
	struct SceneChildInstance;
}

namespace ngine::Threading
{
	struct StageBase;
}

namespace ngine::Entity::Data
{
	//! Represents an external scene asset that is instantiated inside our parent component
	struct ExternalScene final : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;

		ExternalScene(const Entity::ComponentTemplateIdentifier templateIdentifier);
		ExternalScene(const Deserializer& deserializer);
		ExternalScene(const ExternalScene& templateComponent, const Cloner& cloner);

		[[nodiscard]] Entity::ComponentTemplateIdentifier GetTemplateIdentifier() const
		{
			return m_componentTemplateIdentifier;
		}
		void SetTemplateIdentifier(const Entity::ComponentTemplateIdentifier templateIdentifier)
		{
			m_componentTemplateIdentifier = templateIdentifier;
		}

		[[nodiscard]] Asset::Guid GetAssetGuid() const;

		void OnMasterSceneLoaded(
			Entity::HierarchyComponentBase& component,
			const Optional<const Entity::HierarchyComponentBase*> templateSceneComponent,
			Serialization::Data&& serializedData,
			Threading::StageBase& finishedStage
		);

		void SpawnAssetChildren(
			Entity::HierarchyComponentBase& component, const Optional<const Entity::HierarchyComponentBase*> templateSceneComponent
		);

		[[nodiscard]] bool SerializeTemplateComponents(
			Serialization::Writer writer, const Entity::HierarchyComponentBase& component, const Entity::HierarchyComponentBase& templateComponent
		);
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::ExternalScene>;
		//! Template identifier of the asset we are instantiating
		Entity::ComponentTemplateIdentifier m_componentTemplateIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::ExternalScene>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::ExternalScene>(
			"{76A8CFB0-D285-454B-A90F-10324E2ABF88}"_guid,
			MAKE_UNICODE_LITERAL("External Scene Data Component"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableWriteToDisk |
				TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
