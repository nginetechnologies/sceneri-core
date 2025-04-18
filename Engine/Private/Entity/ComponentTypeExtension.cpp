#include "Entity/ComponentTypeExtension.h"

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::ComponentCategory::CameraTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::CameraTag>(
			Entity::ComponentCategory::CameraTag::TypeGuid, MAKE_UNICODE_LITERAL("Camera")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::PrimitiveTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::PrimitiveTag>(
			Entity::ComponentCategory::PrimitiveTag::TypeGuid, MAKE_UNICODE_LITERAL("Primitive")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::GravityTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::GravityTag>(
			Entity::ComponentCategory::GravityTag::TypeGuid, MAKE_UNICODE_LITERAL("Gravity")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::LightTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::LightTag>(
			Entity::ComponentCategory::LightTag::TypeGuid, MAKE_UNICODE_LITERAL("Light")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::PhysicsTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::PhysicsTag>(
			Entity::ComponentCategory::PhysicsTag::TypeGuid, MAKE_UNICODE_LITERAL("Physics")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::SplineTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::SplineTag>(
			Entity::ComponentCategory::SplineTag::TypeGuid, MAKE_UNICODE_LITERAL("Spline")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::TextTag>
	{
		static constexpr auto Type =
			Reflection::Reflect<Entity::ComponentCategory::TextTag>(Entity::ComponentCategory::TextTag::TypeGuid, MAKE_UNICODE_LITERAL("Text"));
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::VehicleTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::VehicleTag>(
			Entity::ComponentCategory::VehicleTag::TypeGuid, MAKE_UNICODE_LITERAL("Vehicle")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::CharactersTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::CharactersTag>(
			Entity::ComponentCategory::CharactersTag::TypeGuid, MAKE_UNICODE_LITERAL("Characters")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::GameplayTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::GameplayTag>(
			Entity::ComponentCategory::GameplayTag::TypeGuid, MAKE_UNICODE_LITERAL("Gameplay")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::DataTag>
	{
		static constexpr auto Type =
			Reflection::Reflect<Entity::ComponentCategory::DataTag>(Entity::ComponentCategory::DataTag::TypeGuid, MAKE_UNICODE_LITERAL("Data"));
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::AudioTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::AudioTag>(
			Entity::ComponentCategory::AudioTag::TypeGuid, MAKE_UNICODE_LITERAL("Audio")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::AnimationTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::AnimationTag>(
			Entity::ComponentCategory::AnimationTag::TypeGuid, MAKE_UNICODE_LITERAL("Animation")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::WidgetTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::WidgetTag>(
			Entity::ComponentCategory::WidgetTag::TypeGuid, MAKE_UNICODE_LITERAL("Widget")
		);
	};

	template<>
	struct ReflectedType<Entity::ComponentCategory::NodeTag>
	{
		static constexpr auto Type =
			Reflection::Reflect<Entity::ComponentCategory::NodeTag>(Entity::ComponentCategory::NodeTag::TypeGuid, MAKE_UNICODE_LITERAL("Node"));
	};
	template<>
	struct ReflectedType<Entity::ComponentCategory::ControlNodeTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::ControlNodeTag>(
			Entity::ComponentCategory::ControlNodeTag::TypeGuid, MAKE_UNICODE_LITERAL("Control Node")
		);
	};
	template<>
	struct ReflectedType<Entity::ComponentCategory::LogicNodeTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::LogicNodeTag>(
			Entity::ComponentCategory::LogicNodeTag::TypeGuid, MAKE_UNICODE_LITERAL("Logic Node")
		);
	};
	template<>
	struct ReflectedType<Entity::ComponentCategory::OperatorNodeTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::OperatorNodeTag>(
			Entity::ComponentCategory::OperatorNodeTag::TypeGuid, MAKE_UNICODE_LITERAL("Operator Node")
		);
	};
	template<>
	struct ReflectedType<Entity::ComponentCategory::MathNodeTag>
	{
		static constexpr auto Type = Reflection::Reflect<Entity::ComponentCategory::MathNodeTag>(
			Entity::ComponentCategory::MathNodeTag::TypeGuid, MAKE_UNICODE_LITERAL("Math Node")
		);
	};
}
namespace ngine::Entity::ComponentCategory
{
	[[maybe_unused]] const bool wasCameraTagRegistered = Reflection::Registry::RegisterType<CameraTag>();
	[[maybe_unused]] const bool wasPrimitiveTagRegistered = Reflection::Registry::RegisterType<PrimitiveTag>();
	[[maybe_unused]] const bool wasGravityTagRegistered = Reflection::Registry::RegisterType<GravityTag>();
	[[maybe_unused]] const bool wasLightTagRegistered = Reflection::Registry::RegisterType<LightTag>();
	[[maybe_unused]] const bool wasPhysicTagRegistered = Reflection::Registry::RegisterType<PhysicsTag>();
	[[maybe_unused]] const bool wasSplineTagRegistered = Reflection::Registry::RegisterType<SplineTag>();
	[[maybe_unused]] const bool wasTextTagRegistered = Reflection::Registry::RegisterType<TextTag>();
	[[maybe_unused]] const bool wasVehicleTagRegistered = Reflection::Registry::RegisterType<VehicleTag>();
	[[maybe_unused]] const bool wasCharactersTagRegistered = Reflection::Registry::RegisterType<CharactersTag>();
	[[maybe_unused]] const bool wasGameplayTagRegistered = Reflection::Registry::RegisterType<GameplayTag>();
	[[maybe_unused]] const bool wasDataTagRegistered = Reflection::Registry::RegisterType<DataTag>();
	[[maybe_unused]] const bool wasAudioTagRegistered = Reflection::Registry::RegisterType<AudioTag>();
	[[maybe_unused]] const bool wasAnimationTagRegistered = Reflection::Registry::RegisterType<AnimationTag>();
	[[maybe_unused]] const bool wasWidgetTagRegistered = Reflection::Registry::RegisterType<WidgetTag>();

	[[maybe_unused]] const bool wasNodeTagRegistered = Reflection::Registry::RegisterType<NodeTag>();
	[[maybe_unused]] const bool wasControlNodeTagRegistered = Reflection::Registry::RegisterType<ControlNodeTag>();
	[[maybe_unused]] const bool wasLogicNodeTagRegistered = Reflection::Registry::RegisterType<LogicNodeTag>();
	[[maybe_unused]] const bool wasOperatorNodeTagRegistered = Reflection::Registry::RegisterType<OperatorNodeTag>();
	[[maybe_unused]] const bool wasMathNodeTagRegistered = Reflection::Registry::RegisterType<MathNodeTag>();
}
