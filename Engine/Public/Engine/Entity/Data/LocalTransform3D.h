#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Common/Math/Transform.h>

namespace ngine::Entity::Data
{
	struct LocalTransform3D final : public Component
	{
		LocalTransform3D(const Deserializer& deserializer) = delete;
		LocalTransform3D(const LocalTransform3D&, const Cloner&) = delete;
		LocalTransform3D(const Math::LocalTransform transform)
			: m_transform(transform)
		{
		}
		LocalTransform3D& operator=(const Math::LocalTransform transform)
		{
			m_transform = transform;
			return *this;
		}

		void SetLocation(const Math::Vector3f location)
		{
			m_transform.SetLocation(location);
		}
		void SetRotation(const Math::Quaternionf rotation)
		{
			m_transform.SetRotation(rotation);
		}
		void SetScale(const Math::Vector3f scale)
		{
			m_transform.SetScale(scale);
		}

		[[nodiscard]] Math::Vector3f GetLocation() const
		{
			return m_transform.GetLocation();
		}
		[[nodiscard]] Math::Quaternionf GetRotation() const
		{
			return m_transform.GetRotationQuaternion();
		}
		[[nodiscard]] Math::Vector3f GetScale() const
		{
			return m_transform.GetScale();
		}

		[[nodiscard]] operator Math::LocalTransform() const
		{
			return m_transform;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::LocalTransform3D>;

		Math::LocalTransform m_transform;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::LocalTransform3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::LocalTransform3D>(
			"7dd21b5e-ed13-42fb-b360-23f658b3bf5c"_guid,
			MAKE_UNICODE_LITERAL("Local Transform"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Local Transform"),
				"Transform",
				"{4FE509BE-54ED-4C93-9D1F-F576C283EF1C}"_guid,
				MAKE_UNICODE_LITERAL("Transform"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI,
				&Entity::Data::LocalTransform3D::m_transform
			)}
		);
	};
}
