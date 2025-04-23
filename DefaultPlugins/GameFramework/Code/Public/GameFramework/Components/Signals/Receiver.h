#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Reflection/EnumTypeExtension.h>
#include <Common/Function/Event.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework::Signal
{
	struct Transmitter;

	enum class Mode : uint8
	{
		//! Activate on signal, stay activated
		Latch,
		//! Deactivate on signal, stay deactivated
		InverseLatch,
		//! Activate on signal, deactivate on signal loss
		Relay,
		//! Deactivate on signal, activate on signal loss
		InverseRelay
	};

	struct Receiver : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using Initializer = BaseType::DynamicInitializer;

		Receiver(const Receiver& templateComponent, const Cloner& cloner);
		Receiver(const Deserializer& deserializer);
		Receiver(Initializer&& initializer);
	protected:
		friend Transmitter;

		void OnSignalReceived(const Transmitter& transmitter, Entity::Component3D& owner);
		void OnSignalLost(const Transmitter& transmitter, Entity::Component3D& owner);

		virtual void Activate(Entity::Component3D& owner) = 0;
		virtual void Deactivate(Entity::Component3D& owner) = 0;

		virtual void SetMode(Entity::Component3D& owner, Mode mode);
		[[nodiscard]] Mode GetMode() const
		{
			return m_mode;
		}
	protected:
		friend struct Reflection::ReflectedType<Receiver>;
		Mode m_mode{Mode::Relay};
		// TODO: Support multiple transmitters and needing more than one to activate
		bool m_isSignaled{false};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Mode>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Mode>(
			"F81BE133-5DB3-49AC-9B98-BE45033A1954"_guid,
			MAKE_UNICODE_LITERAL("Signal Mode"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{GameFramework::Signal::Mode::Latch, MAKE_UNICODE_LITERAL("Latch")},
				Reflection::EnumTypeEntry{GameFramework::Signal::Mode::InverseLatch, MAKE_UNICODE_LITERAL("Inverted Latch")},
				Reflection::EnumTypeEntry{GameFramework::Signal::Mode::Relay, MAKE_UNICODE_LITERAL("Relay")},
				Reflection::EnumTypeEntry{GameFramework::Signal::Mode::InverseRelay, MAKE_UNICODE_LITERAL("Inverted Relay")}
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Signal::Receiver>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Receiver>(
			"fbe54d3f-09cc-4a96-85c0-801a0cc29518"_guid,
			MAKE_UNICODE_LITERAL("Signal Receiver"),
			TypeFlags::IsAbstract,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Mode"),
				"mode",
				"{80E36592-898A-47A1-A627-F297A1319EA9}"_guid,
				MAKE_UNICODE_LITERAL("Receiver"),
				Reflection::PropertyFlags::VisibleToParentScope,
				&GameFramework::Signal::Receiver::SetMode,
				&GameFramework::Signal::Receiver::GetMode
			)}
		);
	};
}
