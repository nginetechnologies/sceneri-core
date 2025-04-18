#pragma once

#include "Scene/SceneComponent.h"
#include <Common/Reflection/DynamicObject.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Reflection
{
	struct DynamicTypeDefinition;
}

namespace ngine::Entity
{
	struct DynamicObjectSceneComponent : public SceneComponent
	{
		using BaseType = SceneComponent;

		DynamicObjectSceneComponent(const DynamicObjectSceneComponent& templateComponent, const Cloner& cloner);
		DynamicObjectSceneComponent(const Deserializer& deserializer);
		DynamicObjectSceneComponent(Initializer&& initializer);
		virtual ~DynamicObjectSceneComponent();

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;
	protected:
		friend struct Reflection::ReflectedType<Entity::DynamicObjectSceneComponent>;
		using ObjectTypePicker = Asset::Picker;
		void SetDynamicObjectType(const ObjectTypePicker asset);
		Threading::JobBatch SetDeserializedDynamicObjectType(
			const ObjectTypePicker asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader
		);
		ObjectTypePicker GetDynamicObjectType() const;

		UniquePtr<Reflection::DynamicTypeDefinition> m_pDynamicType;
		Any m_dynamicObject;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::DynamicObjectSceneComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::DynamicObjectSceneComponent>(
			"0ae25c25-2041-4078-a033-a56a567b0b73"_guid,
			MAKE_UNICODE_LITERAL("Dynamic Object Scene"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Type"),
				"type",
				"{38969DE9-1D73-4FAF-9647-43CABD6D3F3E}"_guid,
				MAKE_UNICODE_LITERAL("type"),
				&Entity::DynamicObjectSceneComponent::SetDynamicObjectType,
				&Entity::DynamicObjectSceneComponent::GetDynamicObjectType,
				&Entity::DynamicObjectSceneComponent::SetDeserializedDynamicObjectType
			)}
		);
	};
}
