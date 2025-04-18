#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/Reflection/Type.h>

namespace ngine::Scripting
{
	enum class TokenType : uint8
	{
		// Single-character tokens
		Plus,
		Minus,
		Star,
		Slash,
		Exponent,
		Percent,
		Ampersand,
		Tilde,
		Bar,
		Pipe = Bar,
		Hashtag,
		Less,
		Greater,
		Equal,
		LeftParentheses,
		RightParentheses,
		LeftBrace,
		RightBrace,
		LeftBracket,
		RightBracket,
		Semicolon,
		Colon,
		Comma,
		Period,
		Exclamation,

		// Two character tokens
		ColonColon,
		SlashSlash,
		EqualEqual,
		NotEqual,
		LessEqual,
		GreaterEqual,
		LessLess,
		GreaterGreater,
		DotDot,

		// Threee character tokens
		DotDotDot,

		// Literals
		Identifier,
		String,
		Number,
		Integer,
		Float,
		Boolean,
		Component2D,
		Component3D,
		ComponentSoftReference,
		Vec2i,
		Vec2f,
		Vec2b,
		Vec3i,
		Vec3f,
		Vec3b,
		Vec4i,
		Vec4f,
		Vec4b,
		Rotation2D,
		Rotation3D,
		Transform2D,
		Transform3D,
		Asset,
		TextureAsset,
		Tag,
		Color,
		RenderStage,

		Abs,
		Acos,
		Asin,
		Atan,
		Atan2,
		Ceil,
		CubicRoot,
		Cos,
		Deg,
		e,
		Exp,
		Floor,
		Fract,
		InverseSqrt,
		Mod,
		Log,
		Log2,
		Log10,
		Max,
		Min,
		MultiplicativeInverse,
		Pi,
		Pi2,
		Power,
		Power2,
		Power10,
		Rad,
		Random,
		Round,
		Sign,
		SignNonZero,
		Sin,
		Sqrt,
		Tan,
		Truncate,
		AreNearlyEqual,

		Dot,
		Cross,
		Length,
		LengthSquared,
		InverseLength,
		Normalize,
		Distance,
		Project,
		Reflect,
		Refract,
		Inverse,

		Right,
		Forward,
		Up,
		Rotate,
		InverseRotate,
		Euler,

		All,
		Any,

		// Keywords
		And,
		Break,
		Do,
		Else,
		Elseif,
		End,
		False,
		For,
		Function,
		Goto,
		If,
		In,
		Local,
		Null,
		Not,
		Or,
		Repeat,
		Return,
		Then,
		True,
		Until,
		While,
		Circumflex,

		// Helper
		Eof,
		Invalid = Eof
	};

	//! Signifies a variable that represents an array
	enum class ArrayVariableType : uint8
	{
		Array
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Scripting::ArrayVariableType>
	{
		static constexpr auto Type =
			Reflection::Reflect<Scripting::ArrayVariableType>("81df429f-b567-48b6-ac9a-ad1a7749aa00"_guid, MAKE_UNICODE_LITERAL("Script Array"));
	};
}
