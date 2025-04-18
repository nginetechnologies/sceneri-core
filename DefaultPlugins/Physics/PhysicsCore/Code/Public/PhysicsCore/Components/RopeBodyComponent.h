#pragma once

#include "KinematicBodyComponent.h"

#include <Common/Asset/Picker.h>
#include <Common/Math/Primitives/Spline.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>
#include <PhysicsCore/ConstraintIdentifier.h>
#include <PhysicsCore/MaterialIdentifier.h>

namespace ngine::Entity
{
	struct SplineComponent;
	struct RopeComponent;
}

namespace ngine::Physics
{
	struct Material;

	struct RopeBodyComponent : public KinematicBodyComponent
	{
		static constexpr Guid TypeGuid = "32061D0F-E829-4E91-86A4-6B495DD319E5"_guid;

		using BaseType = KinematicBodyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = RopeBodyComponent::BaseType::Initializer;

			using BaseType::BaseType;
			Initializer(BaseType&& initializer, MaterialIdentifier materialIdentifier = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_materialIdentifier(materialIdentifier)
			{
			}

			MaterialIdentifier m_materialIdentifier;
		};

		RopeBodyComponent(Initializer&& initializer);
		RopeBodyComponent(const RopeBodyComponent& templateComponent, const Cloner& cloner);
		RopeBodyComponent(const Deserializer& deserializer);
		virtual ~RopeBodyComponent();

		[[nodiscard]] bool IsPhysicalized() const
		{
			return m_constraints.HasElements();
		}

		void Update();

		[[nodiscard]] ArrayView<const ReferenceWrapper<const BodyComponent>, uint16> GetSegments() const
		{
			ArrayView<const ReferenceWrapper<BodyComponent>, uint16> view = m_segments.GetView();
			return reinterpret_cast<ArrayView<const ReferenceWrapper<const BodyComponent>, uint16>&>(view);
		}

		[[nodiscard]] Optional<BodyComponent*> GetClosestSegmentBody(const Math::WorldCoordinate coordinate) const;

		void CreateCapsules(Entity::SceneRegistry& sceneRegistry);
		[[nodiscard]] Math::Lengthf GetSegmentHalfHeight() const
		{
			return m_segmentHalfHeight;
		}
	protected:
		RopeBodyComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);

		[[nodiscard]] Optional<Entity::SplineComponent*> GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Optional<Entity::RopeComponent*> GetRope(Entity::SceneRegistry& sceneRegistry) const;
		void CreateConstraints(const uint16 segmentCount, const Math::Lengthf segmentHalfLength);
	protected:
		friend struct Reflection::ReflectedType<Physics::RopeBodyComponent>;

		using PhysicalMaterialPicker = Asset::Picker;
		void SetMaterialAsset(const PhysicalMaterialPicker asset);
		PhysicalMaterialPicker GetMaterialAsset() const;

		MaterialIdentifier m_materialIdentifier;
		Math::Radiusf m_radius;
		Math::Lengthf m_segmentHalfHeight;

		Vector<ReferenceWrapper<BodyComponent>, uint16> m_segments;
		Vector<ConstraintIdentifier, uint16> m_constraints;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::RopeBodyComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::RopeBodyComponent>(
			Physics::RopeBodyComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Rope Body"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Physical Material"),
				"physical_material",
				"{A54607EE-6E6B-4BDF-B0AE-9EE04EB7BA93}"_guid,
				MAKE_UNICODE_LITERAL("Physics Material"),
				&Physics::RopeBodyComponent::SetMaterialAsset,
				&Physics::RopeBodyComponent::GetMaterialAsset
			)}
		);
	};
}
