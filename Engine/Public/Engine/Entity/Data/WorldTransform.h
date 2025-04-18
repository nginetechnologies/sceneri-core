#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Common/Math/Transform.h>

namespace ngine::Entity::Data
{
	struct WorldTransform final : public Component
	{
		WorldTransform(const Deserializer& deserializer) = delete;
		WorldTransform(const WorldTransform&, const Cloner&) = delete;
		WorldTransform(const Math::WorldTransform transform)
			: m_transform(transform)
		{
		}
		WorldTransform& operator=(const Math::WorldTransform transform)
		{
			m_transform = transform;
			return *this;
		}

		void SetLocation(const Math::WorldCoordinate location)
		{
			m_transform.SetLocation(location);
		}
		void SetRotation(const Math::WorldRotation rotation)
		{
			m_transform.SetRotation(rotation);
		}
		void SetScale(const Math::WorldScale scale)
		{
			m_transform.SetScale(scale);
		}

		[[nodiscard]] Math::WorldCoordinate GetLocation() const
		{
			return m_transform.GetLocation();
		}
		[[nodiscard]] Math::WorldRotation GetRotation() const
		{
			return m_transform.GetRotationQuaternion();
		}
		[[nodiscard]] Math::WorldScale GetScale() const
		{
			return m_transform.GetScale();
		}

		[[nodiscard]] operator Math::WorldTransform() const
		{
			return m_transform;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::WorldTransform>;

		Math::WorldTransform m_transform;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::WorldTransform>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::WorldTransform>(
			"{6A398EAD-7793-490B-902C-A25C6EB29B98}"_guid,
			MAKE_UNICODE_LITERAL("World Transform"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Transform"),
				"Transform",
				"{9371C1E6-846D-4812-9B58-E9E293DF9313}"_guid,
				MAKE_UNICODE_LITERAL("Transform"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI,
				&Entity::Data::WorldTransform::m_transform
			)}
		);
	};
}
