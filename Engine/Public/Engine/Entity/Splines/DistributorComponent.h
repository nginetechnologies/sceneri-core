#pragma once

#include <Engine/Entity/Component3D.h>

#include <Renderer/Assets/Defaults.h>

#include <Common/Function/Function.h>
#include <Common/Asset/Picker.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/AtomicEnumFlags.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Entity
{
	struct SplineComponent;

	struct DistributorComponent : public Component3D
	{
		static constexpr Guid TypeGuid = "f3a902a8-4814-4328-927c-2d43c721ac1e"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		struct Initializer : public Component3D::Initializer
		{
			using BaseType = Component3D::Initializer;
			Initializer(BaseType&& initializer, const Asset::Guid paintedAssetGuid = Rendering::Constants::DefaultMeshSceneAssetGuid)
				: BaseType(Forward<BaseType>(initializer))
				, m_paintedAssetGuid(paintedAssetGuid)
			{
			}

			Asset::Guid m_paintedAssetGuid = Rendering::Constants::DefaultMeshSceneAssetGuid;
		};

		DistributorComponent(const DistributorComponent& templateComponent, const Cloner& cloner);
		DistributorComponent(const Deserializer& deserializer);
		DistributorComponent(Initializer&& initializer);
		virtual ~DistributorComponent();
		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		void OnCreated();

		[[nodiscard]] Optional<SplineComponent*> GetSpline() const;

		using RepopulatedCallback = Function<void(), 24>;
		void Repopulate(RepopulatedCallback&& callback);

		enum class Flags : uint8
		{
			None,
			IsRepopulationQueued = 1 << 1,
			ShouldRequeueRepopulation = 1 << 2
		};

		[[nodiscard]] Math::Radiusf GetNextAssetRadius() const
		{
			return m_nextRadius;
		}
		void SetAsset(const Asset::Guid asset)
		{
			m_asset = asset;
		}
	private:
		friend struct Reflection::ReflectedType<Entity::DistributorComponent>;
		DistributorComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);

		Threading::Mutex m_mutex;
		AtomicEnumFlags<Flags> m_flags;

		Math::Radiusf m_nextRadius;

		using ComponentTypePicker = Asset::Picker;
		void SetComponentType(const ComponentTypePicker asset);
		ComponentTypePicker GetComponentType() const;

		Asset::Guid m_asset = Rendering::Constants::DefaultMeshSceneAssetGuid;
		Math::Lengthf m_offset = 0.5_meters;
	};

	ENUM_FLAG_OPERATORS(DistributorComponent::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::DistributorComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::DistributorComponent>(
			Entity::DistributorComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Asset Distributor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Asset"),
				"asset",
				"{7F726335-567F-452D-8B8B-9C281DE0E8EF}"_guid,
				MAKE_UNICODE_LITERAL("Distributor"),
				&Entity::DistributorComponent::SetComponentType,
				&Entity::DistributorComponent::GetComponentType
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"df0a3a2d-9b79-dbfc-7f87-6cbf0c5a9541"_asset,
				"5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid,
				"2b199751-361c-4915-b7f0-3658157d96ab"_asset
			}}
		);
	};
}
