#pragma once

#include "Engine/Scripting/Parser/TokenType.h"
#include "Engine/Scripting/Parser/StringType.h"

#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/EnumFlags.h>
#include <Common/Reflection/DynamicTypeDefinition.h>
#include <Common/SourceLocation.h>

namespace ngine::Scripting
{
	struct Token
	{
		using Type = TokenType;

		Token() = default;
		Token(const Type type_)
			: type(type_)
		{
		}
		Token(const Type type_, const StringType name, const Guid identifier_, const SourceLocation sourceLocation_ = {"Unknown", 0, 0})
			: type(type_)
			, literal(name)
			, identifier(identifier_)
			, sourceLocation(sourceLocation_)
		{
		}
		Token(const StringType name)
			: type(Type::Identifier)
			, literal(name)
		{
		}

		[[nodiscard]] static constexpr Guid GuidFromScriptString(const StringType::ConstView name)
		{
			constexpr Guid scriptIdentifierBaseGuid = "da38b79e-2daf-467b-8895-f7c5628cce96"_guid;
			return Guid::FromString(name, scriptIdentifierBaseGuid);
		}

		[[nodiscard]] Any ToAny() const;

		[[nodiscard]] bool IsValid() const
		{
			return type != Type::Invalid;
		}
		[[nodiscard]] bool IsInvalid() const
		{
			return type == Type::Invalid;
		}

		[[nodiscard]] bool operator==(const Token& other) const
		{
			return type == other.type && literal == other.literal;
		}
		[[nodiscard]] bool operator!=(const Token& other) const
		{
			return !operator==(other);
		}

		Type type{Type::Invalid};
		StringType literal;
		Guid identifier;
		SourceLocation sourceLocation;
	};

	struct Type : public Variant<Reflection::TypeDefinition, Reflection::DynamicTypeDefinition>
	{
		using BaseType = Variant<Reflection::TypeDefinition, Reflection::DynamicTypeDefinition>;
		using BaseType::BaseType;
		using BaseType::operator=;
		Type(const Reflection::TypeDefinition& typeDefinition)
			: BaseType(typeDefinition)
		{
		}
		Type(Reflection::TypeDefinition&& typeDefinition)
			: BaseType(Forward<Reflection::TypeDefinition>(typeDefinition))
		{
		}
		Type(const Reflection::DynamicTypeDefinition& typeDefinition)
			: BaseType(typeDefinition)
		{
		}
		Type(Reflection::DynamicTypeDefinition&& typeDefinition)
			: BaseType(Forward<Reflection::DynamicTypeDefinition>(typeDefinition))
		{
		}
		Type& operator=(Reflection::TypeDefinition&& typeDefinition)
		{
			static_cast<BaseType&>(*this) = Forward<Reflection::TypeDefinition>(typeDefinition);
			return *this;
		}
		Type& operator=(Reflection::DynamicTypeDefinition&& typeDefinition)
		{
			static_cast<BaseType&>(*this) = Forward<Reflection::DynamicTypeDefinition>(typeDefinition);
			return *this;
		}

		[[nodiscard]] bool IsValid() const
		{
			return BaseType::HasValue();
		}

		[[nodiscard]] Reflection::TypeDefinitionType GetType() const
		{
			return BaseType::Visit(
				[](const Reflection::TypeDefinition)
				{
					return Reflection::TypeDefinitionType::Native;
				},
				[](const Reflection::DynamicTypeDefinition& typeDefinition)
				{
					return typeDefinition.GetType();
				},
				[]()
				{
					return Reflection::TypeDefinitionType::Invalid;
				}
			);
		}

		//! Checks whether this type is the specified native type
		template<typename OtherType>
		[[nodiscard]] PURE_STATICS bool Is() const
		{
			if constexpr (BaseType::ContainsType<OtherType>())
			{
				if (BaseType::Is<OtherType>())
				{
					return true;
				}
			}

			return BaseType::Visit(
				[](const Reflection::TypeDefinition typeDefinition) -> bool
				{
					return typeDefinition.Is<OtherType>();
				},
				[](const Reflection::DynamicTypeDefinition) -> bool
				{
					return false;
				},
				[]() -> bool
				{
					return false;
				}
			);
		}

		//! Whether this type is the exact type, or can support it as a value (i.e. Variant)
		template<typename OtherType>
		[[nodiscard]] PURE_STATICS bool IsOrSupports() const
		{
			return BaseType::Visit(
				[](const Reflection::TypeDefinition typeDefinition) -> bool
				{
					return typeDefinition.Is<OtherType>();
				},
				[](const Reflection::DynamicTypeDefinition& typeDefinition) -> bool
				{
					return typeDefinition.SupportsType<OtherType>();
				},
				[]() -> bool
				{
					return false;
				}
			);
		}

		[[nodiscard]] PURE_STATICS operator Reflection::TypeDefinition() const
		{
			return BaseType::Visit(
				[](const Reflection::TypeDefinition typeDefinition) -> Reflection::TypeDefinition
				{
					return typeDefinition;
				},
				[](const Reflection::DynamicTypeDefinition& typeDefinition) -> Reflection::TypeDefinition
				{
					return typeDefinition;
				},
				[]() -> Reflection::TypeDefinition
				{
					return {};
				}
			);
		}

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;
	};

	using Types = InlineVector<Type, 1>;

	struct VariableToken : public Token
	{
		VariableToken() = default;
		VariableToken(Token&& token, Types&& types)
			: Token(Forward<Token>(token))
			, m_types(Forward<Types>(types))
		{
		}

		[[nodiscard]] bool operator==(const VariableToken& other) const
		{
			return Token::operator==(other) && m_types.GetView() == other.m_types.GetView();
		}
		[[nodiscard]] bool operator!=(const VariableToken& other) const
		{
			return !operator==(other);
		}

		Types m_types;
	};
}
