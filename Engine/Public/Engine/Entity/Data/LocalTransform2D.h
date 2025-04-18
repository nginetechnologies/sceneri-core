#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Common/Math/Transform2D.h>

namespace ngine::Entity::Data
{
	struct LocalTransform2D final : public Component
	{
		using TransformType = Math::TTransform2D<float>;

		LocalTransform2D(const Deserializer& deserializer) = delete;
		LocalTransform2D(const LocalTransform2D&, const Cloner&) = delete;
		LocalTransform2D(const TransformType& transform)
			: m_transform(transform)
		{
		}
		LocalTransform2D& operator=(const TransformType& transform)
		{
			m_transform = transform;
			return *this;
		}

		[[nodiscard]] TransformType GetTransform() const
		{
			return m_transform;
		}
		[[nodiscard]] Math::Vector2f GetLocation() const
		{
			return m_transform.GetLocation();
		}
		[[nodiscard]] Math::Rotation2Df GetRotation() const
		{
			return m_transform.GetRotation();
		}
		[[nodiscard]] Math::Vector2f GetScale() const
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

		[[nodiscard]] operator const TransformType &() const
		{
			return m_transform;
		}

		void SetTransform(const TransformType transform)
		{
			m_transform = transform;
		}
		void SetLocation(const Math::Vector2f location)
		{
			m_transform.SetLocation(location);
		}
		void SetRotation(const Math::Rotation2Df rotation)
		{
			m_transform.SetRotation(rotation);
		}
		void SetScale(const Math::Vector2f scale)
		{
			m_transform.SetScale(scale);
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::LocalTransform2D>;

		TransformType m_transform;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::LocalTransform2D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::LocalTransform2D>(
			"83e9394b-a7a4-4ac4-b646-4b49eb30d99d"_guid,
			MAKE_UNICODE_LITERAL("2D World Transform"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Transform"),
				"Transform",
				"465f27d4-4ef1-4782-a0e0-e1e92c42bf10"_guid,
				MAKE_UNICODE_LITERAL("Transform"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI,
				&Entity::Data::LocalTransform2D::m_transform
			)}
		);
	};
}
