#pragma once

#include "TokenType.h"

#include <Common/Memory/Containers/Array.h>

namespace ngine::Scripting
{
	namespace Internal
	{
		inline static constexpr Array<StringType::ConstView, ngine::size(TokenType::Eof) + 1> TokenTypeSymbols = {
			SCRIPT_STRING_LITERAL("+"),
			SCRIPT_STRING_LITERAL("-"),
			SCRIPT_STRING_LITERAL("*"),
			SCRIPT_STRING_LITERAL("/"),
			SCRIPT_STRING_LITERAL("^"),
			SCRIPT_STRING_LITERAL("%"),
			SCRIPT_STRING_LITERAL("&"),
			SCRIPT_STRING_LITERAL("~"),
			SCRIPT_STRING_LITERAL("|"),
			SCRIPT_STRING_LITERAL("#"),
			SCRIPT_STRING_LITERAL("<"),
			SCRIPT_STRING_LITERAL(">"),
			SCRIPT_STRING_LITERAL("="),
			SCRIPT_STRING_LITERAL("("),
			SCRIPT_STRING_LITERAL(")"),
			SCRIPT_STRING_LITERAL("{"),
			SCRIPT_STRING_LITERAL("}"),
			SCRIPT_STRING_LITERAL("["),
			SCRIPT_STRING_LITERAL("]"),
			SCRIPT_STRING_LITERAL(";"),
			SCRIPT_STRING_LITERAL(":"),
			SCRIPT_STRING_LITERAL(","),
			SCRIPT_STRING_LITERAL("."),
			SCRIPT_STRING_LITERAL("!"),
			SCRIPT_STRING_LITERAL("::"),
			SCRIPT_STRING_LITERAL("//"),
			SCRIPT_STRING_LITERAL("=="),
			SCRIPT_STRING_LITERAL("~="),
			SCRIPT_STRING_LITERAL("<="),
			SCRIPT_STRING_LITERAL(">="),
			SCRIPT_STRING_LITERAL("<<"),
			SCRIPT_STRING_LITERAL(">>"),
			SCRIPT_STRING_LITERAL(".."),
			SCRIPT_STRING_LITERAL("..."),
			SCRIPT_STRING_LITERAL("identifier"),
			SCRIPT_STRING_LITERAL("string"),
			SCRIPT_STRING_LITERAL("number"),
			SCRIPT_STRING_LITERAL("integer"),
			SCRIPT_STRING_LITERAL("float"),
			SCRIPT_STRING_LITERAL("boolean"),
			SCRIPT_STRING_LITERAL("component2d"),
			SCRIPT_STRING_LITERAL("component3d"),
			SCRIPT_STRING_LITERAL("component_soft_ref"),
			SCRIPT_STRING_LITERAL("vec2i"),
			SCRIPT_STRING_LITERAL("vec2f"),
			SCRIPT_STRING_LITERAL("vec2b"),
			SCRIPT_STRING_LITERAL("vec3i"),
			SCRIPT_STRING_LITERAL("vec3f"),
			SCRIPT_STRING_LITERAL("vec3b"),
			SCRIPT_STRING_LITERAL("vec4i"),
			SCRIPT_STRING_LITERAL("vec4f"),
			SCRIPT_STRING_LITERAL("vec4b"),
			SCRIPT_STRING_LITERAL("rotation2d"),
			SCRIPT_STRING_LITERAL("rotation3d"),
			SCRIPT_STRING_LITERAL("transform2d"),
			SCRIPT_STRING_LITERAL("transform3d"),
			SCRIPT_STRING_LITERAL("asset"),
			SCRIPT_STRING_LITERAL("texture"),
			SCRIPT_STRING_LITERAL("tag"),
			SCRIPT_STRING_LITERAL("color"),
			SCRIPT_STRING_LITERAL("stage"),

			SCRIPT_STRING_LITERAL("abs"),
			SCRIPT_STRING_LITERAL("acos"),
			SCRIPT_STRING_LITERAL("asin"),
			SCRIPT_STRING_LITERAL("atan"),
			SCRIPT_STRING_LITERAL("atan2"),
			SCRIPT_STRING_LITERAL("ceil"),
			SCRIPT_STRING_LITERAL("cbrt"),
			SCRIPT_STRING_LITERAL("cos"),
			SCRIPT_STRING_LITERAL("deg"),
			SCRIPT_STRING_LITERAL("e"),
			SCRIPT_STRING_LITERAL("exp"),
			SCRIPT_STRING_LITERAL("floor"),
			SCRIPT_STRING_LITERAL("fract"),
			SCRIPT_STRING_LITERAL("isqrt"),
			SCRIPT_STRING_LITERAL("fmod"),
			SCRIPT_STRING_LITERAL("log"),
			SCRIPT_STRING_LITERAL("log2"),
			SCRIPT_STRING_LITERAL("log10"),
			SCRIPT_STRING_LITERAL("max"),
			SCRIPT_STRING_LITERAL("min"),
			SCRIPT_STRING_LITERAL("rcp"),
			SCRIPT_STRING_LITERAL("pi"),
			SCRIPT_STRING_LITERAL("pi2"),
			SCRIPT_STRING_LITERAL("pow"),
			SCRIPT_STRING_LITERAL("pow2"),
			SCRIPT_STRING_LITERAL("pow10"),
			SCRIPT_STRING_LITERAL("rad"),
			SCRIPT_STRING_LITERAL("random"),
			SCRIPT_STRING_LITERAL("round"),
			SCRIPT_STRING_LITERAL("sign"),
			SCRIPT_STRING_LITERAL("signnonzero"),
			SCRIPT_STRING_LITERAL("sin"),
			SCRIPT_STRING_LITERAL("sqrt"),
			SCRIPT_STRING_LITERAL("tan"),
			SCRIPT_STRING_LITERAL("trunc"),
			SCRIPT_STRING_LITERAL("isclose"),

			SCRIPT_STRING_LITERAL("dot"),
			SCRIPT_STRING_LITERAL("cross"),
			SCRIPT_STRING_LITERAL("length"),
			SCRIPT_STRING_LITERAL("length_squared"),
			SCRIPT_STRING_LITERAL("inverse_length"),
			SCRIPT_STRING_LITERAL("normalize"),
			SCRIPT_STRING_LITERAL("distance"),
			SCRIPT_STRING_LITERAL("project"),
			SCRIPT_STRING_LITERAL("reflect"),
			SCRIPT_STRING_LITERAL("refract"),
			SCRIPT_STRING_LITERAL("inverse"),

			SCRIPT_STRING_LITERAL("right"),
			SCRIPT_STRING_LITERAL("forward"),
			SCRIPT_STRING_LITERAL("up"),
			SCRIPT_STRING_LITERAL("rotate"),
			SCRIPT_STRING_LITERAL("inverse_rotate"),
			SCRIPT_STRING_LITERAL("euler"),

			SCRIPT_STRING_LITERAL("all"),
			SCRIPT_STRING_LITERAL("any"),

			SCRIPT_STRING_LITERAL("and"),
			SCRIPT_STRING_LITERAL("break"),
			SCRIPT_STRING_LITERAL("do"),
			SCRIPT_STRING_LITERAL("else"),
			SCRIPT_STRING_LITERAL("elseif"),
			SCRIPT_STRING_LITERAL("end"),
			SCRIPT_STRING_LITERAL("false"),
			SCRIPT_STRING_LITERAL("for"),
			SCRIPT_STRING_LITERAL("function"),
			SCRIPT_STRING_LITERAL("goto"),
			SCRIPT_STRING_LITERAL("if"),
			SCRIPT_STRING_LITERAL("in"),
			SCRIPT_STRING_LITERAL("local"),
			SCRIPT_STRING_LITERAL("nil"),
			SCRIPT_STRING_LITERAL("not"),
			SCRIPT_STRING_LITERAL("or"),
			SCRIPT_STRING_LITERAL("repeat"),
			SCRIPT_STRING_LITERAL("return"),
			SCRIPT_STRING_LITERAL("then"),
			SCRIPT_STRING_LITERAL("true"),
			SCRIPT_STRING_LITERAL("until"),
			SCRIPT_STRING_LITERAL("while"),
			SCRIPT_STRING_LITERAL("xor"),
			SCRIPT_STRING_LITERAL("EOF")
		};
	}

	[[nodiscard]] constexpr StringType::ConstView GetTokenString(TokenType tokenType)
	{
		return Internal::TokenTypeSymbols[uint8(tokenType)];
	}
	[[nodiscard]] constexpr TokenType GetTokenType(const StringType::ConstView identifier)
	{
		for (const StringType::ConstView& tokenIdentifier : Internal::TokenTypeSymbols)
		{
			if (tokenIdentifier == identifier)
			{
				return static_cast<TokenType>(Internal::TokenTypeSymbols.GetIteratorIndex(&tokenIdentifier));
			}
		}
		return TokenType::Invalid;
	}

	[[nodiscard]] constexpr StringCharType GetTokenCharacter(TokenType tokenType, uint8 index = 0)
	{
		return StringCharType(GetTokenString(tokenType)[index]);
	}
}
