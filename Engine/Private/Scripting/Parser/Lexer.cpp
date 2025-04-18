#include "Engine/Scripting/Parser/Lexer.h"
#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/Parser/TokenTypeLiterals.h"

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Reflection/GenericType.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>

namespace ngine::Scripting
{
	Lexer::Lexer()
		: m_knownKeywordsMap{
				KnownKeywordMapPair{GetTokenString(TokenType::And), TokenType::And},
				KnownKeywordMapPair{GetTokenString(TokenType::Break), TokenType::Break},
				KnownKeywordMapPair{GetTokenString(TokenType::Do), TokenType::Do},
				KnownKeywordMapPair{GetTokenString(TokenType::Else), TokenType::Else},
				KnownKeywordMapPair{GetTokenString(TokenType::Elseif), TokenType::Elseif},
				KnownKeywordMapPair{GetTokenString(TokenType::End), TokenType::End},
				KnownKeywordMapPair{GetTokenString(TokenType::False), TokenType::False},
				KnownKeywordMapPair{GetTokenString(TokenType::For), TokenType::For},
				KnownKeywordMapPair{GetTokenString(TokenType::Function), TokenType::Function},
				KnownKeywordMapPair{GetTokenString(TokenType::Goto), TokenType::Goto},
				KnownKeywordMapPair{GetTokenString(TokenType::If), TokenType::If},
				KnownKeywordMapPair{GetTokenString(TokenType::In), TokenType::In},
				KnownKeywordMapPair{GetTokenString(TokenType::Local), TokenType::Local},
				KnownKeywordMapPair{GetTokenString(TokenType::Null), TokenType::Null},
				KnownKeywordMapPair{GetTokenString(TokenType::Not), TokenType::Not},
				KnownKeywordMapPair{GetTokenString(TokenType::Or), TokenType::Or},
				KnownKeywordMapPair{GetTokenString(TokenType::Repeat), TokenType::Repeat},
				KnownKeywordMapPair{GetTokenString(TokenType::Return), TokenType::Return},
				KnownKeywordMapPair{GetTokenString(TokenType::Then), TokenType::Then},
				KnownKeywordMapPair{GetTokenString(TokenType::True), TokenType::True},
				KnownKeywordMapPair{GetTokenString(TokenType::Until), TokenType::Until},
				KnownKeywordMapPair{GetTokenString(TokenType::While), TokenType::While},
				KnownKeywordMapPair{GetTokenString(TokenType::Component2D), TokenType::Component2D},
				KnownKeywordMapPair{GetTokenString(TokenType::Component3D), TokenType::Component3D},
				KnownKeywordMapPair{GetTokenString(TokenType::ComponentSoftReference), TokenType::ComponentSoftReference},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec2i), TokenType::Vec2i},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec2f), TokenType::Vec2f},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec2b), TokenType::Vec2b},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec3i), TokenType::Vec3i},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec3f), TokenType::Vec3f},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec3b), TokenType::Vec3b},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec4i), TokenType::Vec4i},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec4f), TokenType::Vec4f},
				KnownKeywordMapPair{GetTokenString(TokenType::Vec4b), TokenType::Vec4b},
				KnownKeywordMapPair{GetTokenString(TokenType::Rotation2D), TokenType::Rotation2D},
				KnownKeywordMapPair{GetTokenString(TokenType::Rotation3D), TokenType::Rotation3D},
				KnownKeywordMapPair{GetTokenString(TokenType::Transform2D), TokenType::Transform2D},
				KnownKeywordMapPair{GetTokenString(TokenType::Transform3D), TokenType::Transform3D},
				KnownKeywordMapPair{GetTokenString(TokenType::Asset), TokenType::Asset},
				KnownKeywordMapPair{GetTokenString(TokenType::TextureAsset), TokenType::TextureAsset},
				KnownKeywordMapPair{GetTokenString(TokenType::Tag), TokenType::Tag},
				KnownKeywordMapPair{GetTokenString(TokenType::Color), TokenType::Color},
				KnownKeywordMapPair{GetTokenString(TokenType::RenderStage), TokenType::RenderStage},

				KnownKeywordMapPair{GetTokenString(TokenType::Abs), TokenType::Abs},
				KnownKeywordMapPair{GetTokenString(TokenType::Acos), TokenType::Acos},
				KnownKeywordMapPair{GetTokenString(TokenType::Asin), TokenType::Asin},
				KnownKeywordMapPair{GetTokenString(TokenType::Atan), TokenType::Atan},
				KnownKeywordMapPair{GetTokenString(TokenType::Atan2), TokenType::Atan2},
				KnownKeywordMapPair{GetTokenString(TokenType::Ceil), TokenType::Ceil},
				KnownKeywordMapPair{GetTokenString(TokenType::CubicRoot), TokenType::CubicRoot},
				KnownKeywordMapPair{GetTokenString(TokenType::Cos), TokenType::Cos},
				KnownKeywordMapPair{GetTokenString(TokenType::Deg), TokenType::Deg},
				KnownKeywordMapPair{GetTokenString(TokenType::e), TokenType::e},
				KnownKeywordMapPair{GetTokenString(TokenType::Exp), TokenType::Exp},
				KnownKeywordMapPair{GetTokenString(TokenType::Floor), TokenType::Floor},
				KnownKeywordMapPair{GetTokenString(TokenType::Fract), TokenType::Fract},
				KnownKeywordMapPair{GetTokenString(TokenType::InverseSqrt), TokenType::InverseSqrt},
				KnownKeywordMapPair{GetTokenString(TokenType::Mod), TokenType::Mod},
				KnownKeywordMapPair{GetTokenString(TokenType::Log), TokenType::Log},
				KnownKeywordMapPair{GetTokenString(TokenType::Log2), TokenType::Log2},
				KnownKeywordMapPair{GetTokenString(TokenType::Log10), TokenType::Log10},
				KnownKeywordMapPair{GetTokenString(TokenType::Max), TokenType::Max},
				KnownKeywordMapPair{GetTokenString(TokenType::Min), TokenType::Min},
				KnownKeywordMapPair{GetTokenString(TokenType::MultiplicativeInverse), TokenType::MultiplicativeInverse},
				KnownKeywordMapPair{GetTokenString(TokenType::Pi), TokenType::Pi},
				KnownKeywordMapPair{GetTokenString(TokenType::Pi2), TokenType::Pi2},
				KnownKeywordMapPair{GetTokenString(TokenType::Power), TokenType::Power},
				KnownKeywordMapPair{GetTokenString(TokenType::Power2), TokenType::Power2},
				KnownKeywordMapPair{GetTokenString(TokenType::Power10), TokenType::Power10},
				KnownKeywordMapPair{GetTokenString(TokenType::Rad), TokenType::Rad},
				KnownKeywordMapPair{GetTokenString(TokenType::Random), TokenType::Random},
				KnownKeywordMapPair{GetTokenString(TokenType::Round), TokenType::Round},
				KnownKeywordMapPair{GetTokenString(TokenType::Sign), TokenType::Sign},
				KnownKeywordMapPair{GetTokenString(TokenType::SignNonZero), TokenType::SignNonZero},
				KnownKeywordMapPair{GetTokenString(TokenType::Sin), TokenType::Sin},
				KnownKeywordMapPair{GetTokenString(TokenType::Sqrt), TokenType::Sqrt},
				KnownKeywordMapPair{GetTokenString(TokenType::Tan), TokenType::Tan},
				KnownKeywordMapPair{GetTokenString(TokenType::Truncate), TokenType::Truncate},
				KnownKeywordMapPair{GetTokenString(TokenType::AreNearlyEqual), TokenType::AreNearlyEqual},
				KnownKeywordMapPair{GetTokenString(TokenType::Dot), TokenType::Dot},
				KnownKeywordMapPair{GetTokenString(TokenType::Cross), TokenType::Cross},
				KnownKeywordMapPair{GetTokenString(TokenType::Distance), TokenType::Distance},
				KnownKeywordMapPair{GetTokenString(TokenType::Length), TokenType::Length},
				KnownKeywordMapPair{GetTokenString(TokenType::LengthSquared), TokenType::LengthSquared},
				KnownKeywordMapPair{GetTokenString(TokenType::InverseLength), TokenType::InverseLength},
				KnownKeywordMapPair{GetTokenString(TokenType::Normalize), TokenType::Normalize},
				KnownKeywordMapPair{GetTokenString(TokenType::Project), TokenType::Project},
				KnownKeywordMapPair{GetTokenString(TokenType::Reflect), TokenType::Reflect},
				KnownKeywordMapPair{GetTokenString(TokenType::Refract), TokenType::Refract},
				KnownKeywordMapPair{GetTokenString(TokenType::Inverse), TokenType::Inverse},

				KnownKeywordMapPair{GetTokenString(TokenType::Right), TokenType::Right},
				KnownKeywordMapPair{GetTokenString(TokenType::Forward), TokenType::Forward},
				KnownKeywordMapPair{GetTokenString(TokenType::Up), TokenType::Up},
				KnownKeywordMapPair{GetTokenString(TokenType::Rotate), TokenType::Rotate},
				KnownKeywordMapPair{GetTokenString(TokenType::InverseRotate), TokenType::InverseRotate},
				KnownKeywordMapPair{GetTokenString(TokenType::Euler), TokenType::Euler},

				KnownKeywordMapPair{GetTokenString(TokenType::Any), TokenType::Any},
				KnownKeywordMapPair{GetTokenString(TokenType::All), TokenType::All},
			}
	{
	}

	bool Lexer::ScanTokens(StringType::ConstView string, TokenListType& tokens)
	{
		m_pString = string;
		m_pTokens = tokens;
		Memory::Set(&m_state, sizeof(m_state), 0);

		while (!ReachedEnd())
		{
			m_state.start = m_state.current;
			ScanToken();
		}
		AddToken(TokenType::Eof);

		return m_state.flags.IsNotSet(Flags::Error);
	}

	void Lexer::SetSourceFilePath(const IO::PathView sourceFilePath)
	{
		m_sourceFilePath = ngine::String{sourceFilePath.GetStringView()};
	}

	void Lexer::ScanToken()
	{
		const StringCharType c = Advance();
		switch (c)
		{
			case GetTokenCharacter(TokenType::Plus):
				AddToken(TokenType::Plus);
				break;
			case GetTokenCharacter(TokenType::Minus):
				if (Match(GetTokenCharacter(TokenType::Minus)))
				{
					// Single line comment until end of line
					while (Peek() != StringCharType('\n') && !ReachedEnd())
					{
						Consume();
					}
				}
				else
				{
					AddToken(TokenType::Minus);
				}
				break;
			case GetTokenCharacter(TokenType::Star):
				AddToken(TokenType::Star);
				break;
			case GetTokenCharacter(TokenType::Slash):
				AddToken(Match(GetTokenCharacter(TokenType::SlashSlash, 1)) ? TokenType::SlashSlash : TokenType::Slash);
				break;
			case GetTokenCharacter(TokenType::Exponent):
				AddToken(TokenType::Exponent);
				break;
			case GetTokenCharacter(TokenType::Percent):
				AddToken(TokenType::Percent);
				break;
			case GetTokenCharacter(TokenType::Ampersand):
				AddToken(TokenType::Ampersand);
				break;
			case GetTokenCharacter(TokenType::Tilde):
				AddToken(Match(GetTokenCharacter(TokenType::NotEqual, 1)) ? TokenType::NotEqual : TokenType::Tilde);
				break;
			case GetTokenCharacter(TokenType::Bar):
				AddToken(TokenType::Bar);
				break;
			case GetTokenCharacter(TokenType::Hashtag):
				AddToken(TokenType::Hashtag);
				break;
			case GetTokenCharacter(TokenType::LessEqual):
				AddToken(
					Match(GetTokenCharacter(TokenType::LessEqual, 1))
						? TokenType::LessEqual
						: (Match(GetTokenCharacter(TokenType::LessLess, 1)) ? TokenType::LessLess : TokenType::Less)
				);
				break;
			case GetTokenCharacter(TokenType::GreaterEqual):
				AddToken(
					Match(GetTokenCharacter(TokenType::GreaterEqual, 1))
						? TokenType::GreaterEqual
						: (Match(GetTokenCharacter(TokenType::GreaterGreater, 1)) ? TokenType::GreaterGreater : TokenType::Greater)
				);
				break;
			case GetTokenCharacter(TokenType::Equal):
				AddToken(Match(GetTokenCharacter(TokenType::Equal)) ? TokenType::EqualEqual : TokenType::Equal);
				break;
			case GetTokenCharacter(TokenType::LeftParentheses):
				AddToken(TokenType::LeftParentheses);
				break;
			case GetTokenCharacter(TokenType::RightParentheses):
				AddToken(TokenType::RightParentheses);
				break;
			case GetTokenCharacter(TokenType::LeftBrace):
				AddToken(TokenType::LeftBrace);
				break;
			case GetTokenCharacter(TokenType::RightBrace):
				AddToken(TokenType::RightBrace);
				break;
			case GetTokenCharacter(TokenType::LeftBracket):
				AddToken(TokenType::LeftBracket);
				break;
			case GetTokenCharacter(TokenType::RightBracket):
				AddToken(TokenType::RightBracket);
				break;
			case GetTokenCharacter(TokenType::Semicolon):
				AddToken(TokenType::Semicolon);
				break;
			case GetTokenCharacter(TokenType::Colon):
				AddToken(Match(GetTokenCharacter(TokenType::ColonColon, 1)) ? TokenType::ColonColon : TokenType::Colon);
				break;
			case GetTokenCharacter(TokenType::Comma):
				AddToken(TokenType::Comma);
				break;
			case GetTokenCharacter(TokenType::DotDotDot):
				AddToken(
					Match(GetTokenCharacter(TokenType::DotDotDot, 1))
						? (Match(GetTokenCharacter(TokenType::DotDotDot, 2)) ? TokenType::DotDotDot : TokenType::DotDot)
						: TokenType::Period
				);
				break;
			case StringCharType('"'):
				[[fallthrough]];
			case StringCharType('\''):
				String();
				break;
			case StringCharType(' '):
				[[fallthrough]];
			case StringCharType('\r'):
				[[fallthrough]];
			case StringCharType('\t'):
				break;
			case StringCharType('\n'):
				++m_state.line;
				m_state.lineOffset = m_state.current;
				break;
			default:
				if (IsDigit(c))
				{
					Number();
				}
				else if (IsAlpha(c))
				{
					Identifier();
				}
				else
				{
					StringType error;
					error.Format(SCRIPT_STRING_LITERAL("Expected valid character got '{}')"), c);
					Error(error);
				}
				break;
		}
	}

	void Lexer::String()
	{
		while (Peek() != StringCharType('"') && Peek() != StringCharType('\'') && !ReachedEnd())
		{
			if (Peek() == StringCharType('\n'))
			{
				++m_state.line;
			}
			Consume();
		}

		if (ReachedEnd())
		{
			Error(SCRIPT_STRING_LITERAL("Expected string to be terminated"));
			return;
		}

		Consume(); // closing " or '

		AddToken(TokenType::String);
	}

	void Lexer::Number()
	{
		while (IsDigit(Peek()))
		{
			Consume();
		}

		// decimal?
		if (Peek() == StringCharType('.') && IsDigit(PeekNext()))
		{
			Consume();
			while (IsDigit(Peek()))
			{
				Consume();
			}

			AddToken(TokenType::Float);
		}
		else
		{
			AddToken(TokenType::Integer);
		}
	}

	void Lexer::Identifier()
	{
		while (IsAlphaNumeric(Peek()))
		{
			Consume();
		}

		const StringType::ConstView literalView = m_pString->GetSubstring(m_state.start, m_state.current - m_state.start);
		const auto iterator = m_knownKeywordsMap.Find(literalView);
		if (iterator != m_knownKeywordsMap.end())
		{
			if (iterator->second.guid.IsValid())
			{
				const SourceLocation sourceLocation {m_sourceFilePath, m_state.start + 1 - m_state.lineOffset, m_state.line + 1};
				AddToken(Token{
					iterator->second.token,
					StringType(literalView),
					iterator->second.guid,
					sourceLocation,
				});
			}
			else
			{
				AddToken(iterator->second.token);
			}
		}
		else
		{
			AddToken(TokenType::Identifier);
		}
	}

	void Lexer::Consume()
	{
		++m_state.current;
	}

	StringCharType Lexer::Advance()
	{
		return (*m_pString)[m_state.current++];
	}

	StringCharType Lexer::Peek() const
	{
		if (ReachedEnd())
		{
			return StringCharType('\0');
		}

		return (*m_pString)[m_state.current];
	}

	StringCharType Lexer::PeekNext() const
	{
		if (m_state.current + 1 >= m_pString->GetSize())
		{
			return StringCharType('\0');
		}

		return (*m_pString)[m_state.current + 1];
	}

	bool Lexer::Match(StringCharType expected)
	{
		if (ReachedEnd())
		{
			return false;
		}

		if ((*m_pString)[m_state.current] != expected)
		{
			return false;
		}

		m_state.current++;
		return true;
	}

	bool Lexer::ReachedEnd() const
	{
		return m_state.current >= m_pString->GetSize();
	}

	bool Lexer::IsDigit(StringCharType c) const
	{
		return c >= StringCharType('0') && c <= StringCharType('9');
	}

	bool Lexer::IsAlpha(StringCharType c) const
	{
		return (c >= StringCharType('a') && c <= StringCharType('z')) || (c >= StringCharType('A') && c <= StringCharType('Z')) ||
		       c == StringCharType('_');
	}

	bool Lexer::IsAlphaNumeric(StringCharType c) const
	{
		return IsAlpha(c) || IsDigit(c);
	}

	void Lexer::AddToken(Token&& token)
	{
		m_pTokens->EmplaceBack(Forward<Token>(token));
	}

	void Lexer::AddToken(TokenType type)
	{
		StringType::ConstView literalView = m_pString->GetSubstring(m_state.start, m_state.current - m_state.start);
		
		const SourceLocation sourceLocation {m_sourceFilePath, m_state.start + 1 - m_state.lineOffset, m_state.line + 1};
		AddToken(Token{
			type,
			StringType(literalView),
			Token::GuidFromScriptString(literalView),
			sourceLocation
		});
	}

	void Lexer::EmplaceKnownIdentifier(const StringType::ConstView identifier, const Guid guid)
	{
		m_knownKeywordsMap.EmplaceOrAssign(identifier, KnownKeyword{TokenType::Identifier, guid});
	}

	void Lexer::Error(StringType::ConstView error)
	{
		const uint16 column = uint16(m_state.start + 1 - m_state.lineOffset);
		LogError("{}({},{}): error: {}", m_sourceFilePath, m_state.line + 1, column, StringType(error));
		m_state.flags.Set(Flags::Error);
	}
}
