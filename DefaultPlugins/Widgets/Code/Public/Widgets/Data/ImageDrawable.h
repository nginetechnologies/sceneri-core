#pragma once

#include <Widgets/Data/Drawable.h>

#include <Common/Math/Color.h>
#include <Common/Asset/Guid.h>
#include <Common/Storage/Identifier.h>
#include <Common/Function/EventCallbackResult.h>

#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/AtomicPtr.h>

namespace ngine::Rendering
{
	struct RenderTexture;
	struct LogicalDevice;
	struct MipMask;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Widgets::Data
{
	struct ImageDrawable final : public Drawable
	{
		using BaseType = Drawable;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(
				BaseType&& initializer,
				const Math::Color color = "#FFFFFF"_colorf,
				const Rendering::TextureIdentifier textureIdentifier = {},
				const Style::SizeAxisExpression roundingRadius = 0_ratio
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_color(color)
				, m_textureIdentifier(textureIdentifier)
				, m_roundingRadius(roundingRadius)
			{
			}

			Math::Color m_color;
			Rendering::TextureIdentifier m_textureIdentifier;
			Style::SizeAxisExpression m_roundingRadius{0_ratio};
		};

		ImageDrawable(Initializer&& initializer);
		ImageDrawable(const ImageDrawable&) = delete;
		ImageDrawable& operator=(const ImageDrawable&) = delete;
		ImageDrawable(ImageDrawable&&) = delete;
		ImageDrawable& operator=(ImageDrawable&&) = delete;
		virtual ~ImageDrawable() = default;

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
		virtual uint32 GetMaximumPushConstantInstanceCount([[maybe_unused]] const Widget& owner) const override
		{
			return 1;
		}
		virtual void OnStyleChanged(
			Widget& owner,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) override;
		[[nodiscard]] virtual LoadResourcesResult TryLoadResources(Widget& owner) override;

		void SetColor(const Math::Color color)
		{
			m_color = color;
		}
		[[nodiscard]] Math::Color GetColor()
		{
			return m_color;
		}

		void SetRoundingRadius(const Style::SizeAxisExpressionView roundingRadius)
		{
			m_roundingRadius = roundingRadius;
		}

		void ChangeImage(Widget& owner, const Rendering::TextureIdentifier textureIdentifier);
		[[nodiscard]] Rendering::TextureIdentifier GetTextureIdentifier() const
		{
			return m_textureIdentifier;
		}
	protected:
		EventCallbackResult
		OnTextureLoadedAsync(Widget& owner, Rendering::LogicalDevice& logicalDevice, const Rendering::TextureIdentifier identifier, const Rendering::RenderTexture& texture, Rendering::MipMask changedMipValues, const EnumFlags<Rendering::LoadedTextureFlags>);
	protected:
		Threading::Mutex m_textureLoadMutex;

		Math::Color m_color;
		Rendering::TextureIdentifier m_textureIdentifier;
		Style::SizeAxisExpression m_roundingRadius{0_ratio};

		Rendering::DescriptorSet m_descriptorSet;
		Threading::Atomic<Threading::EngineJobRunnerThread*> m_pDescriptorSetLoadingThread{nullptr};
		Rendering::ImageMapping m_textureView;
		Rendering::Sampler m_sampler;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ImageDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ImageDrawable>(
			"09E05359-2503-4413-B636-62F57BB123F9"_guid,
			MAKE_UNICODE_LITERAL("Image Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
