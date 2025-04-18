#include "Engine/Scripting/Parser/ScriptValue.h"

#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/Interpreter/ScriptTableCache.h"

#include <Common/Math/Hash.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Swap.h>
#include <Common/Storage/Identifier.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.h>
#include <Common/System/Query.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Scripting
{
	ManagedScriptTableIdentifier::ManagedScriptTableIdentifier(ScriptTableIdentifier identifier, ScriptTableCache& cache) noexcept
		: m_identifier(identifier)
		, m_pCache(cache)
	{
		if (m_pCache.IsValid())
		{
			[[maybe_unused]] const int32 count = m_pCache->AddReference(m_identifier);
		}
	}

	ManagedScriptTableIdentifier::ManagedScriptTableIdentifier(const ManagedScriptTableIdentifier& other) noexcept
		: m_identifier(other.m_identifier)
		, m_pCache(other.m_pCache)
	{
		if (m_pCache.IsValid())
		{
			[[maybe_unused]] const int32 count = m_pCache->AddReference(m_identifier);
		}
	}

	ManagedScriptTableIdentifier::~ManagedScriptTableIdentifier()
	{
		if (m_identifier.IsValid() && m_pCache.IsValid())
		{
			[[maybe_unused]] const int32 count = m_pCache->RemoveReference(m_identifier);
		}
	}

	void ManagedScriptTableIdentifier::Swap(ManagedScriptTableIdentifier& other) noexcept
	{
		ngine::Swap(m_identifier, other.m_identifier);
		ngine::Swap(m_pCache, other.m_pCache);
	}

	ManagedScriptTableIdentifier& ManagedScriptTableIdentifier::operator=(ManagedScriptTableIdentifier other) noexcept
	{
		Swap(other);
		return *this;
	}

	size ScriptValue::Hash::operator()(const ScriptValue& value) const noexcept
	{
		size hash = Math::Hash(value.Get().GetActiveIndex());
		return value.Visit(
			[hash](nullptr_type) -> size
			{
				return hash;
			},
			[hash](bool value) -> size
			{
				return Math::CombineHash(hash, Math::Hash(value));
			},
			[hash](const IntegerType value) -> size
			{
				return Math::CombineHash(hash, Math::Hash(value));
			},
			[hash](const FloatType value) -> size
			{
				return Math::CombineHash(hash, Math::Hash(value));
			},
			[hash](const StringType& value) -> size
			{
				return Math::CombineHash(hash, StringType::Hash()(value.GetView()));
			},
			[hash](const FunctionIdentifier value) -> size
			{
				return Math::CombineHash(hash, Math::Hash(value.GetValue()));
			},
			[hash](const ScriptTableIdentifier value) -> size
			{
				return Math::CombineHash(hash, Math::Hash(value.GetValue()));
			},
			[hash](const ConstAnyView value) mutable -> size
			{
				// Note: We don't do deep comparison here
			  // Should delete the hash operator when we don't use script tables / maps anymore
				hash = Math::CombineHash(hash, Reflection::TypeDefinition::Hash()(value.GetTypeDefinition()));
				const ConstByteView data = value.GetByteView();
				hash = Math::CombineHash(hash, Math::Hash(ArrayView<const ByteType, size>{data.GetData(), data.GetDataSize()}));
				return hash;
			},
			[]() -> size
			{
				ExpectUnreachable();
			}
		);
	}

	void ScriptValue::Swap(ScriptValue& other) noexcept
	{
		ngine::Swap(m_value, other.m_value);
	}

	Any ScriptValue::ToAny() const
	{
		return m_value.Visit(
			[](nullptr_type) -> Any
			{
				return nullptr;
			},
			[](bool value) -> Any
			{
				return value;
			},
			[](IntegerType value) -> Any
			{
				return value;
			},
			[](FloatType value) -> Any
			{
				return value;
			},
			[](const StringType& value) -> Any
			{
				return Any(StringType(value));
			},
			[](const FunctionIdentifier value) -> Any
			{
				return Any(value);
			},
			[](const ScriptTableIdentifier value) -> Any
			{
				return Any(value);
			},
			[](const ConstAnyView value) -> Any
			{
				return Any(value);
			},
			[]() -> Any
			{
				ExpectUnreachable();
			}
		);
	}

	ScriptValue::ScriptValue(FunctionIdentifier value) noexcept
		: m_value(Move(value))
	{
	}

	ScriptValue::ScriptValue(ManagedScriptTableIdentifier value) noexcept
		: m_value(Move(value))
	{
	}

	[[nodiscard]] static ScriptValue Convert(const ConstAnyView value)
	{
		if (value.Is<nullptr_type>())
		{
			return ScriptValue(nullptr);
		}
		if (value.Is<bool>())
		{
			return ScriptValue(value.GetExpected<bool>());
		}
		else if (value.Is<int32>())
		{
			return ScriptValue(IntegerType(value.GetExpected<int32>()));
		}
		else if (value.Is<int64>())
		{
			return ScriptValue(IntegerType(value.GetExpected<int64>()));
		}
		else if (value.Is<float>())
		{
			return ScriptValue(FloatType(value.GetExpected<float>()));
		}
		else if (value.Is<double>())
		{
			return ScriptValue(FloatType(value.GetExpected<double>()));
		}
		else if (value.Is<String>())
		{
			return ScriptValue(StringType(value.GetExpected<String>()));
		}
		else if (value.Is<UnicodeString>())
		{
			return ScriptValue(StringType(value.GetExpected<UnicodeString>()));
		}
		else if (value.Is<Guid>())
		{
			return ScriptValue(StringType(value.GetExpected<Guid>().ToString()));
		}
		else if (value.Is<FunctionIdentifier>())
		{
			return ScriptValue(FunctionIdentifier(value.GetExpected<FunctionIdentifier>()));
		}
		else
		{
			return ScriptValue{ScriptValue::ValueType{ConstAnyView{value}}};
		}
	}

	ScriptValue::ScriptValue(const ConstAnyView value) noexcept
		: ScriptValue(Convert(value))
	{
	}

	ScriptValue::ScriptValue(ValueType&& value) noexcept
		: m_value{Forward<ValueType>(value)}
	{
	}

	ScriptValue ScriptValue::operator+(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](bool) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value + other.m_value.GetExpected<IntegerType>()};
				},
				[&other](FloatType value) -> ScriptValue
				{
					return ScriptValue{value + other.m_value.GetExpected<FloatType>()};
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator-(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](bool) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value - other.m_value.GetExpected<IntegerType>()};
				},
				[&other](FloatType value) -> ScriptValue
				{
					return ScriptValue{value - other.m_value.GetExpected<FloatType>()};
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing subtraction");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator*(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](bool) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value * other.m_value.GetExpected<IntegerType>()};
				},
				[&other](FloatType value) -> ScriptValue
				{
					return ScriptValue{value * other.m_value.GetExpected<FloatType>()};
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing multiplication");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator/(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](bool) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value / other.m_value.GetExpected<IntegerType>()};
				},
				[&other](FloatType value) -> ScriptValue
				{
					return ScriptValue{value / other.m_value.GetExpected<FloatType>()};
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing division");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator&(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](bool value) -> ScriptValue
				{
					return ScriptValue{value & other.m_value.GetExpected<bool>()};
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value & other.m_value.GetExpected<IntegerType>()};
				},
				[](FloatType) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing AND");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator|(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](bool value) -> ScriptValue
				{
					return ScriptValue{value | other.m_value.GetExpected<bool>()};
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value | other.m_value.GetExpected<IntegerType>()};
				},
				[](FloatType) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing OR");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator^(const ScriptValue& other) const
	{
		if (m_value.GetActiveIndex() == other.Get().GetActiveIndex())
		{
			return m_value.Visit(
				[](nullptr_type) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[&other](bool value) -> ScriptValue
				{
					return ScriptValue{value ^ other.m_value.GetExpected<bool>()};
				},
				[&other](IntegerType value) -> ScriptValue
				{
					return ScriptValue{value ^ other.m_value.GetExpected<IntegerType>()};
				},
				[](FloatType) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const StringType&) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const FunctionIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ScriptTableIdentifier) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[](const ConstAnyView) -> ScriptValue
				{
					ExpectUnreachable();
				},
				[]() -> ScriptValue
				{
					ExpectUnreachable();
				}
			);
		}
		Assert(false, "Failing OR");
		return ScriptValue();
	}

	ScriptValue ScriptValue::operator!() const
	{
		return m_value.Visit(
			[](nullptr_type) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](bool value) -> ScriptValue
			{
				return ScriptValue{!value};
			},
			[](IntegerType value) -> ScriptValue
			{
				return ScriptValue{value != 0};
			},
			[](FloatType value) -> ScriptValue
			{
				return ScriptValue{value != 0.f};
			},
			[](const StringType&) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const FunctionIdentifier) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const ScriptTableIdentifier) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const ConstAnyView) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[]() -> ScriptValue
			{
				ExpectUnreachable();
			}
		);
	}

	ScriptValue ScriptValue::Not() const
	{
		return m_value.Visit(
			[](nullptr_type) -> ScriptValue
			{
				return ScriptValue{true};
			},
			[](bool value) -> ScriptValue
			{
				return ScriptValue{!value};
			},
			[](IntegerType) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[](FloatType) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[](const StringType&) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[](const FunctionIdentifier) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[](const ScriptTableIdentifier) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[](const ConstAnyView) -> ScriptValue
			{
				return ScriptValue{false};
			},
			[]() -> ScriptValue
			{
				ExpectUnreachable();
			}
		);
	}

	ScriptValue ScriptValue::operator~() const
	{
		return m_value.Visit(
			[](nullptr_type) -> ScriptValue
			{
				return ScriptValue{true};
			},
			[](bool value) -> ScriptValue
			{
				return ScriptValue{!value};
			},
			[](IntegerType value) -> ScriptValue
			{
				return ScriptValue{~value};
			},
			[](FloatType) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const StringType&) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const FunctionIdentifier) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const ScriptTableIdentifier) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[](const ConstAnyView) -> ScriptValue
			{
				ExpectUnreachable();
			},
			[]() -> ScriptValue
			{
				ExpectUnreachable();
			}
		);
	}

	bool ScriptValue::IsTruthy() const
	{
		if (m_value.Is<nullptr_type>())
		{
			return false;
		}
		if (m_value.Is<bool>())
		{
			return m_value.GetExpected<bool>();
		}
		return true;
	}

	bool ScriptValue::IsFalsey() const
	{
		if (m_value.Is<nullptr_type>())
		{
			return true;
		}
		if (m_value.Is<bool>())
		{
			return !m_value.GetExpected<bool>();
		}
		return false;
	}

	ScriptValue ScriptValue::FromToken(const Token& token)
	{
		switch (token.type)
		{
			case TokenType::Number:
			{
				if (token.literal.Contains(StringType::CharType('.')))
				{
					return ScriptValue(FloatType(token.literal.GetView().ToDouble()));
				}
				else
				{
					return ScriptValue(token.literal.GetView().ToIntegral<IntegerType>());
				}
			}
			case TokenType::Float:
			{
				return ScriptValue(FloatType(token.literal.GetView().ToDouble()));
			}
			case TokenType::Integer:
			{
				return ScriptValue(token.literal.GetView().ToIntegral<IntegerType>());
			}
			case TokenType::String:
			{
				// Remove first and last '"' or ','
				StringType::ConstView stringView = token.literal.GetView().GetSubstring(1, token.literal.GetSize() - 2);
				return ScriptValue(StringType(stringView));
			}
			default:
				return ScriptValue();
		}
	}

	ScriptValue& ScriptValue::operator=(ScriptValue other) noexcept
	{
		Swap(other);
		return *this;
	}

	bool ScriptValue::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<Serialization::Reader> valueReader = reader.FindSerializer("value"))
		{
			if (valueReader->GetValue().IsNull())
			{
				*this = ScriptValue{nullptr};
				return true;
			}
			else if (valueReader->GetValue().IsBool())
			{
				*this = ScriptValue{*valueReader->ReadInPlace<bool>()};
				return true;
			}
			else if (valueReader->GetValue().IsDouble())
			{
				*this = ScriptValue{*valueReader->ReadInPlace<FloatType>()};
				return true;
			}
			else if (valueReader->GetValue().IsNumber())
			{
				*this = ScriptValue{*valueReader->ReadInPlace<IntegerType>()};
				return true;
			}
			else if (valueReader->GetValue().IsString())
			{
				*this = ScriptValue{StringType{*valueReader->ReadInPlace<ConstStringView>()}};
				return true;
			}
			else
			{
				return false;
			}
		}
		else if (const Optional<Serialization::Reader> functionReader = reader.FindSerializer("function"))
		{
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			*this = ScriptValue{reflectionRegistry.FindFunctionIdentifier(*functionReader->ReadInPlace<Guid>())};
			return true;
		}
		else if (const Optional<Serialization::Reader> anyReader = reader.FindSerializer("any"))
		{
			Assert(false, "Can't deserialize any script values");
			return false;
		}
		else
		{
			return false;
		}
	}

	bool ScriptValue::Serialize(Serialization::Writer writer) const
	{
		return Visit(
			[writer](nullptr_type) mutable -> bool
			{
				const nullptr_type value = nullptr;
				return writer.Serialize("value", value);
			},
			[writer](const bool value) mutable -> bool
			{
				return writer.Serialize("value", value);
			},
			[writer](const IntegerType value) mutable -> bool
			{
				return writer.Serialize("value", value);
			},
			[writer](const FloatType value) mutable -> bool
			{
				return writer.Serialize("value", value);
			},
			[writer](const StringType& string) mutable -> bool
			{
				return writer.Serialize("value", string);
			},
			[writer](const FunctionIdentifier functionIdentifier) mutable -> bool
			{
				Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
				return writer.Serialize("function", reflectionRegistry.FindFunctionGuid(functionIdentifier));
			},
			[](ManagedScriptTableIdentifier) -> bool
			{
				Assert(false, "Not supported to AST");
				return false;
			},
			[writer](const ConstAnyView any) mutable -> bool
			{
				return writer.Serialize("any", any);
			},
			[]() -> bool
			{
				ExpectUnreachable();
			}
		);
	}
}
