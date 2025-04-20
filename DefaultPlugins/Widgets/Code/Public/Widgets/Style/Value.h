#pragma once

#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include "Size.h"
#include "SizeCorners.h"
#include "ValueTypeIdentifier.h"
#include <Widgets/Style/ReferenceValue.h>

#include "ForwardDeclarations/Value.h"

#include <FontRendering/FontWeight.h>
#include <FontRendering/Point.h>

#include <Engine/Entity/ComponentSoftReference.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Math/Color.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Asset/Guid.h>

namespace ngine::Widgets::Style
{
	struct Value
	{
		using TypeIdentifier = ValueTypeIdentifier;

		template<typename Type>
		constexpr Value(const TypeIdentifier typeIdentifier, Type&& value)
			: m_typeIdentifier(typeIdentifier)
			, m_value(Forward<Type>(value))
		{
		}
		template<typename Type>
		constexpr Value& operator=(Type&& value)
		{
			m_value = Forward<Type>(value);
			return *this;
		}
		[[nodiscard]] TypeIdentifier GetTypeIdentifier() const
		{
			return m_typeIdentifier;
		}

		[[nodiscard]] const EntryValue& Get() const LIFETIME_BOUND
		{
			return m_value;
		}
		[[nodiscard]] EntryValue& Get() LIFETIME_BOUND
		{
			return m_value;
		}
	private:
		TypeIdentifier m_typeIdentifier;
		EntryValue m_value;
	};
}
