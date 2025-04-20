#pragma once

#include <FontRendering/FontIdentifier.h>
#include <FontRendering/Point.h>

#include <Engine/Entity/StaticMeshComponent.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Math/Color.h>

namespace ngine
{
	struct Scene3D;
	struct SceneOctreeNode;
	struct RootSceneOctreeNode;
};

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct RenderViewThreadJob;
	struct Window;
	struct StaticMesh;
	struct Vertex;
}

namespace ngine::Font
{
	struct TextComponent : public Entity::StaticMeshComponent
	{
		static constexpr Guid TypeGuid = "7a085871-3b1d-431e-8f16-8d4ef7ba61bb"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = StaticMeshComponent;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = TextComponent::BaseType::Initializer;

			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				UnicodeString&& text = MAKE_UNICODE_LITERAL("My Text"),
				const Math::Color color = "#FFFFFF"_colorf,
				const Point fontSize = 20_pt,
				const Identifier fontIdentifier = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_text(Forward<UnicodeString>(text))
				, m_color(color)
				, m_fontSize(fontSize)
				, m_fontIdentifier(fontIdentifier)
			{
			}

			UnicodeString m_text{MAKE_UNICODE_LITERAL("My Text")};
			Math::Color m_color = "#FFFFFF"_colorf;
			Point m_fontSize = 20_pt;
			Identifier m_fontIdentifier;
		};

		TextComponent(const TextComponent& templateComponent, const Cloner& cloner);
		TextComponent(Initializer&& initializer);
		TextComponent(const Deserializer& deserializer);
		virtual ~TextComponent();

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		void SetText(const ConstUnicodeStringView text)
		{
			m_text = text;
			OnTextChanged();
		}
		[[nodiscard]] ConstUnicodeStringView GetText() const
		{
			return m_text;
		}
		void SetColor(const Math::Color color)
		{
			m_color = color;
			OnFontColorChanged();
		}
		[[nodiscard]] Math::Color GetTextColor() const
		{
			return m_color;
		}
		void SetFont(const Identifier identifier)
		{
			m_fontIdentifier = identifier;
			OnFontChanged();
		}
		[[nodiscard]] Identifier GetFontIdentifier() const
		{
			return m_fontIdentifier;
		}
		void SetFontSize(const Point fontSize)
		{
			m_fontSize = fontSize;
			OnFontSizeChanged();
		}
		[[nodiscard]] Point GetFontSize() const
		{
			return m_fontSize;
		}
	private:
		friend struct Reflection::ReflectedType<ngine::Font::TextComponent>;
		TextComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);

		using FontPicker = Asset::Picker;
		void SetFontAsset(const FontPicker asset);
		FontPicker GetFont() const;

		void OnTextPropertiesChanged();
		void OnTextChanged()
		{
			OnTextPropertiesChanged();
		}
		void OnFontChanged()
		{
			OnTextPropertiesChanged();
		}
		void OnFontSizeChanged()
		{
			OnTextPropertiesChanged();
		}
		void OnFontColorChanged()
		{
			OnTextPropertiesChanged();
		}
	protected:
		UnicodeString m_text{MAKE_UNICODE_LITERAL("My Text")};
		Math::Color m_color = "#FFFFFF"_colorf;
		Point m_fontSize = 20_pt;
		Identifier m_fontIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Font::TextComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Font::TextComponent>(
			Font::TextComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Text"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Text"),
					"text",
					"{8C31B50B-6935-40AE-ADB0-7BF504AB2CF0}"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					&Font::TextComponent::m_text,
					&Font::TextComponent::OnTextChanged
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Font"),
					"font",
					"{F1F0726F-ECD8-47E4-A6B6-3A3C1142B00B}"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					&Font::TextComponent::SetFontAsset,
					&Font::TextComponent::GetFont
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Size"),
					"size",
					"{3CCF31FC-C0D6-43E6-AF13-679C3D1E8416}"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					&Font::TextComponent::m_fontSize,
					&Font::TextComponent::OnFontSizeChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{3A8B3283-A7B7-4B66-9E28-B533D47F980D}"_guid,
					MAKE_UNICODE_LITERAL("Text"),
					&Font::TextComponent::m_color,
					&Font::TextComponent::OnFontColorChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "b1de7612-e16b-9b8d-9ad3-c9db7a591c41"_asset, "cb5a6f40-b060-440e-b205-0f0942b3d93e"_guid
			}}
		);
	};
}
