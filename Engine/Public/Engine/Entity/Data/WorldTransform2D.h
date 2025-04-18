#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Common/Math/Transform2D.h>

namespace ngine::Entity::Data
{
	struct WorldTransform2D final : public Component
	{
		WorldTransform2D(const Deserializer& deserializer) = delete;
		WorldTransform2D(const WorldTransform2D&, const Cloner&) = delete;
		WorldTransform2D(const Math::WorldTransform2D& transform)
			: m_transform(transform)
		{
		}
		WorldTransform2D& operator=(const Math::WorldTransform2D& transform)
		{
			m_transform = transform;
			return *this;
		}

		[[nodiscard]] Math::WorldTransform2D GetTransform() const
		{
			return m_transform;
		}
		[[nodiscard]] Math::WorldCoordinate2D GetLocation() const
		{
			return m_transform.GetLocation();
		}
		[[nodiscard]] Math::WorldRotation2D GetRotation() const
		{
			return m_transform.GetRotation();
		}
		[[nodiscard]] Math::WorldScale2D GetScale() const
		{
			return m_transform.GetScale();
		}
		[[nodiscard]] Math::Vector2f GetForwardColumn() const
		{
			return m_transform.GetForwardColumn();
		}
		[[nodiscard]] Math::Vector2f GetUpColumn() const
		{
			return m_transform.GetUpColumn();
		}

		[[nodiscard]] operator const Math::WorldTransform2D &() const
		{
			return m_transform;
		}

		void SetTransform(const Math::WorldTransform2D transform)
		{
			m_transform = transform;
		}
		void SetLocation(const Math::WorldCoordinate2D location)
		{
			m_transform.SetLocation(location);
		}
		void SetRotation(const Math::WorldRotation2D rotation)
		{
			m_transform.SetRotation(rotation);
		}
		void SetScale(const Math::WorldScale2D scale)
		{
			m_transform.SetScale(scale);
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::WorldTransform2D>;

		Math::WorldTransform2D m_transform;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::WorldTransform2D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::WorldTransform2D>(
			"{2b8920a3-ef4e-471c-acc7-a10b5676e0e0}"_guid,
			MAKE_UNICODE_LITERAL("2D World Transform"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Transform"),
				"Transform",
				"{11DB6BBE-8B04-4CFF-B04E-B9A9FC8EDC19}"_guid,
				MAKE_UNICODE_LITERAL("Transform"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI,
				&Entity::Data::WorldTransform2D::m_transform
			)}
		);
	};
}
