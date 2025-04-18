#pragma once

#include <Engine/Entity/Component3D.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Threading/AtomicBool.h>

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Animation/MeshSkinIdentifier.h>
#include <Animation/JointIndex.h>
#include <Animation/SkeletonJointPicker.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Animation
{
	struct MeshSkin;
	struct Skeleton;
	struct SkeletonComponent;

	struct SkeletonJointComponent : public Entity::Component3D
	{
	public:
		static constexpr Guid TypeGuid = "5f894dea-bac9-4b62-ab51-291dccd451a0"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		SkeletonJointComponent(const SkeletonJointComponent& templateComponent, const Cloner& cloner);
		SkeletonJointComponent(const Deserializer& deserializer);
		SkeletonJointComponent(Initializer&& initializer);
		virtual ~SkeletonJointComponent();

		// Entity::Component3D
		virtual void OnAttachedToNewParent() override;
		virtual void OnBeforeDetachFromParent() override;
		// ~Entity::Component3D

		[[nodiscard]] Guid GetJointGuid()
		{
			return m_targetJointGuid;
		}
		void SetJointByGuid(const Guid jointGuid);

		void OnCreated();
		void Update();
	protected:
		friend struct Reflection::ReflectedType<Animation::SkeletonJointComponent>;

		[[nodiscard]] bool CanEnableUpdate() const;
		void TryEnableUpdate();

		Optional<SkeletonComponent*> GetSkeleton() const;

		void SetJointPicker(const SkeletonJointPicker asset);
		Threading::JobBatch SetDeserializedJointPicker(
			const SkeletonJointPicker asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader
		);
		[[nodiscard]] SkeletonJointPicker GetJointPicker() const;
	private:
		Math::Matrix3x4f m_relativeToJointTransform = Math::Identity;
		Guid m_targetJointGuid;
		JointIndex m_targetJointIndex = InvalidJointIndex;
		Threading::Atomic<bool> m_isUpdateEnabled = false;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::SkeletonJointComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::SkeletonJointComponent>(
			Animation::SkeletonJointComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Skeleton Joint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Joint"),
				"joint",
				"{6292B389-D62A-4553-A8B3-026DE94A9D08}"_guid,
				MAKE_UNICODE_LITERAL("Skeleton Joint"),
				&Animation::SkeletonJointComponent::SetJointPicker,
				&Animation::SkeletonJointComponent::GetJointPicker,
				&Animation::SkeletonJointComponent::SetDeserializedJointPicker
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2D002E95-6D4F-48BE-9C62-8CEA8F1E7CA9"_asset, "9D186C9A-3D74-4E92-A5DD-A0F16AC9C138"_guid
			}}
		);
	};
}
