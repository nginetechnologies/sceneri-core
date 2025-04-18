#pragma once

#include "Engine/Scripting/Parser/StringType.h"
#include "Engine/Scripting/Parser/TokenType.h"

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Optional.h>
#include <Common/Math/HashedObject.h>
#include <Common/IO/Path.h>
#include <Common/IO/PathView.h>
#include <Common/EnumFlags.h>

namespace ngine::Scripting
{
	struct Token;
}

namespace ngine::Scripting
{
	using TokenListType = Vector<Token>;
	using StringCharType = StringType::CharType;

	class Lexer
	{
	public:
		Lexer();

		enum class Flags : uint8
		{
			Error = 1 << 0
		};
	public:
		bool ScanTokens(StringType::ConstView string, TokenListType& tokens);
		void SetSourceFilePath(IO::PathView sourceFilePath);

		void EmplaceKnownIdentifier(const StringType::ConstView identifier, const Guid guid);
	protected:
		void ScanToken();
		void String();
		void Number();
		void Identifier();
	private:
		void Consume();
		[[nodiscard]] StringCharType Advance();
		[[nodiscard]] StringCharType Peek() const;
		[[nodiscard]] StringCharType PeekNext() const;
		[[nodiscard]] bool Match(StringCharType expected);
		[[nodiscard]] bool ReachedEnd() const;
		[[nodiscard]] bool IsDigit(StringCharType c) const;
		[[nodiscard]] bool IsAlpha(StringCharType c) const;
		[[nodiscard]] bool IsAlphaNumeric(StringCharType c) const;
		void AddToken(Token&& token);
		void AddToken(TokenType type);
		void Error(StringType::ConstView error);
	private:
		ngine::String m_sourceFilePath{"Unknown"};
		Optional<StringType::ConstView> m_pString;
		Optional<TokenListType*> m_pTokens;
		struct State
		{
			uint32 start{0};
			uint32 current{0};
			uint32 line{0};
			uint32 lineOffset{0};
			EnumFlags<Flags> flags;
		};
		State m_state;

		struct KnownKeyword
		{
			TokenType token;
			Guid guid;
		};

		using KnownKeywordMap = UnorderedMap<
			Math::HashedObject<StringType::ConstView>,
			KnownKeyword,
			Math::HashedObject<StringType::ConstView>::Hash,
			Math::HashedObject<StringType::ConstView>::EqualityCheck>;
		using KnownKeywordMapPair = KnownKeywordMap::NewPairType;
		KnownKeywordMap m_knownKeywordsMap;
	};
}
