#include <Widgets/Data/ConstantRotation.h>
#include <Widgets/Widget.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/WorldTransform2D.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	ConstantRotation::ConstantRotation(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_speed(deserializer.m_reader.ReadWithDefaultValue<Math::RotationalSpeedf>("speed", 1_rads))
	{
		if (m_owner.IsEnabled())
		{
			Entity::SceneRegistry& sceneRegistry = deserializer.GetSceneRegistry();
			Entity::ComponentTypeSceneData<ConstantRotation>& typeSceneData = *sceneRegistry.FindComponentTypeData<ConstantRotation>();
			typeSceneData.EnableUpdate(*this);
		}
	}

	ConstantRotation::ConstantRotation(const ConstantRotation& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_speed(templateComponent.m_speed)
	{
		if (m_owner.IsEnabled())
		{
			Entity::SceneRegistry& sceneRegistry = cloner.GetSceneRegistry();
			Entity::ComponentTypeSceneData<ConstantRotation>& typeSceneData = *sceneRegistry.FindComponentTypeData<ConstantRotation>();
			typeSceneData.EnableUpdate(*this);
		}
	}

	ConstantRotation::~ConstantRotation()
	{
	}

	void ConstantRotation::OnEnable()
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<ConstantRotation>& typeSceneData = *sceneRegistry.FindComponentTypeData<ConstantRotation>();
		typeSceneData.EnableUpdate(*this);
	}

	void ConstantRotation::OnDisable()
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<ConstantRotation>& typeSceneData = *sceneRegistry.FindComponentTypeData<ConstantRotation>();
		typeSceneData.DisableUpdate(*this);
	}

	void ConstantRotation::Update()
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& transformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
		Entity::Data::WorldTransform2D& worldTransform = *transformSceneData.GetComponentImplementation(m_owner.GetIdentifier());

		FrameTime frameTime;
		if (m_stopwatch.IsRunning())
		{
			frameTime = FrameTime(m_stopwatch.GetElapsedTimeAndRestart());
		}
		else
		{
			m_stopwatch.Start();
			frameTime = FrameTime(0_seconds);
		}

		Math::WorldRotation2D angle = worldTransform.GetRotation();
		const Time::Durationf frameDuration = frameTime;
		angle += m_speed * frameDuration;
		angle.Wrap();
		worldTransform.SetRotation(angle);
	}

	[[maybe_unused]] const bool wasConstantRotationTypeRegistered = Reflection::Registry::RegisterType<ConstantRotation>();
	[[maybe_unused]] const bool wasConstantRotationComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ConstantRotation>>::Make());
}
