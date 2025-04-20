#pragma once

#include <Widgets/Data/Drawable.h>

#include <Widgets/Alignment.h>
#include <Widgets/Style/SizeAxis.h>
#include <Widgets/WordWrapType.h>
#include <Widgets/TextOverflowType.h>
#include <Widgets/WhiteSpaceType.h>

#include <Common/Asset/Picker.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Function/Function.h>
#include <Common/Undo/UndoHistory.h>
#include <Common/Math/Color.h>
#include <Common/Math/LinearGradient.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicPtr.h>

#include <FontRendering/FontIdentifier.h>
#include <FontRendering/FontInstanceIdentifier.h>
#include <FontRendering/FontModifier.h>
#include <FontRendering/FontWeight.h>

#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/ImageMapping.h>

namespace ngine::Widgets::Style
{
	struct InstanceProperties;
}

namespace ngine::Font
{
	struct Atlas;
}

namespace ngine::Threading
{
	struct JobRunnerThread;
}

namespace ngine::Widgets::Data
{
	struct TextDrawable final : public Drawable
	{
		using BaseType = Drawable;

		inline static constexpr Math::Color DefaultColor = "#C3C3C3"_colorf;
		inline static constexpr Asset::Guid DefaultFontAssetGuid = "73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(
				BaseType&& initializer,
				UnicodeString&& text,
				const Font::Identifier fontIdentifier,
				const Style::SizeAxis fontSize = 7_pt,
				const Style::SizeAxis lineHeight = Math::Ratiof(120_percent),
				const Font::Weight fontWeight = Font::Weight(400),
				const WordWrapType wordWrapType = WordWrapType::Normal,
				const EnumFlags<Font::Modifier> fontModifiers = {},
				const Math::Color color = DefaultColor,
				const Widgets::Alignment horizontalAlignment = Widgets::Alignment::Start,
				const Widgets::Alignment verticalAlignment = Widgets::Alignment::Center,
				Widgets::TextOverflowType textOverflowType = Widgets::TextOverflowType::Clip,
				Widgets::WhiteSpaceType whitespaceType = Widgets::WhiteSpaceType::Normal,
				Math::LinearGradient linearGradient = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_text(Forward<UnicodeString>(text))
				, m_fontIdentifier(fontIdentifier)
				, m_fontSize(fontSize)
				, m_lineHeight(lineHeight)
				, m_fontWeight(fontWeight)
				, m_wordWrap(wordWrapType)
				, m_fontModifiers(fontModifiers)
				, m_color(color)
				, m_horizontalAlignment(horizontalAlignment)
				, m_verticalAlignment(verticalAlignment)
				, m_textOverflowType(textOverflowType)
				, m_whitespaceType(whitespaceType)
				, m_gradient(linearGradient)
			{
			}

			UnicodeString m_text;
			Font::Identifier m_fontIdentifier;
			Style::SizeAxis m_fontSize;
			Style::SizeAxis m_lineHeight;
			Font::Weight m_fontWeight;
			WordWrapType m_wordWrap;
			EnumFlags<Font::Modifier> m_fontModifiers;
			Math::Color m_color = DefaultColor;
			Widgets::Alignment m_horizontalAlignment = Widgets::Alignment::Start;
			Widgets::Alignment m_verticalAlignment = Widgets::Alignment::Center;
			Widgets::TextOverflowType m_textOverflowType = Widgets::TextOverflowType::Clip;
			Widgets::WhiteSpaceType m_whitespaceType = Widgets::WhiteSpaceType::Normal;
			Math::LinearGradient m_gradient;
		};

		TextDrawable(Initializer&& initializer);
		TextDrawable(const TextDrawable&) = delete;
		TextDrawable& operator=(const TextDrawable&) = delete;
		TextDrawable(TextDrawable&&) = delete;
		TextDrawable& operator=(TextDrawable&&) = delete;
		virtual ~TextDrawable() = default;

		void OnDestroying(Widget& owner);

		[[nodiscard]] virtual bool ShouldDrawCommands(const Widget& owner, const Rendering::Pipelines& pipelines) const override;
		virtual void RecordDrawCommands(
			const Widget& owner,
			const Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Math::Rectangleui,
			const Math::Vector2f startPositionShaderSpace,
			const Math::Vector2f endPositionShaderSpace,
			const Rendering::Pipelines& pipelines
		) const override;
		virtual uint32 GetMaximumPushConstantInstanceCount(const Widget& owner) const override;
		virtual void OnStyleChanged(
			Widget& owner,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) override;
		virtual void OnContentAreaChanged(Widget& owner, const EnumFlags<ContentAreaChangeFlags>) override;
		[[nodiscard]] virtual LoadResourcesResult TryLoadResources(Widget& owner) override;
		void OnDisplayPropertiesChanged(Widget& owner);

		[[nodiscard]] Optional<const Font::Atlas*> GetFontAtlas() const
		{
			return m_pFontAtlas;
		}

		[[nodiscard]] uint32 CalculatePerLineHeight(const Widget& owner) const;
		[[nodiscard]] uint32 CalculateTotalLineHeight(const Widget& owner) const;
		[[nodiscard]] uint32 CalculateTotalWidth(const Widget& owner) const;
		[[nodiscard]] Math::Vector2ui CalculateTotalSize(const Widget& owner) const;

		void SetText(Widget& owner, const ConstUnicodeStringView text);
		void SetText(Widget& owner, UnicodeString&& text);
		void UpdateDisplayedText(Widget& owner);
		[[nodiscard]] ConstUnicodeStringView GetText() const
		{
			return m_text;
		}

		struct ConstTextView
		{
			ConstTextView() = default;
			ConstTextView(Threading::SharedMutex& mutex, const ConstUnicodeStringView view)
				: m_lock(mutex)
				, m_view(view)
			{
				if (UNLIKELY_ERROR(!m_lock.IsLocked()))
				{
					m_view = {};
				}
			}

			[[nodiscard]] operator ConstUnicodeStringView() const LIFETIME_BOUND
			{
				return m_view;
			}
		private:
			Threading::SharedLock<Threading::SharedMutex> m_lock;
			ConstUnicodeStringView m_view;
		};

		[[nodiscard]] Style::SizeAxisExpression GetFontSize() const
		{
			return m_fontSize;
		}
		void SetFontSize(const Style::SizeAxis size)
		{
			m_fontSize = size;
		}
		void SetWeight(const Font::Weight weight)
		{
			m_fontWeight = weight;
		}
		void SetModifiers(const EnumFlags<Font::Modifier> modifiers)
		{
			m_fontModifiers = modifiers;
		}
		void SetColor(const Math::Color color)
		{
			m_color = color;
		}
		[[nodiscard]] Math::Color GetColor() const
		{
			return m_color;
		}
		void SetHorizontalAlignment(const Widgets::Alignment alignment)
		{
			m_horizontalAlignment = alignment;
		}
		void SetVerticalAlignment(const Widgets::Alignment alignment)
		{
			m_verticalAlignment = alignment;
		}
		void SetLineOffset(const int32 offset)
		{
			m_lineOffset = -Math::Clamp(offset, 0, (int32)m_lines.GetSize());
		}
		[[nodiscard]] uint32 GetLineCount() const
		{
			return m_lines.GetSize();
		}
	protected:
		friend struct Reflection::ReflectedType<TextDrawable>;

		void SetTextInternal(Widget& owner, const ConstUnicodeStringView text);
		void SetTextInternal(Widget& owner, UnicodeString&& text);

		void WrapTextToLines(const Widget& owner);

		void SetTextFromProperty(Widgets::Widget& owner, UnicodeString text);
		[[nodiscard]] UnicodeString GetTextFromProperty() const
		{
			return m_text;
		}
		void SetFontFromProperty(Widgets::Widget& owner, Asset::Picker font);
		[[nodiscard]] Asset::Picker GetFontFromProperty() const;
		void SetFontSizeFromProperty(Widgets::Widget& owner, Style::SizeAxisExpression size);
		[[nodiscard]] Style::SizeAxisExpression GetFontSizeFromProperty() const
		{
			return m_fontSize;
		}
		void SetFontWeightFromProperty(Widgets::Widget& owner, Font::Weight weight);
		[[nodiscard]] Font::Weight GetFontWeightFromProperty() const
		{
			return m_fontWeight;
		}
		void SetLineHeightFromProperty(Widgets::Widget& owner, Style::SizeAxisExpression size);
		[[nodiscard]] Style::SizeAxisExpression GetLineHeightFromProperty() const
		{
			return m_lineHeight;
		}
		void SetHorizontalAlignmentFromProperty(Widgets::Widget& owner, Widgets::Alignment alignment);
		[[nodiscard]] Widgets::Alignment GetHorizontalAlignmentFromProperty() const
		{
			return m_horizontalAlignment;
		}
		void SetVerticalAlignmentFromProperty(Widgets::Widget& owner, Widgets::Alignment alignment);
		[[nodiscard]] Widgets::Alignment GetVerticalAlignmentFromProperty() const
		{
			return m_verticalAlignment;
		}
	protected:
		Vector<UnicodeString> m_lines;
		int32 m_lineOffset{0};
		UnicodeString m_text;
		bool m_isTextFromStyle{true};
		Font::Identifier m_fontIdentifier;
		Style::SizeAxisExpression m_fontSize;
		Style::SizeAxisExpression m_lineHeight;
		Font::Weight m_fontWeight;
		EnumFlags<Font::Modifier> m_fontModifiers;
		Optional<const Font::Atlas*> m_pFontAtlas;
		Math::Color m_color = DefaultColor;
		Widgets::Alignment m_horizontalAlignment = Widgets::Alignment::Start;
		Widgets::Alignment m_verticalAlignment = Widgets::Alignment::Center;
		Font::InstanceIdentifier m_fontInstanceIdentifier;
		WordWrapType m_wordWrapType = WordWrapType::Normal;
		WhiteSpaceType m_whitespaceType = WhiteSpaceType::Normal;
		TextOverflowType m_textOverflowType = TextOverflowType::Clip;
		Math::LinearGradient m_linearGradient;

		mutable Threading::SharedMutex m_textMutex;

		Rendering::ImageMapping m_fontImageMapping;
		Rendering::DescriptorSet m_fontDescriptorSet;
		Threading::Atomic<Threading::JobRunnerThread*> m_pDescriptorLoadingThread = nullptr;
		Rendering::Sampler m_fontSampler;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::TextDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::TextDrawable>(
			"{2A275411-D02B-4203-9ED7-D971AEF0C05A}"_guid,
			MAKE_UNICODE_LITERAL("Text Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableDeletionFromUserInterface,
			Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Text"),
					"text",
					"368a76ee-1cdb-44fe-bcad-944a69d11646"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetTextFromProperty,
					&Widgets::Data::TextDrawable::GetTextFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Font"),
					"font",
					"ed545a68-3648-4fd7-b3f2-0bd939016eb1"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetFontFromProperty,
					&Widgets::Data::TextDrawable::GetFontFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Font Size"),
					"fontSize",
					"98806616-369b-4bfe-b1b1-308b2c4296e2"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetFontSizeFromProperty,
					&Widgets::Data::TextDrawable::GetFontSizeFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Font Weight"),
					"fontWeight",
					"d01082ff-8b61-4317-b382-1f691b5745cb"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetFontWeightFromProperty,
					&Widgets::Data::TextDrawable::GetFontWeightFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Line Height"),
					"lineHeight",
					"5e687f01-df6d-4ce3-9dc5-910c23727685"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetLineHeightFromProperty,
					&Widgets::Data::TextDrawable::GetLineHeightFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Horizontal Alignment"),
					"horizontalAlignment",
					"de4b2913-4c29-42ab-b909-84d54cbc134e"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetHorizontalAlignmentFromProperty,
					&Widgets::Data::TextDrawable::GetHorizontalAlignmentFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Vertical Alignment"),
					"verticalAlignment",
					"728beee7-5e00-44f1-b84e-d93495db65f5"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Data::TextDrawable::SetVerticalAlignmentFromProperty,
					&Widgets::Data::TextDrawable::GetVerticalAlignmentFromProperty
				)
			}
		);
	};
}
