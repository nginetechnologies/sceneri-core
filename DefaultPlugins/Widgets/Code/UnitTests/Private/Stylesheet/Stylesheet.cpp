#include <Common/Memory/New.h>

#include <Common/Tests/UnitTest.h>

#include <Widgets/Style/ComputedStylesheet.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Widget.h>

#include <Common/Math/Color.h>

namespace ngine::Widgets::Style::Tests
{
	UNIT_TEST(Widgets, ParseStylesheet)
	{
		ComputedStylesheet stylesheet;
		stylesheet.ParseFromCSS(R"(
a9a7e81a-c396-4084-a5e8-3e1574d526fd {
  background-color: #FF0000;
}

036e0b49-9d4c-41e5-903a-bf50195d1c52 {
  color: #00FF00;
  margin-left: 40px;
}
)");

		EXPECT_EQ(stylesheet.GetEntryCount(), 2u);

		{
			const Optional<const Entry*> firstEntry = stylesheet.FindEntry("a9a7e81a-c396-4084-a5e8-3e1574d526fd"_guid);
			EXPECT_TRUE(firstEntry.IsValid());
			if (firstEntry.IsValid())
			{
				const Widgets::Style::MatchingEntryModifiers matchingModifiers = firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::None);
				{
					const Optional<const Widgets::Style::EntryValue*> backgroundColorProperty =
						firstEntry->Find(Widgets::Style::ValueTypeIdentifier::BackgroundColor, matchingModifiers.GetView());
					EXPECT_TRUE(backgroundColorProperty.IsValid());
					if (backgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(backgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(backgroundColorProperty->GetExpected<Math::Color>(), "#FF0000"_colorf);
					}
				}
			}
		}

		{
			const Optional<const Entry*> secondEntry = stylesheet.FindEntry("036e0b49-9d4c-41e5-903a-bf50195d1c52"_guid);
			EXPECT_TRUE(secondEntry.IsValid());
			if (secondEntry.IsValid())
			{
				const Widgets::Style::MatchingEntryModifiers matchingModifiers = secondEntry->GetMatchingModifiers(Widgets::Style::Modifier::None);
				{
					const Optional<const Widgets::Style::EntryValue*> colorProperty =
						secondEntry->Find(Widgets::Style::ValueTypeIdentifier::Color, matchingModifiers.GetView());
					EXPECT_TRUE(colorProperty.IsValid());
					if (colorProperty.IsValid())
					{
						EXPECT_TRUE(colorProperty->Is<Math::Color>());
						EXPECT_EQ(colorProperty->GetExpected<Math::Color>(), "#00FF00"_colorf);
					}
				}

				{
					const Optional<const Widgets::Style::EntryValue*> marginProperty =
						secondEntry->Find(Widgets::Style::ValueTypeIdentifier::Margin, matchingModifiers.GetView());
					EXPECT_TRUE(marginProperty.IsValid());
					if (marginProperty.IsValid())
					{
						EXPECT_TRUE(marginProperty->Is<SizeAxisEdges>());
						EXPECT_TRUE(marginProperty->GetExpected<SizeAxisEdges>().m_left.Is<ReferencePixel>());
						EXPECT_EQ(marginProperty->GetExpected<SizeAxisEdges>().m_left.Get()->m_value.GetExpected<ReferencePixel>(), 40_px);
					}
				}
			}
		}
	}

	UNIT_TEST(Widgets, ParseStylesheetWithComments)
	{
		ComputedStylesheet stylesheet;

		stylesheet.ParseFromCSS(R"( /* start comment
*/a9a7e81a-c396-4084-a5e8-3e1574d526fd /* before block start comment */{
 /* test */
  background-color: /* comment

*/ #FF0000 /*yep*/; /* my note*/
  /* padding-right: 1px */
}

a9a7e81a-c396-4084-a5e8-3e1574d526fd /* comment */: /* selector comment */ active /* post comment */ : /*third*/ hover
  {
  }

/*036e0b49-9d4c-41e5-903a-bf50195d1c52 {
  color: #00FF00;
  margin-left: 40px;
}*/
)");

		EXPECT_EQ(stylesheet.GetEntryCount(), 1u);

		{
			const Optional<const Entry*> firstEntry = stylesheet.FindEntry("a9a7e81a-c396-4084-a5e8-3e1574d526fd"_guid);
			EXPECT_TRUE(firstEntry.IsValid());
			if (firstEntry.IsValid())
			{
				const Widgets::Style::MatchingEntryModifiers matchingModifiers = firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::None);
				{
					const Optional<const Widgets::Style::EntryValue*> backgroundColorProperty =
						firstEntry->Find(Widgets::Style::ValueTypeIdentifier::BackgroundColor, matchingModifiers.GetView());
					EXPECT_TRUE(backgroundColorProperty.IsValid());
					if (backgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(backgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(backgroundColorProperty->GetExpected<Math::Color>(), "#FF0000"_colorf);
					}
				}
			}
		}
	}

	UNIT_TEST(Widgets, ParseStylesheetWithModifiers)
	{
		ComputedStylesheet stylesheet;
		stylesheet.ParseFromCSS(R"(
a9a7e81a-c396-4084-a5e8-3e1574d526fd {
  background-color: #FF0000;
}

a9a7e81a-c396-4084-a5e8-3e1574d526fd:hover {
  background-color: #00FF00;
}

a9a7e81a-c396-4084-a5e8-3e1574d526fd:active {
  background-color: #0000FF;
}

a9a7e81a-c396-4084-a5e8-3e1574d526fd:active:hover {
  background-color: #DEDEDE;
}
)");

		EXPECT_EQ(stylesheet.GetEntryCount(), 1u);

		{
			const Optional<const Entry*> firstEntry = stylesheet.FindEntry("a9a7e81a-c396-4084-a5e8-3e1574d526fd"_guid);
			EXPECT_TRUE(firstEntry.IsValid());
			if (firstEntry.IsValid())
			{
				{
					const Optional<const Widgets::Style::EntryValue*> mainBackgroundColorProperty = firstEntry->Find(
						Widgets::Style::ValueTypeIdentifier::BackgroundColor,
						firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::None).GetView()
					);
					EXPECT_TRUE(mainBackgroundColorProperty.IsValid());
					if (mainBackgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(mainBackgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(mainBackgroundColorProperty->GetExpected<Math::Color>(), "#FF0000"_colorf);
					}
				}

				{
					const Optional<const Widgets::Style::EntryValue*> hoverBackgroundColorProperty = firstEntry->Find(
						Widgets::Style::ValueTypeIdentifier::BackgroundColor,
						firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::Hover).GetView()
					);
					EXPECT_TRUE(hoverBackgroundColorProperty.IsValid());
					if (hoverBackgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(hoverBackgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(hoverBackgroundColorProperty->GetExpected<Math::Color>(), "#00FF00"_colorf);
					}
				}

				{
					const Optional<const Widgets::Style::EntryValue*> activeBackgroundColorProperty = firstEntry->Find(
						Widgets::Style::ValueTypeIdentifier::BackgroundColor,
						firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::Active).GetView()
					);
					EXPECT_TRUE(activeBackgroundColorProperty.IsValid());
					if (activeBackgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(activeBackgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(activeBackgroundColorProperty->GetExpected<Math::Color>(), "#0000FF"_colorf);
					}
				}

				{
					const Optional<const Widgets::Style::EntryValue*> activeAndHoverBackgroundColorProperty = firstEntry->Find(
						Widgets::Style::ValueTypeIdentifier::BackgroundColor,
						firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::Active | Widgets::Style::Modifier::Hover).GetView()
					);
					EXPECT_TRUE(activeAndHoverBackgroundColorProperty.IsValid());
					if (activeAndHoverBackgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(activeAndHoverBackgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(activeAndHoverBackgroundColorProperty->GetExpected<Math::Color>(), "#DEDEDE"_colorf);
					}
				}

				{
					// Check for a modifier combination that doesn't exist
					// Should result in the main value being returned
					const Optional<const Widgets::Style::EntryValue*> defaultBackgroundColorProperty = firstEntry->Find(
						Widgets::Style::ValueTypeIdentifier::BackgroundColor,
						firstEntry->GetMatchingModifiers(Widgets::Style::Modifier::Disabled).GetView()
					);
					EXPECT_TRUE(defaultBackgroundColorProperty.IsValid());
					if (defaultBackgroundColorProperty.IsValid())
					{
						EXPECT_TRUE(defaultBackgroundColorProperty->Is<Math::Color>());
						EXPECT_EQ(defaultBackgroundColorProperty->GetExpected<Math::Color>(), "#FF0000"_colorf);
					}
				}
			}
		}
	}
}
