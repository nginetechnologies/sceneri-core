#pragma once

#include "Engine/Scripting/Parser/StringType.h"
#include "Common/Scripting/VirtualMachine/FunctionIdentifier.h"

#include <Common/Math/CoreNumericTypes.h>

#include <Common/Memory/Containers/ForwardDeclarations/InlineVector.h>
#include <Common/Memory/Variant.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/ForwardDeclarations/Any.h>
#include <Common/Memory/ForwardDeclarations/AnyView.h>
#include <Common/Storage/Identifier.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Scripting
{
	struct ScriptTableCache;
}

namespace ngine::Scripting
{
	struct Token;

	struct TRIVIAL_ABI ScriptTableIdentifier : public TIdentifier<uint32, 12>
	{
		using Base = TIdentifier<uint32, 12>;
		using Base::operator=;
	protected:
		using Base::Base;
	};

	class TRIVIAL_ABI ManagedScriptTableIdentifier
	{
	public:
		constexpr ManagedScriptTableIdentifier() = default;
		ManagedScriptTableIdentifier(ScriptTableIdentifier identifier, ScriptTableCache& cache) noexcept;
		ManagedScriptTableIdentifier(const ManagedScriptTableIdentifier& other) noexcept;
		constexpr ManagedScriptTableIdentifier(ManagedScriptTableIdentifier&& other) noexcept;
		~ManagedScriptTableIdentifier();
		void Swap(ManagedScriptTableIdentifier& other) noexcept;
		constexpr operator ScriptTableIdentifier() const;
		constexpr bool operator==(const ManagedScriptTableIdentifier& other) const noexcept;
		ManagedScriptTableIdentifier& operator=(ManagedScriptTableIdentifier other) noexcept;
	private:
		ScriptTableIdentifier m_identifier;
		Optional<ScriptTableCache*> m_pCache;
	};

	using IntegerType = int32;
	using FloatType = float;

	struct TRIVIAL_ABI ScriptValue
	{
		using ValueType =
			Variant<nullptr_type, bool, IntegerType, FloatType, StringType, FunctionIdentifier, ManagedScriptTableIdentifier, ConstAnyView>;
		struct Hash
		{
			using is_transparent = void;

			size operator()(const ScriptValue& value) const noexcept;
		};
		static ScriptValue FromToken(const Token& token);

		explicit constexpr ScriptValue() noexcept;
		explicit constexpr ScriptValue(nullptr_type value) noexcept;
		explicit constexpr ScriptValue(bool value) noexcept;
		explicit constexpr ScriptValue(IntegerType value) noexcept;
		explicit constexpr ScriptValue(FloatType value) noexcept;
		explicit constexpr ScriptValue(StringType&& value) noexcept;
		explicit ScriptValue(FunctionIdentifier value) noexcept;
		explicit ScriptValue(ManagedScriptTableIdentifier value) noexcept;
		explicit ScriptValue(ConstAnyView value LIFETIME_BOUND) noexcept;
		explicit ScriptValue(ValueType&& value) noexcept;

		constexpr ScriptValue(const ScriptValue& other) noexcept;
		constexpr ScriptValue(ScriptValue&& other) noexcept;

		[[nodiscard]] constexpr const ValueType& Get() const;
		[[nodiscard]] constexpr ValueType& Get();

		[[nodiscard]] Any ToAny() const;

		void Swap(ScriptValue& other) noexcept;

		[[nodiscard]] ScriptValue operator+(const ScriptValue& other) const;
		ScriptValue operator-(const ScriptValue& other) const;
		[[nodiscard]] ScriptValue operator*(const ScriptValue& other) const;
		[[nodiscard]] ScriptValue operator/(const ScriptValue& other) const;

		constexpr bool operator==(const ScriptValue& other) const;
		constexpr bool operator!=(const ScriptValue& other) const;
		constexpr bool operator<(const ScriptValue& other) const;
		constexpr bool operator>(const ScriptValue& other) const;
		constexpr bool operator<=(const ScriptValue& other) const;
		constexpr bool operator>=(const ScriptValue& other) const;

		[[nodiscard]] ScriptValue operator&(const ScriptValue& other) const;
		[[nodiscard]] ScriptValue operator|(const ScriptValue& other) const;
		[[nodiscard]] ScriptValue operator^(const ScriptValue& other) const;

		// explicit constexpr operator bool() const;
		[[nodiscard]] ScriptValue operator!() const;
		[[nodiscard]] ScriptValue operator~() const;
		//! Equivalent to Lua's "truthy"
		[[nodiscard]] bool IsTruthy() const;
		//! Equivalent to Lua's "falsey"
		[[nodiscard]] bool IsFalsey() const;
		//! Equivalent to Lua's "Not"
		[[nodiscard]] ScriptValue Not() const;

		ScriptValue& operator=(ScriptValue other) noexcept;

		template<typename... Callbacks>
		auto Visit(Callbacks&&... callbacks)
		{
			return m_value.Visit(Forward<Callbacks>(callbacks)...);
		}
		template<typename... Callbacks>
		auto Visit(Callbacks&&... callbacks) const
		{
			return m_value.Visit(Forward<Callbacks>(callbacks)...);
		}

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;
	private:
		ValueType m_value;
	};
	using ScriptValues = InlineVector<ScriptValue, 1, uint8, uint8>;
}

namespace ngine::Scripting
{
	constexpr ManagedScriptTableIdentifier::ManagedScriptTableIdentifier(ManagedScriptTableIdentifier&& other) noexcept
		: m_identifier(Move(other.m_identifier))
		, m_pCache(Move(other.m_pCache))
	{
		other.m_identifier = ScriptTableIdentifier();
	}

	constexpr ManagedScriptTableIdentifier::operator ScriptTableIdentifier() const
	{
		return m_identifier;
	}

	constexpr bool ManagedScriptTableIdentifier::operator==(const ManagedScriptTableIdentifier& other) const noexcept
	{
		return m_identifier == other.m_identifier;
	}

	constexpr ScriptValue::ScriptValue() noexcept
	{
	}

	constexpr ScriptValue::ScriptValue(nullptr_type value) noexcept
		: m_value(value)
	{
	}

	constexpr ScriptValue::ScriptValue(bool value) noexcept
		: m_value(value)
	{
	}

	constexpr ScriptValue::ScriptValue(IntegerType value) noexcept
		: m_value(value)
	{
	}

	constexpr ScriptValue::ScriptValue(FloatType value) noexcept
		: m_value(value)
	{
	}

	constexpr ScriptValue::ScriptValue(StringType&& value) noexcept
		: m_value(Move(value))
	{
	}

	constexpr ScriptValue::ScriptValue(const ScriptValue& other) noexcept
		: m_value(other.Get())
	{
	}

	constexpr ScriptValue::ScriptValue(ScriptValue&& other) noexcept
		: m_value(Move(other.Get()))
	{
	}

	constexpr const ScriptValue::ValueType& ScriptValue::Get() const
	{
		return m_value;
	}

	constexpr ScriptValue::ValueType& ScriptValue::Get()
	{
		return m_value;
	}

	constexpr bool ScriptValue::operator==(const ScriptValue& other) const
	{
		return m_value == other.m_value;
	}

	constexpr bool ScriptValue::operator!=(const ScriptValue& other) const
	{
		return !(*this == other);
	}

	constexpr bool ScriptValue::operator<(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			if (m_value.Is<IntegerType>())
			{
				return m_value.GetExpected<IntegerType>() < other.Get().GetExpected<IntegerType>();
			}
			else if (m_value.Is<FloatType>())
			{
				return m_value.GetExpected<FloatType>() < other.Get().GetExpected<FloatType>();
			}
			else if (m_value.Is<StringType>())
			{
				return m_value.GetExpected<StringType>().GetView() < other.Get().GetExpected<StringType>().GetView();
			}
		}
		return false;
	}

	constexpr bool ScriptValue::operator>(const ScriptValue& other) const
	{
		return other < *this;
	}

	constexpr bool ScriptValue::operator<=(const ScriptValue& other) const
	{
		return !(*this > other);
	}

	constexpr bool ScriptValue::operator>=(const ScriptValue& other) const
	{
		return !(*this < other);
	}
}
