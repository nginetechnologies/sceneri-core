#include "Components/SkeletonJointComponent.h"
#include "Components/SkeletonComponent.h"
#include "MeshSkin.h"
#include "Plugin.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Assets/StaticMesh/StaticMesh.h>

#include "MeshSkin.h"
#include <Renderer/Assets/StaticMesh/RenderMesh.h>
#include <Renderer/Buffers/DeviceMemoryView.h>

#include <Common/Platform/Windows.h>

namespace ngine::Animation
{
	bool SkeletonJointPicker::Serialize(const Serialization::Reader serializer)
	{
		return serializer.SerializeInPlace(m_currentSelection);
	}

	bool SkeletonJointPicker::Serialize(Serialization::Writer serializer) const
	{
		return serializer.SerializeInPlace(m_currentSelection);
	}

	SkeletonJointComponent::SkeletonJointComponent(const SkeletonJointComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_relativeToJointTransform(templateComponent.m_relativeToJointTransform)
		, m_targetJointIndex(templateComponent.m_targetJointIndex)
	{
	}

	SkeletonJointComponent::SkeletonJointComponent(const Deserializer& deserializer)
		: Component3D(deserializer)
		, m_relativeToJointTransform(GetRelativeTransform())
	{
		if(const Optional<Serialization::Reader> componentSerializer =
		       deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SkeletonJointComponent>().ToString().GetView()))
		{
			if (Optional<SkeletonComponent*> pSkeleton = GetSkeleton())
			{
				m_targetJointGuid = componentSerializer->ReadWithDefaultValue<Guid>("joint", {});
			}
		}
	}

	SkeletonJointComponent::SkeletonJointComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_relativeToJointTransform(initializer.m_localTransform)
	{
	}

	SkeletonJointComponent::~SkeletonJointComponent()
	{
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkeletonJointComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkeletonJointComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}
	}

	void SkeletonJointComponent::OnCreated()
	{
		TryEnableUpdate();
	}

	Optional<SkeletonComponent*> SkeletonJointComponent::GetSkeleton() const
	{
		return FindFirstParentOfType<SkeletonComponent>();
	}

	void SkeletonJointComponent::OnAttachedToNewParent()
	{
		BaseType::OnAttachedToNewParent();

		if (Optional<SkeletonComponent*> pSkeletonComponent = GetSkeleton())
		{
			if (const Optional<const Skeleton*> pSkeleton = pSkeletonComponent->GetSkeletonInstance().GetSkeleton())
			{
				if (const Optional<JointIndex> jointIndex = pSkeleton->FindJointIndex(m_targetJointGuid))
				{
					m_targetJointIndex = *jointIndex;
					TryEnableUpdate();
				}
			}
		}
	}

	void SkeletonJointComponent::OnBeforeDetachFromParent()
	{
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkeletonJointComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkeletonJointComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}
	}

	bool SkeletonJointComponent::CanEnableUpdate() const
	{
		if (Optional<SkeletonComponent*> pSkeletonComponent = GetSkeleton())
		{
			if (const Optional<const Skeleton*> pSkeleton = pSkeletonComponent->GetSkeletonInstance().GetSkeleton())
			{
				const uint16 jointCount = pSkeleton->GetJointCount();
				if (UNLIKELY(m_targetJointIndex >= jointCount))
				{
					return false;
				}

				return true;
			}
		}
		return false;
	}

	void SkeletonJointComponent::TryEnableUpdate()
	{
		if (CanEnableUpdate())
		{
			bool expected = false;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, true))
			{
				Entity::ComponentTypeSceneData<SkeletonJointComponent>& skeletonJointTypeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkeletonJointComponent>&>(*GetTypeSceneData());
				Entity::ComponentTypeSceneData<SkeletonComponent>& skeletonTypeSceneData =
					*GetSceneRegistry().FindComponentTypeData<SkeletonComponent>();
				skeletonJointTypeSceneData.EnableUpdate(*this);

				skeletonTypeSceneData.GetUpdateStage()->AddSubsequentStage(*skeletonJointTypeSceneData.GetUpdateStage(), GetSceneRegistry());
			}
		}
		else
		{
			bool expected = true;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
			{
				Entity::ComponentTypeSceneData<SkeletonJointComponent>& typeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkeletonJointComponent>&>(*GetTypeSceneData());
				typeSceneData.DisableUpdate(*this);
			}
		}
	}

	void SkeletonJointComponent::Update()
	{
		Assert(CanEnableUpdate());

		const SkeletonComponent& skeletonComponent = static_cast<const SkeletonComponent&>(*GetSkeleton());
		const Math::Matrix4x4f& jointMatrix = skeletonComponent.GetSkeletonInstance().GetModelSpaceMatrices()[m_targetJointIndex];
		Math::LocalTransform jointTransform = reinterpret_cast<const Math::Matrix3x4f&>(jointMatrix);

		jointTransform = jointTransform.Transform(Math::LocalTransform(m_relativeToJointTransform));

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		SetRelativeTransform(jointTransform, sceneRegistry);
	}

	void SkeletonJointComponent::SetJointByGuid(const Guid jointGuid)
	{
		m_targetJointGuid = jointGuid;
		if (Optional<SkeletonComponent*> pSkeletonComponent = GetSkeleton())
		{
			if (const Optional<const Skeleton*> pSkeleton = pSkeletonComponent->GetSkeletonInstance().GetSkeleton())
			{
				m_targetJointIndex = *pSkeleton->FindJointIndex(jointGuid);
				TryEnableUpdate();
			}
		}
	}

	void SkeletonJointComponent::SetJointPicker(const SkeletonJointPicker joint)
	{
		SetJointByGuid(joint.m_currentSelection);
	}

	Threading::JobBatch SkeletonJointComponent::SetDeserializedJointPicker(
		const SkeletonJointPicker joint,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		m_targetJointGuid = joint.m_currentSelection;
		if (Optional<SkeletonComponent*> pSkeletonComponent = GetSkeleton())
		{
			return Threading::CreateCallback(
				[this, &skeletonComponent = *pSkeletonComponent, jointGuid = joint.m_currentSelection](const Threading::JobRunnerThread&)
				{
					if (const Optional<const Skeleton*> pSkeleton = skeletonComponent.GetSkeletonInstance().GetSkeleton())
					{
						m_targetJointIndex = *pSkeleton->FindJointIndex(jointGuid);
						TryEnableUpdate();
					}
				},
				Threading::JobPriority::LoadSkeletonJoint
			);
		}
		return {};
	}

	SkeletonJointPicker SkeletonJointComponent::GetJointPicker() const
	{
		const Optional<SkeletonComponent*> pSkeletonComponent = GetSkeleton();
		return SkeletonJointPicker{
			pSkeletonComponent.IsValid() ? pSkeletonComponent->GetSkeletonInstance().GetSkeleton() : nullptr,
			m_targetJointGuid
		};
	}

	[[maybe_unused]] const bool wasSkeletonJointRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SkeletonJointComponent>>::Make());
	[[maybe_unused]] const bool wasSkeletonJointTypeRegistered = Reflection::Registry::RegisterType<SkeletonJointComponent>();
}
