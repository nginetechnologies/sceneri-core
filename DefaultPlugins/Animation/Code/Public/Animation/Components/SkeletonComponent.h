#pragma once

#include <Engine/Entity/Component3D.h>
#include <Common/Asset/Picker.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Function/ThreadSafeEvent.h>

#include <Animation/SkeletonInstance.h>
#include <Animation/SkeletonIdentifier.h>

namespace ngine::Animation
{
	struct SkinnedMeshComponent;
	struct SkeletonJointComponent;
	struct Controller;

	struct SkeletonComponent : public Entity::Component3D
	{
	public:
		static constexpr Guid TypeGuid = "f6140603-c29e-4bf9-946d-498dba6709ff"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Component3D::Initializer;
			using BaseType::BaseType;
			Initializer(const BaseType& other)
				: BaseType(other)
			{
			}

			SkeletonIdentifier m_skeletonIdentifier;
		};

		SkeletonComponent(Initializer&& initializer);
		SkeletonComponent(const SkeletonComponent& templateComponent, const Cloner& cloner);
		SkeletonComponent(const Deserializer& deserializer);
		virtual ~SkeletonComponent();

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		void OnCreated();
		void SetAnimationController(Controller* pAnimationController);

		void Update();
		void OnEnable();
		void OnDisable();

		[[nodiscard]] const SkeletonInstance& GetSkeletonInstance() const
		{
			return m_skeletonInstance;
		}
		[[nodiscard]] SkeletonInstance& GetSkeletonInstance()
		{
			return m_skeletonInstance;
		}
		[[nodiscard]] Optional<Controller*> GetAnimationController() const
		{
			return m_pAnimationController;
		}

		void TryEnableUpdate();

		ThreadSafe::Event<void(void*), 24> OnToggledUpdate;
	protected:
		SkeletonComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);

		friend struct Reflection::ReflectedType<ngine::Animation::SkeletonComponent>;
		using SkeletonReference = Asset::Picker;
		void SetSkeleton(const SkeletonReference asset);
		Threading::JobBatch SetDeserializedSkeleton(
			const SkeletonReference asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader
		);
		SkeletonReference GetSkeleton() const;

		bool CanEnableUpdate() const;
	private:
		Threading::Atomic<bool> m_isUpdateEnabled = false;
		SkeletonIdentifier m_skeletonIdentifier;
		SkeletonInstance m_skeletonInstance;
		Controller* m_pAnimationController = nullptr;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::SkeletonComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::SkeletonComponent>(
			Animation::SkeletonComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Skeleton"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Skeleton"),
				"skeleton",
				"{415ABBF1-3C25-4E28-ADD8-80E8BE11BD30}"_guid,
				MAKE_UNICODE_LITERAL("Skeleton"),
				&Animation::SkeletonComponent::SetSkeleton,
				&Animation::SkeletonComponent::GetSkeleton,
				&Animation::SkeletonComponent::SetDeserializedSkeleton
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2D002E95-6D4F-48BE-9C62-8CEA8F1E7CA9"_asset, "9D186C9A-3D74-4E92-A5DD-A0F16AC9C138"_guid
			}}
		);
	};
}
