#include <Components/Signals/Receiver.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework::Signal
{
	Receiver::Receiver(const Receiver&, const Cloner&)
	{
	}
	Receiver::Receiver(const Deserializer&)
	{
	}
	Receiver::Receiver(Initializer&&)
	{
	}

	void Receiver::SetMode(Entity::Component3D&, Mode mode)
	{
		m_mode = mode;
	}

	void Receiver::OnSignalReceived(const Transmitter&, Entity::Component3D& owner)
	{
		if (m_isSignaled)
		{
			return;
		}
		m_isSignaled = true;

		switch (m_mode)
		{
			case Mode::Latch:
			case Mode::Relay:
				Activate(owner);
				break;
			case Mode::InverseLatch:
			case Mode::InverseRelay:
				Deactivate(owner);
				break;
		}
	}

	void Receiver::OnSignalLost(const Transmitter&, Entity::Component3D& owner)
	{
		if (!m_isSignaled)
		{
			return;
		}
		m_isSignaled = false;

		switch (m_mode)
		{
			case Mode::Latch:
			case Mode::InverseLatch:
				break;
			case Mode::Relay:
				Deactivate(owner);
				break;
			case Mode::InverseRelay:
				Activate(owner);
				break;
		}
	}

	[[maybe_unused]] const bool wasReceiverRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Receiver>>::Make()
	);
	[[maybe_unused]] const bool wasReceiverTypeRegistered = Reflection::Registry::RegisterType<Receiver>();
	[[maybe_unused]] const bool wasModeTypeRegistered = Reflection::Registry::RegisterType<Mode>();
}
