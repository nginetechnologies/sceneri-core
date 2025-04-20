#include <Widgets/Data/PositionOscillator.h>
#include <Widgets/Widget.h>
#include <Widgets/Style/Entry.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/WorldTransform2D.h>
#include <Engine/Context/EventManager.inl>

#include <Common/Reflection/Registry.inl>

#include <Common/Math/LinearInterpolate.h>

namespace ngine::Widgets::Data
{
	PositionOscillator::PositionOscillator(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_speed(deserializer.m_reader.ReadWithDefaultValue<float>("speed", 1.0f))
		, m_duration(deserializer.m_reader.ReadWithDefaultValue<float>("duration", 1.0f))
		, m_maximum(deserializer.m_reader.ReadWithDefaultValue<Math::Vector2f>("maximum", Math::Vector2f(Math::Zero)))
		, m_triggerGuid(deserializer.m_reader.ReadWithDefaultValue<Guid>("event", Guid()))
	{
		Context::EventManager::Subscribe(
			m_triggerGuid,
			this,
			deserializer.GetParent(),
			deserializer.GetSceneRegistry(),
			[&owner = deserializer.GetParent()]()
			{
				Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
				Entity::ComponentTypeSceneData<PositionOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<PositionOscillator>();
				if (const Optional<PositionOscillator*> pPositionOscillator = owner.FindDataComponentOfType<PositionOscillator>(typeSceneData))
				{
					typeSceneData.EnableUpdate(*pPositionOscillator);
					pPositionOscillator->m_stopwatch.Stop();
				}
			}
		);
	}

	PositionOscillator::PositionOscillator(const PositionOscillator& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_speed(templateComponent.m_speed)
		, m_duration(templateComponent.m_duration)
		, m_maximum(templateComponent.m_maximum)
		, m_triggerGuid(templateComponent.m_triggerGuid)
	{
		Context::EventManager::Subscribe(
			m_triggerGuid,
			this,
			cloner.GetParent(),
			cloner.GetSceneRegistry(),
			[&owner = cloner.GetParent()]()
			{
				Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
				Entity::ComponentTypeSceneData<PositionOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<PositionOscillator>();
				if (const Optional<PositionOscillator*> pPositionOscillator = owner.FindDataComponentOfType<PositionOscillator>(typeSceneData))
				{
					typeSceneData.EnableUpdate(*pPositionOscillator);
					pPositionOscillator->m_stopwatch.Stop();
				}
			}
		);
	}

	PositionOscillator::~PositionOscillator()
	{
	}

	void PositionOscillator::OnDestroying(ParentType& owner)
	{
		Context::EventManager::Unsubscribe(m_triggerGuid, *this, owner, owner.GetSceneRegistry());
	}

	void PositionOscillator::OnEnable(ParentType& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<PositionOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<PositionOscillator>();
		typeSceneData.EnableUpdate(*this);
	}

	void PositionOscillator::OnDisable(ParentType& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<PositionOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<PositionOscillator>();
		typeSceneData.DisableUpdate(*this);
	}

	void PositionOscillator::Update()
	{
		Widget& owner = m_owner;
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& transformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
		Entity::Data::WorldTransform2D& worldTransform = *transformSceneData.GetComponentImplementation(owner.GetIdentifier());

		FrameTime frameTime;
		if (m_stopwatch.IsRunning())
		{
			frameTime = FrameTime(m_stopwatch.GetElapsedTime());
		}
		else
		{
			m_stopwatch.Start();
			frameTime = FrameTime(0_seconds);
			m_start = worldTransform.GetLocation();
		}

		auto anim = [this](float frameTime, float value) -> float
		{
			const float mul = (value - 0.0f) * 0.5f;
			const float off = mul + 0.0f;
			return Math::Sin(frameTime * m_speed - (Math::PI.GetRadians() * 0.5f)) * mul + off;
		};

		worldTransform.SetLocation(m_start + Math::Vector2f(anim(frameTime, m_maximum.x), anim(frameTime, m_maximum.y)));

		if (frameTime >= m_duration)
		{
			m_stopwatch.Stop();
			worldTransform.SetLocation(m_start);

			Entity::ComponentTypeSceneData<PositionOscillator>& typeSceneData = *sceneRegistry.FindComponentTypeData<PositionOscillator>();
			typeSceneData.DisableUpdate(*this);
		}
	}

	[[maybe_unused]] const bool wasPositionOscillatorTypeRegistered = Reflection::Registry::RegisterType<PositionOscillator>();
	[[maybe_unused]] const bool wasPositionOscillatorComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<PositionOscillator>>::Make());
}
