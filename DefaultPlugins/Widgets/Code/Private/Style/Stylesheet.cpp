#include "Style/ComputedStylesheet.h"

#include "Style/SizeEdges.h"
#include "Style/SizeCorners.h"
#include "Style/DynamicEntry.h"
#include "DefaultStyles.h"

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/Primitives/Serialization/RectangleEdges.h>
#include <Common/Math/Primitives/Serialization/RectangleCorners.h>
#include <Common/Math/Format/Color.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/System/Query.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

#include <Engine/DataSource/DataSourceCache.h>

#include "Widgets/Widget.h"
#include "Widgets/Data/ImageDrawable.h"
#include "Widgets/Data/TextDrawable.h"
#include "Widgets/Alignment.h"
#include "Widgets/Orientation.h"
#include "Widgets/Data/Layout.h"
#include "Widgets/WordWrapType.h"

namespace ngine::Widgets::Style
{
	namespace CSS
	{
		struct TextParserBase
		{
			TextParserBase(const ConstStringView entryString)
				: m_parsedString(entryString)
			{
			}

			[[nodiscard]] static ConstStringView SkipLeadingWhitespaces(ConstStringView input)
			{
				while (input.HasElements() && isspace(input[0]))
				{
					input++;
				}
				return input;
			}

			[[nodiscard]] static ConstStringView SkipTrailingWhitespaces(ConstStringView input)
			{
				while (input.HasElements() && isspace(input.GetLastElement()))
				{
					input--;
				}
				return input;
			}

			[[nodiscard]] static ConstStringView SkipTrailingMultiLineComments(ConstStringView input)
			{
				if (input.HasElements())
				{
					input = SkipTrailingWhitespaces(input);
					while (input.EndsWith("*/"))
					{
						const ConstStringView commentSubstring = input.FindLastRange("/*");
						if (commentSubstring.HasElements())
						{
							const uint32 startCommentPosition = input.GetIteratorIndex(&commentSubstring[0]);
							input -= input.GetSize() - startCommentPosition + 1;
							input = SkipTrailingWhitespaces(input);
						}
						else
						{
							input = {};
						}
					}
				}
				return input;
			}

			[[nodiscard]] static ConstStringView SkipTrailingMultiLineCommentsAndWhitespaces(ConstStringView input)
			{
				input = SkipTrailingMultiLineComments(input);
				return input;
			}

			[[nodiscard]] static ConstStringView SkipLeadingMultiLineComments(ConstStringView input)
			{
				if (input.HasElements())
				{
					input = SkipLeadingWhitespaces(input);
					while (input.StartsWith("/*"))
					{
						const ConstStringView endCommentSubstring = input.FindFirstRange("*/");
						if (endCommentSubstring.HasElements())
						{
							const uint32 endCommentPosition = input.GetIteratorIndex(&endCommentSubstring[0]);
							input += endCommentPosition + 2;
							input = SkipLeadingWhitespaces(input);
						}
						else
						{
							input = {};
						}
					}
				}
				return input;
			}

			[[nodiscard]] static ConstStringView SkipLeadingMultiLineCommentsAndWhitespaces(ConstStringView input)
			{
				input = SkipLeadingMultiLineComments(input);
				return input;
			}
		protected:
			ConstStringView m_parsedString;
		};

		//! Helper used to parse the inside of a CSS class entry
		//! Note that this class is only responsible for parsing properties, not the selector or brackets / hierarchy
		struct EntryParser : public TextParserBase
		{
			using TextParserBase::TextParserBase;

			[[nodiscard]] ConstStringView ParsePropertyName()
			{
				ConstStringView remainingParsableString = m_parsedString;
				remainingParsableString = SkipLeadingMultiLineCommentsAndWhitespaces(remainingParsableString);
				const uint32 delimiterPosition = remainingParsableString.FindFirstOf(':');
				if (UNLIKELY(delimiterPosition == ConstStringView::InvalidPosition))
				{
					// Indicate that we are done parsing
					m_parsedString = {};
					return {};
				}

				ConstStringView propertyName = remainingParsableString.GetSubstringUpTo(delimiterPosition);
				propertyName = SkipLeadingMultiLineCommentsAndWhitespaces(propertyName);
				propertyName = SkipTrailingMultiLineCommentsAndWhitespaces(propertyName);

				remainingParsableString += delimiterPosition + 1;
				m_parsedString = remainingParsableString;
				return propertyName;
			}
			[[nodiscard]] ConstStringView ParsePropertyValue()
			{
				ConstStringView remainingParsableString = m_parsedString;
				remainingParsableString = SkipLeadingMultiLineCommentsAndWhitespaces(remainingParsableString);
				const uint32 semicolonPosition = remainingParsableString.FindFirstOf(';');
				if (UNLIKELY(semicolonPosition == ConstStringView::InvalidPosition))
				{
					// If there is no semicolon this is the last value
					ConstStringView propertyValue = remainingParsableString;
					m_parsedString = {};

					// Remove escaped strings
					propertyValue += propertyValue[0] == '\"';
					propertyValue -= propertyValue.GetLastElement() == '\"';

					return propertyValue;
				}

				ConstStringView propertyValue = remainingParsableString.GetSubstringUpTo(semicolonPosition);

				propertyValue = SkipLeadingMultiLineCommentsAndWhitespaces(propertyValue);
				propertyValue = SkipTrailingMultiLineCommentsAndWhitespaces(propertyValue);

				remainingParsableString += semicolonPosition + 1;

				bool changed = false;
				do
				{
					changed = false;
					while (remainingParsableString.HasElements() && remainingParsableString[0] == ';')
					{
						remainingParsableString++;
						changed = true;
					}

					remainingParsableString = SkipLeadingMultiLineCommentsAndWhitespaces(remainingParsableString);
					remainingParsableString = SkipTrailingMultiLineCommentsAndWhitespaces(remainingParsableString);
				} while (changed);

				// Remove escaped strings
				if (propertyValue.HasElements())
				{
					propertyValue += propertyValue[0] == '\"';
					propertyValue -= propertyValue.GetLastElement() == '\"';
				}

				m_parsedString = remainingParsableString;

				return propertyValue;
			};

			struct Property
			{
				ConstStringView name;
				ConstStringView value;
			};
			[[nodiscard]] Optional<Property> ParseProperty()
			{
				if (m_parsedString.HasElements())
				{
					const ConstStringView propertyName = ParsePropertyName();
					if (propertyName.HasElements())
					{
						const ConstStringView propertyValue = ParsePropertyValue();
						return Property{propertyName, propertyValue};
					}
				}
				return Invalid;
			};
		};

		//! Helper used to parse the inside of a CSS class entry
		//! Note that this class is only responsible for parsing properties, not the selector or brackets / hierarchy
		struct StylesheetParser : public TextParserBase
		{
			using TextParserBase::TextParserBase;

			struct Selector
			{
				ConstStringView name;
				InlineVector<ConstStringView, 4> pseudoClasses;
			};

			[[nodiscard]] Selector ParseEntrySelector()
			{
				ConstStringView remainingParsableString = m_parsedString;
				remainingParsableString = SkipLeadingMultiLineCommentsAndWhitespaces(remainingParsableString);
				const uint32 delimiterPosition = remainingParsableString.FindFirstOf('{');
				if (UNLIKELY(delimiterPosition == ConstStringView::InvalidPosition))
				{
					// Indicate that we are done parsing
					m_parsedString = {};
					return {};
				}

				ConstStringView entryName = remainingParsableString.GetSubstringUpTo(delimiterPosition);
				entryName = SkipLeadingMultiLineCommentsAndWhitespaces(entryName);
				entryName = SkipTrailingMultiLineCommentsAndWhitespaces(entryName);

				Selector selector;
				{
					uint32 lastPseudoClassDelimiterPosition = entryName.FindLastOf(':');
					while (lastPseudoClassDelimiterPosition != ConstStringView::InvalidPosition)
					{
						ConstStringView pseudoClassName = entryName.GetSubstringFrom(lastPseudoClassDelimiterPosition + 1);
						pseudoClassName = SkipLeadingMultiLineCommentsAndWhitespaces(pseudoClassName);
						selector.pseudoClasses.EmplaceBack(pseudoClassName);

						entryName = entryName.GetSubstringUpTo(lastPseudoClassDelimiterPosition);
						entryName = SkipTrailingMultiLineCommentsAndWhitespaces(entryName);
						lastPseudoClassDelimiterPosition = entryName.FindLastOf(':');
					}
				}

				selector.name = entryName;

				remainingParsableString += delimiterPosition + 1;
				m_parsedString = remainingParsableString;
				return selector;
			}

			[[nodiscard]] ConstStringView ParseEntryBody()
			{
				ConstStringView remainingParsableString = m_parsedString;
				remainingParsableString = SkipLeadingMultiLineCommentsAndWhitespaces(remainingParsableString);
				const uint32 endScopePosition = remainingParsableString.FindFirstOf('}');
				if (UNLIKELY(endScopePosition == ConstStringView::InvalidPosition))
				{
					// If there is no final bracket we failed parsing
					m_parsedString = {};
					return {};
				}

				ConstStringView propertyEntry = remainingParsableString.GetSubstringUpTo(endScopePosition);
				propertyEntry = SkipLeadingMultiLineCommentsAndWhitespaces(propertyEntry);
				propertyEntry = SkipTrailingMultiLineCommentsAndWhitespaces(propertyEntry);

				remainingParsableString += endScopePosition + 1;
				m_parsedString = remainingParsableString;

				return propertyEntry;
			};

			struct Entry
			{
				Selector selector;
				ConstStringView body;
			};
			[[nodiscard]] Optional<Entry> ParseEntry()
			{
				if (m_parsedString.HasElements())
				{
					const Selector entrySelector = ParseEntrySelector();
					if (entrySelector.name.HasElements())
					{
						const ConstStringView entryBody = ParseEntryBody();
						return Entry{entrySelector, entryBody};
					}
				}
				return Invalid;
			};
		};
	}

	void ComputedStylesheet::ParseFromCSS(ConstStringView parsedString)
	{
		if (parsedString.IsEmpty())
		{
			return;
		}

		CSS::StylesheetParser stylesheetParser(parsedString);
		while (const Optional<CSS::StylesheetParser::Entry> entry = stylesheetParser.ParseEntry())
		{
			Guid entryGuid;
			if (const Optional<Guid> parsedEntryGuid = Guid::TryParse(entry->selector.name))
			{
				entryGuid = *parsedEntryGuid;
			}
			else
			{
				// Generate a guid from string
				// These are not guaranteed to be globally unique, but for local css files it's considered good enough
				const Guid cssEntryBaseGuid = "4DAD5E5B-B94C-438E-A16E-F001420DD049"_guid;
				entryGuid = Guid::FromString(entry->selector.name, cssEntryBaseGuid);
			}

			Entry* pEntry;
			if (const Optional<Entry*> pExistingEntry = FindEntry(entryGuid))
			{
				pEntry = pExistingEntry;
			}
			else
			{
				pEntry = &m_entries.Emplace(Guid(entryGuid), Entry{})->second;
			}

			EnumFlags<Modifier> modifiers;

			for (const ConstStringView pseudoClass : entry->selector.pseudoClasses)
			{
				if (pseudoClass == "active")
				{
					modifiers |= Modifier::Active;
				}
				else if (pseudoClass == "hover")
				{
					modifiers |= Modifier::Hover;
				}
				else if (pseudoClass == "disabled")
				{
					modifiers |= Modifier::Disabled;
				}
				else if (pseudoClass == "focus")
				{
					modifiers |= Modifier::Focused;
				}
				else if (pseudoClass == "valid")
				{
					modifiers |= Modifier::Valid;
				}
				else if (pseudoClass == "invalid")
				{
					modifiers |= Modifier::Invalid;
				}
				else if (pseudoClass == "required")
				{
					modifiers |= Modifier::Required;
				}
				else if (pseudoClass == "optional")
				{
					modifiers |= Modifier::Optional;
				}
				else if (pseudoClass == "toggled_off")
				{
					modifiers |= Modifier::ToggledOff;
				}
			}

			Entry::ModifierValues& values = pEntry->FindOrEmplaceExactModifierMatch(modifiers);
			values.ParseFromCSS(entry->body);
			pEntry->m_valueTypeMask |= values.GetValueTypeMask();
			pEntry->m_dynamicValueTypeMask |= values.GetDynamicValueTypeMask();
		}
	}

	[[nodiscard]] ConstStringView ParseFunctionContents(ConstStringView stringValue)
	{
		stringValue = CSS::TextParserBase::SkipLeadingWhitespaces(stringValue);
		uint32 functionDelimiterPosition = stringValue.FindFirstOf('(');
		stringValue = stringValue.GetSubstringFrom(functionDelimiterPosition + 1);
		stringValue = CSS::TextParserBase::SkipLeadingWhitespaces(stringValue);

		functionDelimiterPosition = stringValue.FindLastOf(')');
		stringValue = stringValue.GetSubstringUpTo(functionDelimiterPosition);
		stringValue = CSS::TextParserBase::SkipTrailingWhitespaces(stringValue);

		return stringValue;
	}

	[[nodiscard]] Optional<ConstStringView> ParseFunction(const ConstStringView functionName, ConstStringView& stringValue)
	{
		if (stringValue.StartsWith(functionName))
		{
			stringValue += functionName.GetSize();
			stringValue = ParseFunctionContents(stringValue);
			return stringValue;
		}
		return Invalid;
	}

	auto parseDynamicProperty(ConstStringView input) -> ngine::DataSource::PropertyIdentifier
	{
		if (input.GetSize() > 2 && input[0] == '{' && input.GetLastElement() == '}')
		{
			const ConstStringView propertyName = input.GetSubstring(1, input.GetSize() - 2);
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			return dataSourceCache.FindOrRegisterPropertyIdentifier(propertyName);
		}
		return {};
	}

	void parseOperations(SizeAxisExpression& expression, ConstStringView& stringValue, bool& hasDynamicData)
	{
		if (Optional<SizeAxis> sizeAxis = SizeAxis::Parse(stringValue))
		{
			hasDynamicData |= sizeAxis->m_value.Is<ngine::DataSource::PropertyIdentifier>();
			expression.m_operations.EmplaceBack(Move(*sizeAxis));
		}
		else if (const Optional<ConstStringView> minFunctionStringValue = ParseFunction("min", stringValue))
		{
			const uint32 delimiterPosition = minFunctionStringValue->FindLastOf(',');
			if (LIKELY(delimiterPosition != ConstStringView::InvalidPosition))
			{
				ConstStringView leftStringValue = minFunctionStringValue->GetSubstringUpTo(delimiterPosition);
				leftStringValue = CSS::TextParserBase::SkipTrailingWhitespaces(leftStringValue);
				ConstStringView rightStringValue = minFunctionStringValue->GetSubstringFrom(delimiterPosition + 1);
				rightStringValue = CSS::TextParserBase::SkipLeadingWhitespaces(rightStringValue);
				rightStringValue = CSS::TextParserBase::SkipTrailingWhitespaces(rightStringValue);

				expression.m_operations.Reserve(expression.m_operations.GetSize() + 3);
				expression.m_operations.EmplaceBack(OperationType::Min);
				parseOperations(expression, leftStringValue, hasDynamicData);
				parseOperations(expression, rightStringValue, hasDynamicData);
			}
			else
			{
				Assert(false, "Invalid expression!");
				expression.m_operations.Clear();
			}
		}
		else if (const Optional<ConstStringView> maxFunctionStringValue = ParseFunction("max", stringValue))
		{
			const uint32 delimiterPosition = maxFunctionStringValue->FindLastOf(',');
			if (LIKELY(delimiterPosition != ConstStringView::InvalidPosition))
			{
				ConstStringView leftStringValue = maxFunctionStringValue->GetSubstringUpTo(delimiterPosition);
				leftStringValue = CSS::TextParserBase::SkipTrailingWhitespaces(leftStringValue);
				ConstStringView rightStringValue = maxFunctionStringValue->GetSubstringFrom(delimiterPosition + 1);
				rightStringValue = CSS::TextParserBase::SkipLeadingWhitespaces(rightStringValue);

				expression.m_operations.Reserve(expression.m_operations.GetSize() + 3);
				expression.m_operations.EmplaceBack(OperationType::Max);
				parseOperations(expression, leftStringValue, hasDynamicData);
				parseOperations(expression, rightStringValue, hasDynamicData);
				Assert(expression.m_operations[0].Is<OperationType>());
				Assert(!expression.m_operations[0].Is<SizeAxis>());
			}
			else
			{
				Assert(false, "Invalid expression!");
				expression.m_operations.Clear();
			}
		}
		else if (const Optional<ConstStringView> calcFunctionStringValue = ParseFunction("calc", stringValue))
		{
			uint32 delimiterPosition = calcFunctionStringValue->FindFirstOf(' ');
			if (LIKELY(delimiterPosition != ConstStringView::InvalidPosition))
			{
				ConstStringView leftStringValue = calcFunctionStringValue->GetSubstringUpTo(delimiterPosition);
				leftStringValue = CSS::TextParserBase::SkipTrailingWhitespaces(leftStringValue);
				ConstStringView operatorStringValue = calcFunctionStringValue->GetSubstringFrom(delimiterPosition + 1);
				operatorStringValue = CSS::TextParserBase::SkipLeadingWhitespaces(operatorStringValue);

				delimiterPosition = operatorStringValue.FindFirstOf(' ');
				if (LIKELY(delimiterPosition != ConstStringView::InvalidPosition))
				{
					ConstStringView rightStringValue = operatorStringValue.GetSubstringFrom(delimiterPosition + 1);
					rightStringValue = CSS::TextParserBase::SkipLeadingWhitespaces(rightStringValue);
					operatorStringValue = operatorStringValue.GetSubstringUpTo(delimiterPosition);
					operatorStringValue = CSS::TextParserBase::SkipTrailingWhitespaces(operatorStringValue);

					if (UNLIKELY(operatorStringValue.GetSize() != 1))
					{
						Assert(false, "Invalid expression!");
						expression.m_operations.Clear();
						return;
					}

					// TODO: Handle operator precedence

					expression.m_operations.Reserve(expression.m_operations.GetSize() + 3);
					switch (operatorStringValue[0])
					{
						case '+':
							expression.m_operations.EmplaceBack(OperationType::Addition);
							break;
						case '-':
							expression.m_operations.EmplaceBack(OperationType::Subtraction);
							break;
						case '*':
							expression.m_operations.EmplaceBack(OperationType::Multiplication);
							break;
						case '/':
							expression.m_operations.EmplaceBack(OperationType::Division);
							break;
						default:
							Assert(false, "Invalid expression!");
							expression.m_operations.Clear();
							return;
					}
					parseOperations(expression, leftStringValue, hasDynamicData);
					parseOperations(expression, rightStringValue, hasDynamicData);
					Assert(expression.m_operations[0].Is<OperationType>());
					Assert(!expression.m_operations[0].Is<SizeAxis>());
				}
				else
				{
					expression.m_operations.Clear();
				}
			}
			else
			{
				expression.m_operations.Clear();
			}
		}
		else
		{
			expression.m_operations.Clear();
		}
	}

	auto parseSizeAxisExpression(ConstStringView stringValue, bool& hasDynamicData) -> SizeAxisExpression
	{
		SizeAxisExpression expression;
		expression.m_operations.Clear();
		parseOperations(expression, stringValue, hasDynamicData);
		return expression;
	}

	/* static */ Optional<SizeAxisExpression> SizeAxisExpression::Parse(ConstStringView stringValue)
	{
		SizeAxisExpression expression;
		expression.m_operations.Clear();
		bool hasDynamicData{false};
		parseOperations(expression, stringValue, hasDynamicData);
		return expression;
	}

	auto parseNextSizeAxisExpression(ConstStringView& stringValue, bool& hasDynamicData) -> SizeAxisExpression
	{
		if (stringValue.IsEmpty())
		{
			return Invalid;
		}

		uint16 scopeCounter{0};
		for (auto it = stringValue.begin(), endIt = stringValue.end(); it != endIt; ++it)
		{
			if (*it == ' ' && scopeCounter == 0)
			{
				const uint32 nextSpacePosition = stringValue.GetIteratorIndex(it);
				ConstStringView sizeAxisString = stringValue.GetSubstringUpTo(nextSpacePosition);
				stringValue += nextSpacePosition + 1;
				return parseSizeAxisExpression(sizeAxisString, hasDynamicData);
			}
			scopeCounter += *it == '(';
			scopeCounter -= *it == ')';
		}

		ConstStringView sizeAxisString = stringValue;
		stringValue = {};
		return parseSizeAxisExpression(sizeAxisString, hasDynamicData);
	}

	auto parseSize(ConstStringView stringValue, bool& hasDynamicData) -> Optional<Size>
	{
		Array<SizeAxisExpression, 2> values = {
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData)
		};

		const uint8 count = values[0].IsValid() + values[1].IsValid();
		switch (count)
		{
			case 0:
				return Invalid;
			case 1:
			{
				SizeAxisExpression y{values[0]};
				return Size{Move(values[0]), Move(y)};
			}
			case 2:
			{
				return Size{Move(values[0]), Move(values[1])};
			}
			default:
				ExpectUnreachable();
		}
	}

	auto parseSizeEdges(ConstStringView stringValue, bool& hasDynamicData) -> Optional<SizeAxisEdges>
	{
		Array<SizeAxisExpression, 4> values = {
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData)
		};

		const uint8 count = values[0].IsValid() + values[1].IsValid() + values[2].IsValid() + values[3].IsValid();
		switch (count)
		{
			case 0:
				return Invalid;
			case 1:
			{
				SizeAxisExpression left = Move(values[0]);
				SizeAxisExpression top{left};
				SizeAxisExpression right{left};
				SizeAxisExpression bottom{left};

				return SizeAxisEdges{Move(top), Move(right), Move(bottom), Move(left)};
			}
			case 2:
			{
				SizeAxisExpression left = Move(values[1]);
				SizeAxisExpression top = Move(values[0]);
				SizeAxisExpression right{left};
				SizeAxisExpression bottom{top};
				return SizeAxisEdges{Move(top), Move(right), Move(bottom), Move(left)};
			}
			case 3:
			{
				SizeAxisExpression top = Move(values[0]);
				SizeAxisExpression left = Move(values[1]);
				SizeAxisExpression right{left};
				SizeAxisExpression bottom = Move(values[2]);
				return SizeAxisEdges{Move(top), Move(right), Move(bottom), Move(left)};
			}
			case 4:
			{
				SizeAxisExpression top = Move(values[0]);
				SizeAxisExpression right = Move(values[1]);
				SizeAxisExpression bottom = Move(values[2]);
				SizeAxisExpression left = Move(values[3]);
				return SizeAxisEdges{Move(top), Move(right), Move(bottom), Move(left)};
			}
			default:
				ExpectUnreachable();
		}
	}

	auto parseSizeCorners(ConstStringView stringValue, bool& hasDynamicData) -> Optional<SizeAxisCorners>
	{
		Array<SizeAxisExpression, 4> values = {
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData),
			parseNextSizeAxisExpression(stringValue, hasDynamicData)
		};

		const uint8 count = values[0].IsValid() + values[1].IsValid() + values[2].IsValid() + values[3].IsValid();
		switch (count)
		{
			case 0:
				return Invalid;
			case 1:
			{
				SizeAxisExpression topLeft = Move(values[0]);
				SizeAxisExpression topRight{topLeft};
				SizeAxisExpression bottomRight{topLeft};
				SizeAxisExpression bottomLeft{topLeft};
				return SizeAxisCorners{Move(topLeft), Move(topRight), Move(bottomRight), Move(bottomLeft)};
			}
			case 2:
			{
				SizeAxisExpression topLeft = Move(values[0]);
				SizeAxisExpression bottomRight{topLeft};
				SizeAxisExpression topRight{Move(values[1])};
				SizeAxisExpression bottomLeft{topRight};
				return SizeAxisCorners{Move(topLeft), Move(topRight), Move(bottomRight), Move(bottomLeft)};
			}
			case 3:
			{
				SizeAxisExpression topLeft = Move(values[0]);
				SizeAxisExpression topRight = Move(values[1]);
				SizeAxisExpression bottomLeft{topLeft};
				SizeAxisExpression bottomRight = Move(values[2]);
				return SizeAxisCorners{Move(topLeft), Move(topRight), Move(bottomRight), Move(bottomLeft)};
			}
			case 4:
			{
				SizeAxisExpression topLeft = Move(values[0]);
				SizeAxisExpression topRight = Move(values[1]);
				SizeAxisExpression bottomRight = Move(values[2]);
				SizeAxisExpression bottomLeft = Move(values[3]);
				return SizeAxisCorners{Move(topLeft), Move(topRight), Move(bottomRight), Move(bottomLeft)};
			}
			default:
				ExpectUnreachable();
		}
	}

	struct BorderResult
	{
		SizeAxisExpression width;
		// TODO: Style
		Optional<Math::Color> color;
	};

	auto parseColor(const ConstStringView stringValue) -> Optional<Math::Color>
	{
		constexpr ConstStringView rgbaValuePrefix{"rgba("};
		if (stringValue.StartsWith(rgbaValuePrefix))
		{
			ConstStringView rgbaValueString =
				stringValue.GetSubstring(rgbaValuePrefix.GetSize(), stringValue.GetSize() - rgbaValuePrefix.GetSize() - 1);

			uint32 delimiterPosition = rgbaValueString.FindFirstOf(',');
			if (UNLIKELY(delimiterPosition == ConstStringView::InvalidPosition))
			{
				return Invalid;
			}
			ConstStringView rValueString = rgbaValueString.GetSubstringUpTo(delimiterPosition);
			rValueString = CSS::TextParserBase::SkipTrailingWhitespaces(rValueString);

			rgbaValueString += delimiterPosition + 1;

			delimiterPosition = rgbaValueString.FindFirstOf(',');
			if (UNLIKELY(delimiterPosition == ConstStringView::InvalidPosition))
			{
				return Invalid;
			}

			ConstStringView gValueString = rgbaValueString.GetSubstringUpTo(delimiterPosition);
			gValueString = CSS::TextParserBase::SkipLeadingWhitespaces(gValueString);

			rgbaValueString += delimiterPosition + 1;
			delimiterPosition = rgbaValueString.FindFirstOf(',');
			if (UNLIKELY(delimiterPosition == ConstStringView::InvalidPosition))
			{
				return Invalid;
			}

			ConstStringView bValueString = rgbaValueString.GetSubstringUpTo(delimiterPosition);
			bValueString = CSS::TextParserBase::SkipLeadingWhitespaces(bValueString);

			ConstStringView aValueString = rgbaValueString.GetSubstringFrom(delimiterPosition + 1);
			aValueString = CSS::TextParserBase::SkipLeadingWhitespaces(aValueString);

			static auto parseUnit = [](ConstStringView unitValueString) -> float
			{
				if (unitValueString.GetLastElement() == '%')
				{
					unitValueString--;
					return unitValueString.ToFloat() * 0.01f;
				}
				else
				{
					return (float)unitValueString.ToIntegral<uint8>() / 255.f;
				}
			};
			return Math::Color{parseUnit(rValueString), parseUnit(gValueString), parseUnit(bValueString), aValueString.ToFloat()};
		}

		return Math::Color::TryParse(stringValue.GetData(), (uint32)stringValue.GetSize());
	}

	auto parseBorder(ConstStringView stringValue, bool& hasDynamicData) -> BorderResult
	{
		uint32 nextSpacePosition = stringValue.FindFirstOf(' ');
		if (nextSpacePosition == ConstStringView::InvalidPosition)
		{
			if (Optional<SizeAxisExpression> borderSize = parseSizeAxisExpression(stringValue, hasDynamicData))
			{
				return BorderResult{Move(*borderSize)};
			}
			return {};
		}

		BorderResult result;

		if (Optional<SizeAxisExpression> borderSize = parseSizeAxisExpression(stringValue.GetSubstringUpTo(nextSpacePosition), hasDynamicData))
		{
			result.width = Move(*borderSize);
		}
		stringValue += nextSpacePosition + 1;
		nextSpacePosition = stringValue.FindFirstOf(' ');
		if (nextSpacePosition == ConstStringView::InvalidPosition)
		{
			return result;
		}
		[[maybe_unused]] ConstStringView borderStyle = stringValue.GetSubstringUpTo(nextSpacePosition);
		// TODO: Handle style
		stringValue += nextSpacePosition + 1;

		result.color = parseColor(stringValue);
		return result;
	};

	template<typename Callback>
	void IterateScopeElements(const ConstStringView stringView, const ConstStringView::CharType split, Callback&& callback)
	{
		uint32 startOffset = 0;
		uint32 scope = 0;
		const uint32 size = stringView.GetSize();
		for (uint32 i = 0; i < size; ++i)
		{
			auto character = stringView[i];
			if (character == '(')
			{
				scope++;
			}
			else if (character == ')')
			{
				scope--;
			}
			else if (scope == 0 && character == split)
			{
				const ConstStringView elementView = stringView.GetSubstring(startOffset, i - startOffset);
				if (!elementView.IsEmpty())
				{
					startOffset = i + 1;
					callback(elementView);
				}
				else
				{
					startOffset++;
				}
			}
			else if (i == size - 1)
			{
				const ConstStringView elementView = stringView.GetSubstring(startOffset, size - startOffset);
				if (!elementView.IsEmpty())
				{
					callback(elementView);
				}
			}
		}
	}

	[[nodiscard]] Optional<Math::Anglef> ParseAngle(ConstStringView view)
	{
		const ConstStringView result = view.FindFirstRange("deg");
		if (!result.IsEmpty())
		{
			const ConstStringView angleView = view.GetSubstringUpTo(view.GetIteratorIndex(result.GetData()));
			if (auto value = angleView.TryToFloat(); value.success)
			{
				return Math::Anglef::FromDegrees(value.value);
			}
		}
		return Invalid;
	}

	[[nodiscard]] Optional<Math::Ratiof> ParsePercentageValue(const ConstStringView view)
	{
		const uint32 index = view.FindFirstOf('%');
		if (view.IsValidIndex(index))
		{
			const ConstStringView percentageView = view.GetSubstringUpTo(index);
			if (auto value = percentageView.TryToFloat(); value.success)
			{
				return Math::Ratiof(value.value / 100.f);
			}
		}
		return Invalid;
	}

	[[nodiscard]] Math::LinearGradient ParseLinearGradient(const ConstStringView valueView)
	{
		Math::LinearGradient gradient;

		IterateScopeElements(
			valueView,
			',',
			[elementIndex = 0, &gradient](const ConstStringView valueElement) mutable
			{
				// First element should always be the gradient orientation
				switch (elementIndex)
				{
					case 0:
					{
						if (Optional<Math::Anglef> parsedAngle = ParseAngle(valueElement))
						{
							gradient.m_orientation = *parsedAngle;
						}
					}
					break;
					default:
					{
						Math::LinearGradient::Color gradientColor;
						uint32 colorIndex = 0;
						IterateScopeElements(
							valueElement,
							' ',
							[&colorIndex, &gradientColor](const ConstStringView colorElement) mutable
							{
								switch (colorIndex)
								{
									case 0:
									{
										if (Optional<Math::Color> color = parseColor(colorElement))
										{
											gradientColor.m_color = *color;
										}
									}
									break;
									case 1:
									{
										if (Optional<Math::Ratiof> ratio = ParsePercentageValue(colorElement))
										{
											gradientColor.m_stopPoint = *ratio;
										}
									}
									break;
									case 2:
										Assert(false);
								}

								++colorIndex;
							}
						);

						// TODO: Fully support all possible css gradient combinations
						Assert(colorIndex == 2, "Expects a color and one procent value!");
						if (LIKELY(colorIndex == 2))
						{
							gradient.m_colors.EmplaceBack(Move(gradientColor));
						}
					}
					break;
				}
				elementIndex++;
			}
		);

		return Move(gradient);
	}

	[[nodiscard]] Math::LinearGradient ParseConicGradient(ConstStringView valueView)
	{
		Math::LinearGradient gradient;

		const ConstStringView from = valueView.FindFirstRange("from ");
		if (from.HasElements())
		{
			ConstStringView fromView = valueView.GetSubstringUpTo(valueView.FindFirstOf(','));
			fromView += from.GetSize();
			fromView = CSS::TextParserBase::SkipLeadingWhitespaces(fromView);
			const ConstStringView::IndexType fromDelimiterIndex = fromView.FindFirstOf(',');
			fromView = fromView.GetSubstringUpTo(fromDelimiterIndex);
			fromView = CSS::TextParserBase::SkipTrailingWhitespaces(fromView);

			Optional<Math::Anglef> parsedAngle = ParseAngle(fromView);
			Assert(parsedAngle.IsValid());
			if (LIKELY(parsedAngle.IsValid()))
			{
				gradient.m_orientation = *parsedAngle;
			}

			fromView = fromView.GetSubstringFrom(fromDelimiterIndex);
			// TODO: Parse at

			valueView = valueView.GetSubstringFrom(valueView.FindFirstOf(','));
		}

		IterateScopeElements(
			valueView,
			',',
			[&gradient](const ConstStringView valueElement) mutable
			{
				uint8 elementIndex = 0;
				Math::LinearGradient::Color gradientColor;
				IterateScopeElements(
					valueElement,
					' ',
					[&elementIndex, &gradientColor](const ConstStringView colorElement) mutable
					{
						switch (elementIndex)
						{
							case 0:
							{
								if (Optional<Math::Color> color = parseColor(colorElement))
								{
									gradientColor.m_color = *color;
								}
							}
							break;
							case 1:
							{
								if (Optional<Math::Anglef> parsedAngle = ParseAngle(colorElement))
								{
									gradientColor.m_stopPoint = parsedAngle->GetRadians() / Math::Constantsf::PI2;
								}
							}
							break;
							case 2:
								Assert(false);
						}

						++elementIndex;
					}
				);

				Assert(elementIndex > 0, "Expects a color");
				if (LIKELY(elementIndex > 0))
				{
					gradient.m_colors.EmplaceBack(Move(gradientColor));
				}
			}
		);

		return Move(gradient);
	}

	void Entry::ModifierValues::ParseFromCSS(ConstStringView parsedString)
	{
		if (parsedString.IsEmpty())
		{
			return;
		}

		CSS::EntryParser entryParser(parsedString);
		while (const Optional<CSS::EntryParser::Property> property = entryParser.ParseProperty())
		{
			// Find the guid for this style entry
			if (property->name == "background")
			{
				// TODO: Support commas to define multiple layers.

				constexpr ConstStringView assetValuePrefix{"asset("};
				constexpr ConstStringView linearGradientPrefix{"linear-gradient("};
				constexpr ConstStringView conicGradientPrefix{"conic-gradient("};
				if (property->value.StartsWith(assetValuePrefix))
				{
					const ConstStringView assetValueString =
						property->value.GetSubstring(assetValuePrefix.GetSize(), property->value.GetSize() - assetValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(assetValueString))
					{
						Emplace(ValueType::AssetIdentifier, Asset::Guid{*guid});
					}
					else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(assetValueString))
					{
						Emplace(ValueType::AssetIdentifier, dynamicPropertyIdentifier);
					}
				}
				else if (property->value.StartsWith(linearGradientPrefix))
				{
					const ConstStringView gradientValueString =
						property->value.GetSubstring(linearGradientPrefix.GetSize(), property->value.GetSize() - linearGradientPrefix.GetSize() - 1);
					if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(gradientValueString))
					{
						Emplace(ValueType::BackgroundLinearGradient, dynamicPropertyIdentifier);
						Clear(ValueType::BackgroundConicGradient);
						Clear(ValueType::BackgroundColor);
					}
					else
					{
						// Extract values from line-gradient scope
						const uint32 linearGradientValueStart = property->value.FindFirstOf('(') + 1;
						const uint32 linearGradientValueEnd = property->value.FindLastOf(')');
						const uint32 linearGradientValueLength = linearGradientValueEnd - linearGradientValueStart;
						const ConstStringView linearGradientValueView =
							property->value.GetSubstring(linearGradientValueStart, linearGradientValueLength);

						Math::LinearGradient linearGradient = ParseLinearGradient(linearGradientValueView);
						if (linearGradient.IsValid())
						{
							Emplace(ValueType::BackgroundLinearGradient, Move(linearGradient));
							Clear(ValueType::BackgroundConicGradient);
							Clear(ValueType::BackgroundColor);
						}

						// Parse background at the end if available
						ConstStringView remainderStringView = property->value.GetSubstringFrom(linearGradientValueEnd + 1);
						remainderStringView = CSS::TextParserBase::SkipLeadingWhitespaces(remainderStringView);
						remainderStringView += remainderStringView.HasElements() && remainderStringView[0] == ',';
						remainderStringView = CSS::TextParserBase::SkipLeadingWhitespaces(remainderStringView);

						if (const Optional<Math::Color> color = parseColor(remainderStringView))
						{
							Emplace(ValueType::BackgroundColor, *color);
						}
					}
				}
				else if (property->value.StartsWith(conicGradientPrefix))
				{
					const ConstStringView gradientValueString =
						property->value.GetSubstring(conicGradientPrefix.GetSize(), property->value.GetSize() - conicGradientPrefix.GetSize() - 1);
					if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(gradientValueString))
					{
						Emplace(ValueType::BackgroundConicGradient, dynamicPropertyIdentifier);
						Clear(ValueType::BackgroundColor);
						Clear(ValueType::BackgroundLinearGradient);
					}
					else
					{
						// Extract values from line-gradient scope
						const uint32 conicGradientValueStart = property->value.FindFirstOf('(') + 1;
						const uint32 conicGradientValueEnd = property->value.FindLastOf(')');
						const uint32 conicGradientValueLength = conicGradientValueEnd - conicGradientValueStart;
						const ConstStringView conicGradientValueView = property->value.GetSubstring(conicGradientValueStart, conicGradientValueLength);

						Math::LinearGradient conicGradient = ParseConicGradient(conicGradientValueView);
						if (conicGradient.IsValid())
						{
							Emplace(ValueType::BackgroundConicGradient, Move(conicGradient));
							Clear(ValueType::BackgroundColor);
							Clear(ValueType::BackgroundLinearGradient);
						}

						// Parse background at the end if available
						ConstStringView remainderStringView = property->value.GetSubstringFrom(conicGradientValueEnd + 1);
						remainderStringView = CSS::TextParserBase::SkipLeadingWhitespaces(remainderStringView);
						remainderStringView += remainderStringView.HasElements() && remainderStringView[0] == ',';
						remainderStringView = CSS::TextParserBase::SkipLeadingWhitespaces(remainderStringView);

						if (const Optional<Math::Color> color = parseColor(remainderStringView))
						{
							Emplace(ValueType::BackgroundColor, *color);
						}
					}
				}
				else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::BackgroundColor, dynamicPropertyIdentifier);
					Clear(ValueType::BackgroundLinearGradient);
					Clear(ValueType::BackgroundConicGradient);
				}
				else if (property->value == "none" || property->value == "unset")
				{
					Clear(ValueType::BackgroundColor);
					Clear(ValueType::AssetIdentifier);
					Clear(ValueType::BackgroundGrid);
					Clear(ValueType::BackgroundSpline);
					Clear(ValueType::BackgroundConicGradient);
					Clear(ValueType::BackgroundLinearGradient);
				}
				else if (const Optional<Math::Color> color = parseColor(property->value))
				{
					Emplace(ValueType::BackgroundColor, *color);
					Clear(ValueType::BackgroundConicGradient);
					Clear(ValueType::BackgroundLinearGradient);
				}
				else
				{
					Clear(ValueType::BackgroundColor);
					Clear(ValueType::AssetIdentifier);
					Clear(ValueType::BackgroundGrid);
					Clear(ValueType::BackgroundSpline);
					Clear(ValueType::BackgroundConicGradient);
					Clear(ValueType::BackgroundLinearGradient);
				}
			}
			else if (property->name == "background-color")
			{
				if (const Optional<Math::Color> color = parseColor(property->value))
				{
					Emplace(ValueType::BackgroundColor, *color);
					Clear(ValueType::BackgroundLinearGradient);
					Clear(ValueType::BackgroundConicGradient);
				}
				else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::BackgroundColor, dynamicPropertyIdentifier);
					Clear(ValueType::BackgroundLinearGradient);
					Clear(ValueType::BackgroundConicGradient);
				}
				else
				{
					Clear(ValueType::BackgroundColor);
					Clear(ValueType::BackgroundLinearGradient);
					Clear(ValueType::BackgroundConicGradient);
				}
			}
			else if (property->name == "background-image")
			{
				constexpr ConstStringView assetValuePrefix{"asset("};
				if (property->value.StartsWith(assetValuePrefix))
				{
					const ConstStringView assetValueString =
						property->value.GetSubstring(assetValuePrefix.GetSize(), property->value.GetSize() - assetValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(assetValueString))
					{
						Emplace(ValueType::AssetIdentifier, Asset::Guid{*guid});
					}
					else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(assetValueString))
					{
						Emplace(ValueType::AssetIdentifier, dynamicPropertyIdentifier);
					}
					else
					{
						Clear(ValueType::AssetIdentifier);
					}
				}
				else
				{
					Clear(ValueType::AssetIdentifier);
				}
			}
			else if (property->name == "opacity")
			{
				Emplace(ValueType::Opacity, Math::Ratiof{property->value.ToFloat()});
			}
			else if (property->name == "draggable-asset")
			{
				constexpr ConstStringView assetValuePrefix{"asset("};
				constexpr ConstStringView componentValuePrefix{"component("};
				if (property->value.StartsWith(assetValuePrefix))
				{
					const ConstStringView assetValueString =
						property->value.GetSubstring(assetValuePrefix.GetSize(), property->value.GetSize() - assetValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(assetValueString))
					{
						Emplace(ValueType::DraggableAsset, Asset::Guid{*guid});
					}
					else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(assetValueString))
					{
						Emplace(ValueType::DraggableAsset, dynamicPropertyIdentifier);
					}
				}
				else if (property->value.StartsWith(componentValuePrefix))
				{
					const ConstStringView componentValueString =
						property->value.GetSubstring(componentValuePrefix.GetSize(), property->value.GetSize() - componentValuePrefix.GetSize() - 1);
					if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(componentValueString))
					{
						Emplace(ValueType::DraggableComponent, dynamicPropertyIdentifier);
					}
				}
			}
			else if (property->name == "attached-asset")
			{
				constexpr ConstStringView assetValuePrefix{"asset("};
				constexpr ConstStringView componentValuePrefix{"component("};
				constexpr ConstStringView documentValuePrefix{"document("};
				if (property->value.StartsWith(assetValuePrefix))
				{
					const ConstStringView assetValueString =
						property->value.GetSubstring(assetValuePrefix.GetSize(), property->value.GetSize() - assetValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(assetValueString))
					{
						Emplace(ValueType::AttachedAsset, Asset::Guid{*guid});
					}
					else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(assetValueString))
					{
						Emplace(ValueType::AttachedAsset, dynamicPropertyIdentifier);
					}
				}
				else if (property->value.StartsWith(componentValuePrefix))
				{
					const ConstStringView componentValueString =
						property->value.GetSubstring(componentValuePrefix.GetSize(), property->value.GetSize() - componentValuePrefix.GetSize() - 1);
					if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(componentValueString))
					{
						Emplace(ValueType::AttachedComponent, dynamicPropertyIdentifier);
					}
				}
				else if (property->value.StartsWith(documentValuePrefix))
				{
					const ConstStringView documentValueString =
						property->value.GetSubstring(documentValuePrefix.GetSize(), property->value.GetSize() - documentValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(documentValueString))
					{
						Emplace(ValueType::AttachedDocumentAsset, Asset::Guid{*guid});
					}
					else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(documentValueString))
					{
						Emplace(ValueType::AttachedDocumentAsset, dynamicPropertyIdentifier);
					}
				}
			}
			else if (property->name == "edit_document")
			{
				if (property->value == "true")
				{
					Emplace(ValueType::EnableEditing, true);
				}
				else if (property->value == "false")
				{
					Emplace(ValueType::EnableEditing, false);
				}
				else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::EnableEditing, dynamicPropertyIdentifier);
				}
			}
			else if (property->name == "border")
			{
				bool hasDynamicData{false};
				BorderResult border = parseBorder(property->value, hasDynamicData);
				if (border.width.IsValid())
				{
					Emplace(ValueType::BorderThickness, Style::SizeAxisEdges{Move(border.width)});
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
				if (border.color.IsValid())
				{
					Emplace(ValueType::BorderColor, *border.color);
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderColor);
					}
				}
			}
			else if (property->name == "border-left")
			{
				bool hasDynamicData{false};
				BorderResult border = parseBorder(property->value, hasDynamicData);
				if (border.width.IsValid())
				{
					if (const Optional<SizeAxisEdges*> existingBorder = Find<SizeAxisEdges>(ValueType::BorderThickness))
					{
						existingBorder->m_left = Move(border.width);
					}
					else
					{
						Emplace(ValueType::BorderThickness, SizeAxisEdges{Math::Zero, Math::Zero, Math::Zero, border.width});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
				// TODO: Handle per-edge color
				if (border.color.IsValid())
				{
					Emplace(ValueType::BorderColor, *border.color);
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderColor);
					}
				}
			}
			else if (property->name == "border-right")
			{
				bool hasDynamicData{false};
				BorderResult border = parseBorder(property->value, hasDynamicData);
				if (border.width.IsValid())
				{
					if (const Optional<SizeAxisEdges*> existingBorder = Find<SizeAxisEdges>(ValueType::BorderThickness))
					{
						existingBorder->m_right = Move(border.width);
					}
					else
					{
						Emplace(ValueType::BorderThickness, SizeAxisEdges{Math::Zero, border.width, Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
				// TODO: Handle per-edge color
				if (border.color.IsValid())
				{
					Emplace(ValueType::BorderColor, *border.color);
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderColor);
					}
				}
			}
			else if (property->name == "border-top")
			{
				bool hasDynamicData{false};
				BorderResult border = parseBorder(property->value, hasDynamicData);
				if (border.width.IsValid())
				{
					if (const Optional<SizeAxisEdges*> existingBorder = Find<SizeAxisEdges>(ValueType::BorderThickness))
					{
						existingBorder->m_top = Move(border.width);
					}
					else
					{
						Emplace(ValueType::BorderThickness, SizeAxisEdges{border.width, Math::Zero, Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
				// TODO: Handle per-edge color
				if (border.color.IsValid())
				{
					Emplace(ValueType::BorderColor, *border.color);
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderColor);
					}
				}
			}
			else if (property->name == "border-bottom")
			{
				bool hasDynamicData{false};
				BorderResult border = parseBorder(property->value, hasDynamicData);
				if (border.width.IsValid())
				{
					if (const Optional<SizeAxisEdges*> existingBorder = Find<SizeAxisEdges>(ValueType::BorderThickness))
					{
						existingBorder->m_bottom = Move(border.width);
					}
					else
					{
						Emplace(ValueType::BorderThickness, SizeAxisEdges{Math::Zero, Math::Zero, border.width, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
				// TODO: Handle per-edge color
				if (border.color.IsValid())
				{
					Emplace(ValueType::BorderColor, *border.color);
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderColor);
					}
				}
			}
			else if (property->name == "border-radius")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisCorners> borderRadius = parseSizeCorners(property->value, hasDynamicData))
				{
					Emplace(ValueType::RoundingRadius, Move(*borderRadius));
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::RoundingRadius);
					}
				}
			}
			else if (property->name == "border-color")
			{
				if (const Optional<Math::Color> color = parseColor(property->value))
				{
					Emplace(ValueType::BorderColor, *color);
				}
			}
			else if (property->name == "border-width")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisEdges> borderWidth = parseSizeEdges(property->value, hasDynamicData))
				{
					Emplace(ValueType::BorderThickness, Move(*borderWidth));
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::BorderThickness);
					}
				}
			}
			else if (property->name == "display")
			{
				if (property->value == "flex")
				{
					Emplace(ValueType::LayoutType, LayoutType::Flex);
				}
				else if (property->value == "grid")
				{
					Emplace(ValueType::LayoutType, LayoutType::Grid);
				}
				else if (property->value == "none")
				{
					Emplace(ValueType::LayoutType, LayoutType::None);
				}
				else if (property->value == "block")
				{
					Emplace(ValueType::LayoutType, LayoutType::Block);
				}
				else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::LayoutType, dynamicPropertyIdentifier);
				}
				else
				{
					Clear(ValueType::LayoutType);
				}
			}
			else if (property->name == "visibility")
			{
				if (property->value == "visible")
				{
					Emplace(ValueType::Visibility, true);
				}
				else if (property->value == "hidden")
				{
					Emplace(ValueType::Visibility, false);
				}
			}
			else if (property->name == "flex-wrap")
			{
				if (property->value == "nowrap")
				{
					Clear(ValueType::WrapType);
				}
				else if (property->value == "wrap")
				{
					Emplace(ValueType::WrapType, WrapType::Wrap);
				}
				else if (property->value == "wrap-reverse")
				{
					Emplace(ValueType::WrapType, WrapType::WrapReverse);
				}
			}
			else if (property->name == "flex-grow")
			{
				Emplace(ValueType::ChildGrowthFactor, property->value.ToFloat());
			}
			else if (property->name == "flex-shrink")
			{
				Emplace(ValueType::ChildShrinkFactor, property->value.ToFloat());
			}
			else if (property->name == "flex-basis")
			{
				bool hasDynamicData{false};
				if (property->value == "auto")
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->x = Auto;
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Auto, Auto});
					}
				}
				else if (Optional<SizeAxisExpression> width = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->x = Move(*width);
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Move(*width), Auto});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::PreferredSize);
					}
				}
			}
			else if (property->name == "overflow")
			{
				if (property->value == "visible")
				{
					Emplace(ValueType::OverflowType, OverflowType::Visible);
				}
				else if (property->value == "hidden")
				{
					Clear(ValueType::OverflowType);
				}
				else if (property->value == "scroll")
				{
					Emplace(ValueType::OverflowType, OverflowType::Scroll);
				}
				else if (property->value == "auto")
				{
					Emplace(ValueType::OverflowType, OverflowType::Auto);
				}
			}
			else if (property->name == "box-sizing")
			{
				if (property->value == "content-box")
				{
					Clear(ValueType::ElementSizingType);
				}
				else if (property->value == "border-box")
				{
					Emplace(ValueType::ElementSizingType, ElementSizingType::BorderBox);
				}
			}
			else if (property->name == "flex-direction")
			{
				if (property->value == "row")
				{
					Emplace(ValueType::Orientation, Orientation::Horizontal);
				}
				else if (property->value == "column")
				{
					Emplace(ValueType::Orientation, Orientation::Vertical);
				}
			}
			else if (property->name == "align-items")
			{
				if (property->value == "flex-start")
				{
					Emplace(ValueType::SecondaryDirectionAlignment, Alignment::Start);
				}
				else if (property->value == "center")
				{
					Emplace(ValueType::SecondaryDirectionAlignment, Alignment::Center);
				}
				else if (property->value == "flex-end")
				{
					Emplace(ValueType::SecondaryDirectionAlignment, Alignment::End);
				}
				else if (property->value == "stretch")
				{
					Emplace(ValueType::SecondaryDirectionAlignment, Alignment::Stretch);
				}
			}
			else if (property->name == "justify-content")
			{
				if (property->value == "flex-start")
				{
					Emplace(ValueType::PrimaryDirectionAlignment, Alignment::Start);
				}
				else if (property->value == "center")
				{
					Emplace(ValueType::PrimaryDirectionAlignment, Alignment::Center);
				}
				else if (property->value == "flex-end")
				{
					Emplace(ValueType::PrimaryDirectionAlignment, Alignment::End);
				}
				else if (property->value == "stretch")
				{
					Emplace(ValueType::PrimaryDirectionAlignment, Alignment::Stretch);
				}
			}
			else if (property->name == "align-self")
			{
				if (property->value == "flex-start")
				{
					Emplace(ValueType::ChildSecondaryAlignment, Alignment::Start);
				}
				else if (property->value == "center")
				{
					Emplace(ValueType::ChildSecondaryAlignment, Alignment::Center);
				}
				else if (property->value == "flex-end")
				{
					Emplace(ValueType::ChildSecondaryAlignment, Alignment::End);
				}
				else if (property->value == "stretch")
				{
					Emplace(ValueType::ChildSecondaryAlignment, Alignment::Stretch);
				}
			}
			else if (property->name == "justify-self")
			{
				if (property->value == "flex-start")
				{
					Emplace(ValueType::ChildPrimaryAlignment, Alignment::Start);
				}
				else if (property->value == "center")
				{
					Emplace(ValueType::ChildPrimaryAlignment, Alignment::Center);
				}
				else if (property->value == "flex-end")
				{
					Emplace(ValueType::ChildPrimaryAlignment, Alignment::End);
				}
				else if (property->value == "stretch")
				{
					Emplace(ValueType::ChildPrimaryAlignment, Alignment::Stretch);
				}
			}
			else if (property->name == "padding")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisEdges> padding = parseSizeEdges(property->value, hasDynamicData))
				{
					Emplace(ValueType::Padding, Move(*padding));
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Padding);
					}
				}
			}
			else if (property->name == "padding-left")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> padding = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingPadding = Find<SizeAxisEdges>(ValueType::Padding))
					{
						existingPadding->m_left = Move(*padding);
					}
					else
					{
						Emplace(ValueType::Padding, SizeAxisEdges{Math::Zero, Math::Zero, Math::Zero, Move(*padding)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Padding);
					}
				}
			}
			else if (property->name == "padding-right")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> padding = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingPadding = Find<SizeAxisEdges>(ValueType::Padding))
					{
						existingPadding->m_right = Move(*padding);
					}
					else
					{
						Emplace(ValueType::Padding, SizeAxisEdges{Math::Zero, Move(*padding), Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Padding);
					}
				}
			}
			else if (property->name == "padding-top")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> padding = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingPadding = Find<SizeAxisEdges>(ValueType::Padding))
					{
						existingPadding->m_top = Move(*padding);
					}
					else
					{
						Emplace(ValueType::Padding, SizeAxisEdges{Move(*padding), Math::Zero, Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Padding);
					}
				}
			}
			else if (property->name == "padding-bottom")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> padding = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingPadding = Find<SizeAxisEdges>(ValueType::Padding))
					{
						existingPadding->m_bottom = Move(*padding);
					}
					else
					{
						Emplace(ValueType::Padding, SizeAxisEdges{Math::Zero, Math::Zero, Move(*padding), Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Padding);
					}
				}
			}
			else if (property->name == "margin")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisEdges> margin = parseSizeEdges(property->value, hasDynamicData))
				{
					Emplace(ValueType::Margin, Move(*margin));
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Margin);
					}
				}
			}
			else if (property->name == "margin-left")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> margin = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingMargin = Find<SizeAxisEdges>(ValueType::Margin))
					{
						existingMargin->m_left = Move(*margin);
					}
					else
					{
						Emplace(ValueType::Margin, SizeAxisEdges{Math::Zero, Math::Zero, Math::Zero, Move(*margin)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Margin);
					}
				}
			}
			else if (property->name == "margin-right")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> margin = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingMargin = Find<SizeAxisEdges>(ValueType::Margin))
					{
						existingMargin->m_right = Move(*margin);
					}
					else
					{
						Emplace(ValueType::Margin, SizeAxisEdges{Math::Zero, Move(*margin), Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Margin);
					}
				}
			}
			else if (property->name == "margin-top")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> margin = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingMargin = Find<SizeAxisEdges>(ValueType::Margin))
					{
						existingMargin->m_top = Move(*margin);
					}
					else
					{
						Emplace(ValueType::Margin, SizeAxisEdges{Move(*margin), Math::Zero, Math::Zero, Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Margin);
					}
				}
			}
			else if (property->name == "margin-bottom")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> margin = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> existingMargin = Find<SizeAxisEdges>(ValueType::Margin))
					{
						existingMargin->m_bottom = Move(*margin);
					}
					else
					{
						Emplace(ValueType::Margin, SizeAxisEdges{Math::Zero, Math::Zero, Move(*margin), Math::Zero});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Margin);
					}
				}
			}
			else if (property->name == "gap")
			{
				bool hasDynamicData{false};
				if (Optional<Size> size = parseSize(property->value, hasDynamicData))
				{
					Emplace(ValueType::ChildOffset, Move(*size));
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::ChildOffset);
					}
				}
			}
			else if (property->name == "row-gap")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> size = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> childOffset = Find<Size>(ValueType::ChildOffset))
					{
						childOffset->y = Move(*size);
					}
					else
					{
						Emplace(ValueType::ChildOffset, Size{0_px, Move(*size)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::ChildOffset);
					}
				}
			}
			else if (property->name == "column-gap")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> size = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> childOffset = Find<Size>(ValueType::ChildOffset))
					{
						childOffset->x = Move(*size);
					}
					else
					{
						Emplace(ValueType::ChildOffset, Size{Move(*size), 0_px});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::ChildOffset);
					}
				}
			}
			else if (property->name == "width")
			{
				bool hasDynamicData{false};
				if (property->value == "auto")
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->x = Auto;
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Auto, Auto});
					}
				}
				/*else if(const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
				  if (const Optional<Size*> preferredSize = Find<Size>(Widgets::Widget::PreferredSizeStyleGuid))
				  {
				    preferredSize->x = dynamicPropertyIdentifier;
				  }
				  else
				  {
				    Emplace(Guid(Widgets::Widget::PreferredSizeStyleGuid), Size{dynamicPropertyIdentifier, Auto});
				  }
				}*/
				else if (Optional<SizeAxisExpression> width = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->x = Move(*width);
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Move(*width), Auto});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::PreferredSize);
					}
				}
			}
			else if (property->name == "height")
			{
				bool hasDynamicData{false};
				if (property->value == "auto")
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->y = Auto;
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Auto, Auto});
					}
				}
				else if (Optional<SizeAxisExpression> height = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> preferredSize = Find<Size>(ValueType::PreferredSize))
					{
						preferredSize->y = Move(*height);
					}
					else
					{
						Emplace(ValueType::PreferredSize, Size{Auto, Move(*height)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::PreferredSize);
					}
				}
			}
			else if (property->name == "min-width")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> width = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> minimumSize = Find<Size>(ValueType::MinimumSize))
					{
						minimumSize->x = Move(*width);
					}
					else
					{
						Emplace(ValueType::MinimumSize, Size{Move(*width), 0_percent});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::MinimumSize);
					}
				}
			}
			else if (property->name == "min-height")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> height = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> minimumSize = Find<Size>(ValueType::MinimumSize))
					{
						minimumSize->y = Move(*height);
					}
					else
					{
						Emplace(ValueType::MinimumSize, Size{0_percent, Move(*height)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::MinimumSize);
					}
				}
			}
			else if (property->name == "max-width")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> width = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> maximumSize = Find<Size>(ValueType::MaximumSize))
					{
						maximumSize->x = Move(*width);
					}
					else
					{
						Emplace(ValueType::MaximumSize, Size{Move(*width), SizeAxis{NoMaximum}});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::MaximumSize);
					}
				}
			}
			else if (property->name == "max-height")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> height = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> maximumSize = Find<Size>(ValueType::MaximumSize))
					{
						maximumSize->y = Move(*height);
					}
					else
					{
						Emplace(ValueType::MaximumSize, Size{SizeAxis{NoMaximum}, Move(*height)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::MaximumSize);
					}
				}
			}
			else if (property->name == "position")
			{
				if (property->value == "absolute")
				{
					Emplace(ValueType::PositionType, PositionType::Absolute);
				}
				else if (property->value == "relative")
				{
					Emplace(ValueType::PositionType, PositionType::Relative);
				}
				else if (property->value == "static")
				{
					Emplace(ValueType::PositionType, PositionType::Static);
				}
				else if (property->value == "dynamic")
				{
					Emplace(ValueType::PositionType, PositionType::Dynamic);
				}
			}
			else if (property->name == "left")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> offset = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> position = Find<SizeAxisEdges>(ValueType::Position))
					{
						position->m_left = Move(*offset);
					}
					else
					{
						Emplace(ValueType::Position, SizeAxisEdges{Invalid, Invalid, Invalid, Move(*offset)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Position);
					}
				}
			}
			else if (property->name == "right")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> offset = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> position = Find<SizeAxisEdges>(ValueType::Position))
					{
						position->m_right = Move(*offset);
					}
					else
					{
						Emplace(ValueType::Position, SizeAxisEdges{Invalid, Move(*offset), Invalid, Invalid});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Position);
					}
				}
			}
			else if (property->name == "top")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> offset = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> position = Find<SizeAxisEdges>(ValueType::Position))
					{
						position->m_top = Move(*offset);
					}
					else
					{
						Emplace(ValueType::Position, SizeAxisEdges{Move(*offset), Invalid, Invalid, Invalid});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Position);
					}
				}
			}
			else if (property->name == "bottom")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> offset = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<SizeAxisEdges*> position = Find<SizeAxisEdges>(ValueType::Position))
					{
						position->m_bottom = Move(*offset);
					}
					else
					{
						Emplace(ValueType::Position, SizeAxisEdges{Invalid, Invalid, Move(*offset), Invalid});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::Position);
					}
				}
			}
			else if (property->name == "text")
			{
				if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::Text, dynamicPropertyIdentifier);
				}
				else
				{
					Emplace(ValueType::Text, UnicodeString(property->value));
				}
			}
			else if (property->name == "word-wrap")
			{
				if (property->value == "normal")
				{
					Emplace(ValueType::WordWrapType, WordWrapType::Normal);
				}
				else if (property->value == "break-word")
				{
					Emplace(ValueType::WordWrapType, WordWrapType::BreakWord);
				}
			}
			else if (property->name == "color")
			{
				if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(property->value))
				{
					Emplace(ValueType::Color, dynamicPropertyIdentifier);
				}
				else if (const Optional<Math::Color> color = parseColor(property->value))
				{
					Emplace(ValueType::Color, *color);
				}
			}
			else if (property->name == "font-size")
			{
				if (Optional<SizeAxis> sizeAxis = SizeAxis::Parse(property->value))
				{
					const bool isDynamic = sizeAxis->m_value.Is<ngine::DataSource::PropertyIdentifier>();
					Emplace(ValueType::PointSize, Move(*sizeAxis));
					if (isDynamic)
					{
						OnDynamicValueTypeAdded(ValueType::PointSize);
					}
				}
			}
			else if (property->name == "line-height")
			{
				if (Optional<SizeAxis> sizeAxis = SizeAxis::Parse(property->value))
				{
					const bool isDynamic = sizeAxis->m_value.Is<ngine::DataSource::PropertyIdentifier>();
					Emplace(ValueType::LineHeight, Move(*sizeAxis));
					if (isDynamic)
					{
						OnDynamicValueTypeAdded(ValueType::LineHeight);
					}
				}
				else if (property->value == "normal")
				{
					Emplace(ValueType::LineHeight, SizeAxis{(Math::Ratiof)120_percent});
				}
				else
				{
					Emplace(ValueType::LineHeight, SizeAxis(Math::Ratiof(property->value.ToFloat() * 100.f)));
				}
			}
			else if (property->name == "font-family")
			{
				constexpr ConstStringView assetValuePrefix{"asset("};
				if (property->value.StartsWith(assetValuePrefix))
				{
					const ConstStringView guidString =
						property->value.GetSubstring(assetValuePrefix.GetSize(), property->value.GetSize() - assetValuePrefix.GetSize() - 1);
					if (const Optional<Guid> guid = Guid::TryParse(guidString))
					{
						Emplace(ValueType::FontAsset, Asset::Guid{*guid});
					}
				}
			}
			else if (property->name == "font-weight")
			{
				if (property->value == "normal")
				{
					Emplace(ValueType::FontWeight, Font::Weight{400});
				}
				else if (property->value == "bold")
				{
					Emplace(ValueType::FontWeight, Font::Weight{700});
				}
				else
				{
					const uint16 fontWeight = property->value.ToIntegral<uint16>();
					if (fontWeight > 0)
					{
						Emplace(ValueType::FontWeight, Font::Weight{fontWeight});
					}
				}
			}
			else if (property->name == "font-style")
			{
				if (property->value == "normal")
				{
					Emplace(ValueType::FontModifiers, EnumFlags<Font::Modifier>{Font::Modifier::None});
				}
				else if (property->value == "italic")
				{
					Emplace(ValueType::FontModifiers, EnumFlags<Font::Modifier>{Font::Modifier::Italic});
				}
			}
			else if (property->name == "text-align")
			{
				if (property->value == "left")
				{
					Emplace(ValueType::HorizontalAlignment, Widgets::Alignment::Start);
				}
				else if (property->value == "right")
				{
					Emplace(ValueType::HorizontalAlignment, Widgets::Alignment::End);
				}
				else if (property->value == "center")
				{
					Emplace(ValueType::HorizontalAlignment, Widgets::Alignment::Center);
				}
			}
			else if (property->name == "text-overflow")
			{
				if (property->value == "clip")
				{
					Emplace(ValueType::TextOverflowType, TextOverflowType::Clip);
				}
				else if (property->value == "ellipsis")
				{
					Emplace(ValueType::TextOverflowType, TextOverflowType::Ellipsis);
				}
				else if (property->value == "string")
				{
					Emplace(ValueType::TextOverflowType, TextOverflowType::String);
				}
			}
			else if (property->name == "white-space")
			{
				if (property->value == "normal")
				{
					Emplace(ValueType::WhiteSpaceType, WhiteSpaceType::Normal);
				}
				else if (property->value == "nowrap")
				{
					Emplace(ValueType::WhiteSpaceType, WhiteSpaceType::NoWrap);
				}
				else if (property->value == "pre")
				{
					Emplace(ValueType::WhiteSpaceType, WhiteSpaceType::Pre);
				}
				else if (property->value == "pre-line")
				{
					Emplace(ValueType::WhiteSpaceType, WhiteSpaceType::PreLine);
				}
				else if (property->value == "pre-wrap")
				{
					Emplace(ValueType::WhiteSpaceType, WhiteSpaceType::PreWrap);
				}
			}
			else if (property->name == "vertical-align")
			{
				if (property->value == "top" || property->value == "text-top")
				{
					Emplace(ValueType::VerticalAlignment, Widgets::Alignment::Start);
				}
				if (property->value == "bottom" || property->value == "text-bottom")
				{
					Emplace(ValueType::VerticalAlignment, Widgets::Alignment::End);
				}
				else if (property->value == "middle")
				{
					Emplace(ValueType::VerticalAlignment, Widgets::Alignment::Center);
				}
			}
			else if (property->name == "grid-auto-columns")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> width = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> maximumSize = Find<Size>(ValueType::ChildEntrySize))
					{
						maximumSize->x = Move(*width);
					}
					else
					{
						Emplace(ValueType::ChildEntrySize, Size{Move(*width), 100_percent});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::ChildEntrySize);
					}
				}
			}
			else if (property->name == "grid-auto-rows")
			{
				bool hasDynamicData{false};
				if (Optional<SizeAxisExpression> height = parseSizeAxisExpression(property->value, hasDynamicData))
				{
					if (const Optional<Size*> maximumSize = Find<Size>(ValueType::ChildEntrySize))
					{
						maximumSize->y = Move(*height);
					}
					else
					{
						Emplace(ValueType::ChildEntrySize, Size{100_percent, Move(*height)});
					}
					if (hasDynamicData)
					{
						OnDynamicValueTypeAdded(ValueType::ChildEntrySize);
					}
				}
			}
			else if (property->name == "z-index")
			{
				if (property->value == "auto")
				{
					Clear(ValueType::DepthOffset);
				}
				else
				{
					Emplace(ValueType::DepthOffset, property->value.ToIntegral<int32>());
				}
			}
		}
	}

	[[nodiscard]] String GetDynamicPropertyName(const ngine::DataSource::PropertyIdentifier propertyIdentifier)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		return dataSourceCache.FindPropertyName(propertyIdentifier);
	}

	/* static */ Optional<SizeAxis> SizeAxis::Parse(ConstStringView stringValue)
	{
		if (stringValue.GetSize() == 1 && stringValue[0] == '0')
		{
			return SizeAxis(Math::Zero);
		}
		else if (const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier = parseDynamicProperty(stringValue))
		{
			return SizeAxis(dynamicPropertyIdentifier);
		}
		else if (stringValue.EndsWith("px"))
		{
			stringValue -= 2;
			if (stringValue.Contains('.'))
			{
				return SizeAxis(ReferencePixel::FromValue(stringValue.ToFloat()));
			}
			else
			{
				return SizeAxis(ReferencePixel::FromValue((float)stringValue.ToIntegral<int32>()));
			}
		}
		else if (stringValue.EndsWith("pt"))
		{
			stringValue -= 2;
			if (stringValue.Contains('.'))
			{
				return SizeAxis(Font::Point::FromValue(stringValue.ToFloat()));
			}
			else
			{
				return SizeAxis(Font::Point::FromValue((float)stringValue.ToIntegral<int32>()));
			}
		}
		/* TODO: "em", "cm", "in", "pc" */
		else if (stringValue.EndsWith("%"))
		{
			stringValue -= 1;
			return SizeAxis(Math::Ratiof(stringValue.ToFloat() * 0.01f));
		}
		else if (stringValue.EndsWith("fr"))
		{
			stringValue -= 2;
			return SizeAxis(Math::Ratiof(stringValue.ToFloat()));
		}
		else if (stringValue.EndsWith("vw"))
		{
			stringValue -= 2;
			return SizeAxis(ViewportWidthRatio(stringValue.ToFloat() * 0.01f));
		}
		else if (stringValue.EndsWith("vh"))
		{
			stringValue -= 2;
			return SizeAxis(ViewportHeightRatio(stringValue.ToFloat() * 0.01f));
		}
		else if (stringValue.EndsWith("vmin"))
		{
			stringValue -= 4;
			return SizeAxis(ViewportMinimumRatio(stringValue.ToFloat() * 0.01f));
		}
		else if (stringValue.EndsWith("vmax"))
		{
			stringValue -= 4;
			return SizeAxis(ViewportMaximumRatio(stringValue.ToFloat() * 0.01f));
		}
		else if (stringValue == "auto")
		{
			return SizeAxis(100_percent);
		}
		else if (stringValue == "min-content")
		{
			return SizeAxis(Style::MinContentSize);
		}
		else if (stringValue == "max-content")
		{
			return SizeAxis(Style::MaxContentSize);
		}
		else if (stringValue == "fit-content")
		{
			return SizeAxis(Style::FitContentSize);
		}
		else if (const Optional<ConstStringView> envFunctionStringValue = ParseFunction("env", stringValue))
		{
			if (envFunctionStringValue->StartsWith("safe-area-inset"))
			{
				if (*envFunctionStringValue == "safe-area-inset-left")
				{
					return SizeAxis(Style::SafeAreaInsetLeft);
				}
				else if (*envFunctionStringValue == "safe-area-inset-right")
				{
					return SizeAxis(Style::SafeAreaInsetRight);
				}
				else if (*envFunctionStringValue == "safe-area-inset-top")
				{
					return SizeAxis(Style::SafeAreaInsetTop);
				}
				else if (*envFunctionStringValue == "safe-area-inset-bottom")
				{
					return SizeAxis(Style::SafeAreaInsetBottom);
				}
			}
		}
		else if (auto integerValue = stringValue.TryToIntegral<int32>(); integerValue.success)
		{
			return SizeAxis{(float)integerValue.value};
		}
		else if (auto floatValue = stringValue.TryToFloat(); floatValue.success)
		{
			return SizeAxis{floatValue.value};
		}

		return SizeAxis{Invalid};
	}

	[[nodiscard]] String SizeAxis::ToString() const
	{
		return m_value.Visit(
			[](const ReferencePixel& pixel) -> String
			{
				return String().Format("{}px", pixel.GetPoint());
			},
			[](const PhysicalSizePixel&) -> String
			{
				Assert(false, "Unsupported to string");
				return "0";
			},
			[](const Math::Ratiof ratio) -> String
			{
				return String().Format("{}%", (float)ratio * 100.f);
			},
			[](const PixelValue) -> String
			{
				Assert(false, "Unsupported to string");
				return "0";
			},
			[](const ViewportWidthRatio ratio) -> String
			{
				return String().Format("{}vw", (float)ratio);
			},
			[](const ViewportHeightRatio ratio) -> String
			{
				return String().Format("{}vh", (float)ratio);
			},
			[](const ViewportMinimumRatio ratio) -> String
			{
				return String().Format("{}vmin", (float)ratio);
			},
			[](const ViewportMaximumRatio ratio) -> String
			{
				return String().Format("{}vmax", (float)ratio);
			},
			[](const float value) -> String
			{
				return String().Format("{}", value);
			},
			[](const Font::Point point) -> String
			{
				return String().Format("{}pt", point.GetPoints());
			},
			[](const AutoType) -> String
			{
				return "auto";
			},
			[](const NoMaximumType) -> String
			{
				return "auto";
			},
			[](const MinContentSizeType) -> String
			{
				return "min-content";
			},
			[](const MaxContentSizeType) -> String
			{
				return "max-content";
			},
			[](const FitContentSizeType) -> String
			{
				return "fit-content";
			},
			[](const SafeAreaInsetLeftType) -> String
			{
				return "env(safe-area-inset-left)";
			},
			[](const SafeAreaInsetRightType) -> String
			{
				return "env(safe-area-inset-right)";
			},
			[](const SafeAreaInsetTopType) -> String
			{
				return "env(safe-area-inset-top)";
			},
			[](const SafeAreaInsetBottomType) -> String
			{
				return "env(safe-area-inset-bottom)";
			},
			[](const ngine::DataSource::PropertyIdentifier propertyIdentifier) -> String
			{
				return GetDynamicPropertyName(propertyIdentifier);
			},
			[]() -> String
			{
				return "0px";
			}
		);
	}

	[[nodiscard]] String ToStringInternal(SizeAxisExpressionView& expressions)
	{
		const SizeAxisOperation& operation = expressions[0];
		expressions++;
		if (operation.Is<SizeAxis>())
		{
			return operation.GetExpected<SizeAxis>().ToString();
		}
		else
		{
			String lhs = ToStringInternal(expressions);
			String rhs = ToStringInternal(expressions);
			switch (operation.GetExpected<OperationType>())
			{
				case OperationType::Addition:
					return String().Format("calc({} + {})", lhs, rhs);
				case OperationType::Subtraction:
					return String().Format("calc({} - {})", lhs, rhs);
				case OperationType::Multiplication:
					return String().Format("calc({} * {})", lhs, rhs);
				case OperationType::Division:
					return String().Format("calc({} / {})", lhs, rhs);
				case OperationType::Min:
					return String().Format("min({}, {})", lhs, rhs);
				case OperationType::Max:
					return String().Format("max({}, {})", lhs, rhs);
			}
			ExpectUnreachable();
		}
	}

	[[nodiscard]] String SizeAxisExpressionView::ToString() const
	{
		SizeAxisExpressionView view{*this};
		return ToStringInternal(view);
	}

	[[nodiscard]] String SizeAxisExpression::ToString() const
	{
		return SizeAxisExpressionView{*this}.ToString();
	}

	String Entry::ModifierValues::ToString() const
	{
		String result;

		auto append = [&result](const ConstStringView format)
		{
			if (result.HasElements())
			{
				result += "; ";
			}
			result += format;
		};

		if (Contains(ValueType::LayoutType))
		{
			if (IsDynamic(ValueType::LayoutType))
			{
				String format;
				format.Format(
					"display: {{{}}}",
					GetDynamicPropertyName(m_values[ValueType::LayoutType]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
				append(format);
			}
			else
			{
				switch (m_values[ValueType::LayoutType]->GetExpected<LayoutType>())
				{
					case LayoutType::Flex:
						append("display: flex");
						break;
					case LayoutType::Grid:
						append("display: grid");
						break;
					case LayoutType::None:
						append("display: none");
						break;
					case LayoutType::Block:
						append("display: block");
						break;
				}
			}
		}

		if (Contains(ValueType::Orientation))
		{
			switch (m_values[ValueType::Orientation]->GetExpected<Orientation>())
			{
				case Orientation::Horizontal:
					append("flex-direction: row");
					break;
				case Orientation::Vertical:
					append("flex-direction: column");
					break;
			}
		}

		if (Contains(ValueType::SecondaryDirectionAlignment))
		{
			switch (m_values[ValueType::SecondaryDirectionAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("align-items: flex-start");
					break;
				case Alignment::Center:
					append("align-items: center");
					break;
				case Alignment::End:
					append("align-items: flex-end");
					break;
				case Alignment::Inherit:
					append("align-items: inherit");
					break;
				case Alignment::Stretch:
					append("align-items: stretch");
					break;
			}
		}

		if (Contains(ValueType::PrimaryDirectionAlignment))
		{
			switch (m_values[ValueType::PrimaryDirectionAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("justify-content: flex-start");
					break;
				case Alignment::Center:
					append("justify-content: center");
					break;
				case Alignment::End:
					append("justify-content: flex-end");
					break;
				case Alignment::Inherit:
					append("justify-content: inherit");
					break;
				case Alignment::Stretch:
					append("justify-content: stretch");
					break;
			}
		}

		if (Contains(ValueType::ChildSecondaryAlignment))
		{
			switch (m_values[ValueType::ChildSecondaryAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("align-self: flex-start");
					break;
				case Alignment::Center:
					append("align-self: center");
					break;
				case Alignment::End:
					append("align-self: flex-end");
					break;
				case Alignment::Inherit:
					append("align-self: inherit");
					break;
				case Alignment::Stretch:
					append("align-self: stretch");
					break;
			}
		}

		if (Contains(ValueType::ChildPrimaryAlignment))
		{
			switch (m_values[ValueType::ChildPrimaryAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("justify-self: flex-start");
					break;
				case Alignment::Center:
					append("justify-self: center");
					break;
				case Alignment::End:
					append("justify-self: flex-end");
					break;
				case Alignment::Inherit:
					append("justify-self: inherit");
					break;
				case Alignment::Stretch:
					append("justify-self: stretch");
					break;
			}
		}

		if (Contains(ValueType::Padding))
		{
			const SizeAxisEdges& paddingEdges = m_values[ValueType::Padding]->GetExpected<SizeAxisEdges>();
			String format;
			format.Format(
				"padding: {} {} {} {}",
				paddingEdges.m_top.ToString(),
				paddingEdges.m_right.ToString(),
				paddingEdges.m_bottom.ToString(),
				paddingEdges.m_left.ToString()
			);
			append(format);
		}

		if (Contains(ValueType::Margin))
		{
			const SizeAxisEdges& marginEdges = m_values[ValueType::Margin]->GetExpected<SizeAxisEdges>();
			String format;
			format.Format(
				"margin: {} {} {} {}",
				marginEdges.m_top.ToString(),
				marginEdges.m_right.ToString(),
				marginEdges.m_bottom.ToString(),
				marginEdges.m_left.ToString()
			);
			append(format);
		}

		if (Contains(ValueType::MinimumSize))
		{
			const Size& minimumSize = m_values[ValueType::MinimumSize]->GetExpected<Size>();
			String format;
			if (!minimumSize.x.Is<AutoType>())
			{
				format.Format("min-width: {}", minimumSize.x.ToString());
				append(format);
			}
			if (!minimumSize.y.Is<AutoType>())
			{
				format.Format("min-height: {}", minimumSize.y.ToString());
				append(format);
			}
		}

		if (Contains(ValueType::MaximumSize))
		{
			const Size& maximumSize = m_values[ValueType::MaximumSize]->GetExpected<Size>();
			String format;
			if (!maximumSize.x.Is<AutoType>())
			{
				format.Format("max-width: {}", maximumSize.x.ToString());
				append(format);
			}
			if (!maximumSize.y.Is<AutoType>())
			{
				format.Format("max-height: {}", maximumSize.y.ToString());
				append(format);
			}
		}

		if (Contains(ValueType::PreferredSize))
		{
			const Size& preferredSize = m_values[ValueType::PreferredSize]->GetExpected<Size>();
			String format;
			if (!preferredSize.x.Is<AutoType>())
			{
				format.Format("width: {}", preferredSize.x.ToString());
				append(format);
			}
			if (!preferredSize.y.Is<AutoType>())
			{
				format.Format("height: {}", preferredSize.y.ToString());
				append(format);
			}
		}

		if (Contains(ValueType::PositionType))
		{
			switch (m_values[ValueType::PositionType]->GetExpected<PositionType>())
			{
				case PositionType::Absolute:
					append("position: absolute");
					break;
				case PositionType::Relative:
					append("position: relative");
					break;
				case PositionType::Static:
					append("position: static");
					break;
				case PositionType::Dynamic:
					append("position: dynamic");
					break;
			}
		}

		if (Contains(ValueType::OverflowType))
		{
			switch (m_values[ValueType::OverflowType]->GetExpected<OverflowType>())
			{
				case OverflowType::Auto:
					append("overflow: auto");
					break;
				case OverflowType::Visible:
					append("overflow: visible");
					break;
				case OverflowType::Hidden:
					append("overflow: hidden");
					break;
				case OverflowType::Scroll:
					append("overflow: scroll");
					break;
			}
		}

		if (Contains(ValueType::ElementSizingType))
		{
			switch (m_values[ValueType::ElementSizingType]->GetExpected<ElementSizingType>())
			{
				case ElementSizingType::BorderBox:
					append("box-sizing: border-box");
					break;
				case ElementSizingType::ContentBox:
					append("box-sizing: content-box");
					break;
			}
		}

		if (Contains(ValueType::RoundingRadius))
		{
			const SizeAxisCorners& roundingRadiusCorners = m_values[ValueType::RoundingRadius]->GetExpected<SizeAxisCorners>();
			String format;
			format.Format(
				"border-radius: {} {} {} {}",
				roundingRadiusCorners.m_topLeft.ToString(),
				roundingRadiusCorners.m_topRight.ToString(),
				roundingRadiusCorners.m_bottomRight.ToString(),
				roundingRadiusCorners.m_bottomLeft.ToString()
			);
			append(format);
		}

		if (Contains(ValueType::ChildOffset))
		{
			const Size& childOffset = m_values[ValueType::ChildOffset]->GetExpected<Size>();
			String format;
			format.Format("gap: {}", childOffset.x.ToString());
			append(format);
			format.Format("column-gap: {}", childOffset.x.ToString());
			append(format);
			format.Format("row-gap: {}", childOffset.y.ToString());
			append(format);
		}

		if (Contains(ValueType::ChildEntrySize))
		{
			const Size& childEntrySize = m_values[ValueType::ChildEntrySize]->GetExpected<Size>();
			String format;
			format.Format("grid-auto-columns: {}", childEntrySize.x.ToString());
			append(format);
			format.Format("grid-auto-rows: {}", childEntrySize.y.ToString());
			append(format);
		}

		if (Contains(ValueType::Visibility))
		{
			if (m_values[ValueType::Visibility]->GetExpected<bool>())
			{
				append("visibility: visible");
			}
			else
			{
				append("visibility: hidden");
			}
		}

		if (Contains(ValueType::ChildGrowthFactor))
		{
			String format;
			format.Format("flex-grow: {}", m_values[ValueType::ChildGrowthFactor]->GetExpected<float>());
			append(format);
		}

		if (Contains(ValueType::ChildShrinkFactor))
		{
			String format;
			format.Format("flex-shrink: {}", m_values[ValueType::ChildShrinkFactor]->GetExpected<float>());
			append(format);
		}

		if (Contains(ValueType::Color))
		{
			String format;
			format.Format("color: {}", m_values[ValueType::Color]->GetExpected<Math::Color>());
			append(format);
		}

		if (Contains(ValueType::BackgroundColor))
		{
			String format;
			format.Format("background-color: {}", m_values[ValueType::BackgroundColor]->GetExpected<Math::Color>());
			append(format);
		}

		if (Contains(ValueType::BackgroundLinearGradient))
		{
			const Math::LinearGradient& linearGradient = m_values[ValueType::BackgroundLinearGradient]->GetExpected<Math::LinearGradient>();

			String format = "background: linear-gradient(";

			{
				String angleFormat;
				angleFormat.Format("{}deg", linearGradient.m_orientation.GetDegrees());
				format += angleFormat;
			}

			for (const Math::LinearGradient::Color color : linearGradient.m_colors)
			{
				String colorFormat;
				colorFormat.Format(", {} {}%", color.m_color, color.m_stopPoint * 100.f);
				format += colorFormat;
			}

			format += ")";
			append(format);
		}

		if (Contains(ValueType::BackgroundConicGradient))
		{
			const Math::LinearGradient& conicGradient = m_values[ValueType::BackgroundConicGradient]->GetExpected<Math::LinearGradient>();

			String format = "background: conic-gradient(";

			{
				String angleFormat;
				angleFormat.Format("from {}deg", conicGradient.m_orientation.GetDegrees());
				format += angleFormat;
			}

			for (const Math::LinearGradient::Color color : conicGradient.m_colors)
			{
				String colorFormat;
				colorFormat.Format(", {} {}deg", color.m_color, Math::Anglef::FromRadians(color.m_stopPoint * Math::Constantsf::PI2).GetDegrees());
				format += colorFormat;
			}

			format += ")";
			append(format);
		}

		if (Contains(ValueType::BorderColor))
		{
			String format;
			format.Format("border-color: {}", m_values[ValueType::BorderColor]->GetExpected<Math::Color>());
			append(format);
		}

		if (Contains(ValueType::BorderThickness))
		{
			String format;
			const Style::SizeAxisEdges& sizeAxisEdges = m_values[ValueType::BorderThickness]->GetExpected<Style::SizeAxisEdges>();
			format.Format(
				"border-width: {}",
				sizeAxisEdges.m_bottom.ToString(),
				sizeAxisEdges.m_left.ToString(),
				sizeAxisEdges.m_right.ToString(),
				sizeAxisEdges.m_top.ToString()
			);
			append(format);
		}

		if (Contains(ValueType::Opacity))
		{
			String format;
			format.Format("opacity: {}", (float)m_values[ValueType::Opacity]->GetExpected<Math::Ratiof>());
			append(format);
		}

		if (Contains(ValueType::DepthOffset))
		{
			String format;
			format.Format("z-index: {}", m_values[ValueType::DepthOffset]->GetExpected<int32>());
			append(format);
		}

		if (Contains(ValueType::Position))
		{
			const SizeAxisEdges& position = m_values[ValueType::Position]->GetExpected<SizeAxisEdges>();
			if (position.m_left.IsValid())
			{
				String format;
				format.Format("left: {}", position.m_left.ToString());
				append(format);
			}
			else if (position.m_right.IsValid())
			{
				String format;
				format.Format("right: {}", position.m_right.ToString());
				append(format);
			}
			if (position.m_top.IsValid())
			{
				String format;
				format.Format("top: {}", position.m_top.ToString());
				append(format);
			}
			else if (position.m_bottom.IsValid())
			{
				String format;
				format.Format("bottom: {}", position.m_bottom.ToString());
				append(format);
			}
		}

		if (Contains(ValueType::Text))
		{
			String format;
			if (IsDynamic(ValueType::Text))
			{
				format
					.Format("text: {{{}}}", GetDynamicPropertyName(m_values[ValueType::Text]->GetExpected<ngine::DataSource::PropertyIdentifier>()));
			}
			else
			{
				format.Format("text: {}", m_values[ValueType::Text]->GetExpected<UnicodeString>());
			}
			append(format);
		}

		if (Contains(ValueType::FontAsset))
		{
			String format;
			format.Format("font-family: asset({})", m_values[ValueType::FontAsset]->GetExpected<Asset::Guid>().ToString());
			append(format);
		}

		if (Contains(ValueType::HorizontalAlignment))
		{
			switch (m_values[ValueType::HorizontalAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("text-align: left");
					break;
				case Alignment::Center:
					append("text-align: center");
					break;
				case Alignment::End:
					append("text-align: right");
					break;
				case Alignment::Inherit:
					append("text-align: inherit");
					break;
				case Alignment::Stretch:
					append("text-align: stretch");
					break;
			}
		}

		if (Contains(ValueType::VerticalAlignment))
		{
			switch (m_values[ValueType::VerticalAlignment]->GetExpected<Alignment>())
			{
				case Alignment::Start:
					append("vertical-align: top");
					break;
				case Alignment::Center:
					append("vertical-align: middle");
					break;
				case Alignment::End:
					append("vertical-align: bottom");
					break;
				case Alignment::Inherit:
					append("vertical-align: inherit");
					break;
				case Alignment::Stretch:
					append("vertical-align: stretch");
					break;
			}
		}

		if (Contains(ValueType::WrapType))
		{
			switch (m_values[ValueType::WrapType]->GetExpected<WrapType>())
			{
				case WrapType::NoWrap:
					append("flex-wrap: nowrap");
					break;
				case WrapType::Wrap:
					append("flex-wrap: wrap");
					break;
				case WrapType::WrapReverse:
					append("flex-wrap: wrap-reverse");
					break;
			}
		}

		if (Contains(ValueType::WordWrapType))
		{
			switch (m_values[ValueType::WordWrapType]->GetExpected<WordWrapType>())
			{
				case WordWrapType::Normal:
					append("word-wrap: normal");
					break;
				case WordWrapType::BreakWord:
					append("word-wrap: break-word");
					break;
			}
		}

		if (Contains(ValueType::WhiteSpaceType))
		{
			switch (m_values[ValueType::WhiteSpaceType]->GetExpected<WhiteSpaceType>())
			{
				case WhiteSpaceType::Normal:
					append("white-space: normal");
					break;
				case WhiteSpaceType::NoWrap:
					append("white-space: nowrap");
					break;
				case WhiteSpaceType::Pre:
					append("white-space: pre");
					break;
				case WhiteSpaceType::PreLine:
					append("white-space: pre-line");
					break;
				case WhiteSpaceType::PreWrap:
					append("white-space: pre-wrap");
					break;
			}
		}

		if (Contains(ValueType::TextOverflowType))
		{
			switch (m_values[ValueType::TextOverflowType]->GetExpected<TextOverflowType>())
			{
				case TextOverflowType::Clip:
					append("text-overflow: clip");
					break;
				case TextOverflowType::Ellipsis:
					append("text-overflow: ellipsis");
					break;
				case TextOverflowType::String:
					append("text-overflow: string");
					break;
			}
		}

		if (Contains(ValueType::FontModifiers))
		{
			const EnumFlags<Font::Modifier> fontModifiers = m_values[ValueType::FontModifiers]->GetExpected<EnumFlags<Font::Modifier>>();
			if (fontModifiers.IsSet(Font::Modifier::Italic))
			{
				append("font-style: italic");
			}
			else if (fontModifiers.IsSet(Font::Modifier::Italic))
			{
				append("font-style: normal");
			}
		}

		if (Contains(ValueType::PointSize))
		{
			const SizeAxis& pointSize = m_values[ValueType::PointSize]->GetExpected<SizeAxis>();
			String format;
			format.Format("font-size: {}", pointSize.ToString());
			append(format);
		}

		if (Contains(ValueType::FontWeight))
		{
			const Font::Weight& fontWeight = m_values[ValueType::FontWeight]->GetExpected<Font::Weight>();
			String format;
			format.Format("font-weight: {}", fontWeight.GetValue());
			append(format);
		}

		if (Contains(ValueType::LineHeight))
		{
			const SizeAxis& lineHeight = m_values[ValueType::PointSize]->GetExpected<SizeAxis>();
			String format;
			format.Format("line-height: {}", lineHeight.ToString());
			append(format);
		}

		if (Contains(ValueType::AssetIdentifier))
		{
			String format;
			if (IsDynamic(ValueType::AssetIdentifier))
			{
				format.Format(
					"background: asset({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::AssetIdentifier]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				format.Format("background: asset({})", m_values[ValueType::AssetIdentifier]->GetExpected<Asset::Guid>());
			}
			append(format);
		}

		if (Contains(ValueType::DraggableAsset))
		{
			String format;
			if (IsDynamic(ValueType::DraggableAsset))
			{
				format.Format(
					"draggable-asset: asset({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::DraggableAsset]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				format.Format("draggable-asset: asset({})", m_values[ValueType::DraggableAsset]->GetExpected<Asset::Guid>());
			}
			append(format);
		}

		if (Contains(ValueType::AttachedAsset))
		{
			String format;
			if (IsDynamic(ValueType::AttachedAsset))
			{
				format.Format(
					"attached-asset: asset({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::AttachedAsset]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				format.Format("attached-asset: asset({})", m_values[ValueType::AttachedAsset]->GetExpected<Asset::Guid>());
			}
			append(format);
		}

		if (Contains(ValueType::DraggableComponent))
		{
			String format;
			if (IsDynamic(ValueType::DraggableComponent))
			{
				format.Format(
					"draggable-asset: component({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::DraggableComponent]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				Assert(false, "TODO");
			}
			append(format);
		}

		if (Contains(ValueType::AttachedComponent))
		{
			String format;
			if (IsDynamic(ValueType::AttachedComponent))
			{
				format.Format(
					"attached-asset: component({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::AttachedComponent]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				Assert(false, "TODO");
			}
			append(format);
		}

		if (Contains(ValueType::AttachedDocumentAsset))
		{
			String format;
			if (IsDynamic(ValueType::AttachedDocumentAsset))
			{
				format.Format(
					"attached-asset: document({{{}}})",
					GetDynamicPropertyName(m_values[ValueType::AttachedDocumentAsset]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				Assert(false, "TODO");
			}
			append(format);
		}

		if (Contains(ValueType::EnableEditing))
		{
			String format;
			if (IsDynamic(ValueType::EnableEditing))
			{
				format.Format(
					"edit_document: {{{}}})",
					GetDynamicPropertyName(m_values[ValueType::EnableEditing]->GetExpected<ngine::DataSource::PropertyIdentifier>())
				);
			}
			else
			{
				format.Format("edit_document: {}", m_values[ValueType::EnableEditing]->GetExpected<bool>());
			}
			append(format);
		}

		return Move(result);
	}

	const Entry& ComputedStylesheet::GetEntry(const Guid id) const
	{
		auto it = m_entries.Find(id);
		if (it != m_entries.end())
		{
			return it->second;
		}

		static Entry dummyEntry;
		return dummyEntry;
	}

	Optional<const Entry*> ComputedStylesheet::FindEntry(const Guid id) const
	{
		auto it = m_entries.Find(id);
		if (it != m_entries.end())
		{
			return it->second;
		}
		return Invalid;
	}

	Optional<Entry*> ComputedStylesheet::FindEntry(const Guid id)
	{
		auto it = m_entries.Find(id);
		if (it != m_entries.end())
		{
			return it->second;
		}
		return Invalid;
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Style::Size>
	{
		inline static constexpr auto Type =
			Reflection::Reflect<Widgets::Style::Size>("{CA5D8587-70A4-4DB9-A1A6-560DCD0D800F}"_guid, MAKE_UNICODE_LITERAL("2D Size"));
	};

	[[maybe_unused]] const bool wasSizeTypeRegistered = Reflection::Registry::RegisterType<Widgets::Style::Size>();
}

namespace ngine
{
	template struct UnorderedMap<Guid, Widgets::Style::Entry, Guid::Hash>;
}
