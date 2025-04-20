#include <Widgets/Data/OpacityOscillator.h>
#include <Widgets/Widget.h>
#include <Widgets/Style/Entry.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Random.h>

namespace ngine::Widgets::Data
{
	OpacityOscillator::OpacityOscillator(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_speed(deserializer.m_reader.ReadWithDefaultValue<float>("speed", 1.0f))
		, m_minimum(deserializer.m_reader.ReadWithDefaultValue<float>("minimum", 0.0f))
		, m_maximum(deserializer.m_reader.ReadWithDefaultValue<float>("maximum", 1.0f))
		, m_randomOffset(float(Math::Random(0, 10000)))
	{
		if (m_owner.IsEnabled())
		{
			Entity::SceneRegistry& sceneRegistry = deserializer.GetParent().GetSceneRegistry();
			Entity::ComponentTypeSceneData<OpacityOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<OpacityOscillator>();
			typeSceneData.EnableUpdate(*this);
		}
	}

	OpacityOscillator::OpacityOscillator(const OpacityOscillator& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_speed(templateComponent.m_speed)
		, m_minimum(templateComponent.m_minimum)
		, m_maximum(templateComponent.m_maximum)
		, m_randomOffset(templateComponent.m_randomOffset)
	{
		if (m_owner.IsEnabled())
		{
			Entity::SceneRegistry& sceneRegistry = cloner.GetParent().GetSceneRegistry();
			Entity::ComponentTypeSceneData<OpacityOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<OpacityOscillator>();
			typeSceneData.EnableUpdate(*this);
		}
	}

	OpacityOscillator::~OpacityOscillator()
	{
	}

	void OpacityOscillator::OnEnable()
	{
	}

	void OpacityOscillator::OnDisable()
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<OpacityOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<OpacityOscillator>();
		typeSceneData.DisableUpdate(*this);
	}

	void OpacityOscillator::Update()
	{
		FrameTime frameTime;
		if (m_stopwatch.IsRunning())
		{
			frameTime = FrameTime(m_stopwatch.GetElapsedTime());
		}
		else
		{
			m_stopwatch.Start();
			frameTime = FrameTime(0_seconds);
		}

		const float mul = (m_maximum - m_minimum) * 0.5f;
		const float off = mul + m_minimum;
		const Math::Ratiof opacity = Math::Cos(frameTime * m_speed + m_randomOffset) * mul + off;
		m_owner.EmplaceInlineStyle({Widgets::Style::ValueType::Opacity, opacity});
	}

	[[maybe_unused]] const bool wasOpacityOscillatorTypeRegistered = Reflection::Registry::RegisterType<OpacityOscillator>();
	[[maybe_unused]] const bool wasOpacityOscillatorComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<OpacityOscillator>>::Make());
}
