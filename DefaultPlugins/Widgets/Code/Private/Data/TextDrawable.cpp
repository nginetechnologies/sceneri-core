#include "Data/TextDrawable.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Pipelines/Pipelines.h>
#include <Widgets/Style/Entry.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/Point.h>
#include <Widgets/LoadResourcesResult.h>

#include <Common/Math/Color.h>
#include <Common/Math/Vector2/Ceil.h>
#include <Common/Reflection/Registry.inl>

#include <FontRendering/Manager.h>
#include <FontRendering/Font.h>
#include <FontRendering/FontAtlas.h>
#include <FontRendering/FontAssetType.h>

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Assets/Texture/RenderTexture.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Primitives/Serialization/RectangleEdges.h>

#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Widgets::Data
{
	[[nodiscard]] Font::Identifier FindOrRegisterFont(const Asset::Guid fontGuid)
	{
		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
		Font::Cache& fontCache = fontManager.GetCache();
		return fontCache.FindOrRegisterAsset(fontGuid);
	}

	[[nodiscard]] Font::InstanceIdentifier FindOrRegisterFontInstance(
		Rendering::ToolWindow& owningWindow,
		const Font::Identifier fontIdentifier,
		const Font::Point fontSize,
		const Font::Weight fontWeight,
		const EnumFlags<Font::Modifier> fontModifiers
	)
	{
		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
		Font::Cache& fontCache = fontManager.GetCache();

		return fontCache.FindOrRegisterInstance(Font::InstanceProperties{
			fontIdentifier,
			fontSize,
			owningWindow.GetDevicePixelRatio() * DpiReference,
			fontModifiers,
			fontWeight,
			Font::DefaultCharacters
		});
	}

	[[nodiscard]] Font::Point GetFontSizeAsPoint(const Style::SizeAxisExpression fontSize, const Rendering::ScreenProperties screenProperties)
	{
		// TODO: Percentage in font-size means % of first other font-size in hierarchy.
		const int32 parentFontSize = 0;
		const float sizeInDevicePoints = fontSize.GetPoint(parentFontSize, screenProperties);

		return Point::FromPixel(sizeInDevicePoints / screenProperties.m_devicePixelRatio);
	}

	TextDrawable::TextDrawable(Initializer&& initializer)
		: m_text(Move(initializer.m_text))
		, m_fontIdentifier(initializer.m_fontIdentifier)
		, m_fontSize(initializer.m_fontSize)
		, m_lineHeight(initializer.m_lineHeight)
		, m_fontWeight(initializer.m_fontWeight)
		, m_fontModifiers(initializer.m_fontModifiers)
		, m_color(initializer.m_color)
		, m_horizontalAlignment(initializer.m_horizontalAlignment)
		, m_verticalAlignment(initializer.m_verticalAlignment)
		, m_fontInstanceIdentifier(FindOrRegisterFontInstance(
				*initializer.GetParent().GetOwningWindow(),
				m_fontIdentifier,
				GetFontSizeAsPoint(initializer.m_fontSize, initializer.GetParent().GetOwningWindow()->GetCurrentScreenProperties()),
				m_fontWeight,
				m_fontModifiers
			))
		, m_wordWrapType(initializer.m_wordWrap)
		, m_whitespaceType(initializer.m_whitespaceType)
		, m_textOverflowType(initializer.m_textOverflowType)
		, m_linearGradient(initializer.m_gradient)
	{
		UpdateDisplayedText(initializer.GetParent());
	}

	void TextDrawable::OnDestroying(Widget& owner)
	{
		Rendering::ToolWindow& owningWindow = *owner.GetOwningWindow();
		Rendering::LogicalDevice& logicalDevice = owningWindow.GetLogicalDevice();
		m_fontImageMapping.Destroy(logicalDevice);

		if (Threading::JobRunnerThread* pDescriptorSetLoadingThread = m_pDescriptorLoadingThread.Exchange(nullptr))
		{
			Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*pDescriptorSetLoadingThread);
			thread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_fontDescriptorSet));
		}

		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
		Font::Cache& fontCache = fontManager.GetCache();

		if (const Optional<const Font::Atlas*> pFontAtlas = fontCache.GetInstanceAtlas(m_fontInstanceIdentifier))
		{
			const Rendering::TextureIdentifier atlasTextureIdentifier = pFontAtlas->GetTextureIdentifier();
			if (atlasTextureIdentifier.IsValid() && owner.HasLoadedResources())
			{
				[[maybe_unused]] const bool wasRemoved = System::Get<Rendering::Renderer>().GetTextureCache().RemoveRenderTextureListener(
					logicalDevice.GetIdentifier(),
					atlasTextureIdentifier,
					this
				);
				Assert(wasRemoved);
			}
		}
	}

	Vector<ConstUnicodeStringView> SplitTextIntoWords(const ConstUnicodeStringView text)
	{
		// TODO: Deal with multiple whitespaces and ignore them
		Vector<ConstUnicodeStringView> words;
		uint32 index = 0u;
		uint32 offset = 0u;
		while (text.GetSize() > offset)
		{
			offset = text.FindFirstOf(MAKE_UNICODE_LITERAL(' '), offset);
			words.EmplaceBack(text.GetSubstring(index, offset - index));

			if (offset == StringView::InvalidPosition)
			{
				break;
			}

			offset++;
			index = offset;
		}

		return words;
	}

	void TextDrawable::WrapTextToLines(const Widget& owner)
	{
		if (m_pFontAtlas == nullptr)
		{
			return;
		}

		m_lines.Clear();

		uint32 ownerWidth = owner.GetSize().x;

		const OverflowType overflowType = owner.GetStyle().GetWithDefault<OverflowType>(
			Style::ValueTypeIdentifier::OverflowType,
			OverflowType(DefaultOverflowType),
			owner.GetStyle().GetMatchingModifiers(owner.GetActiveModifiers())
		);

		// Both of the following properties are required for text-overflow to work
		// https://www.w3schools.com/cssref/css3_pr_text-overflow.php
		if (overflowType == OverflowType::Hidden && m_whitespaceType == WhiteSpaceType::NoWrap)
		{
			switch (m_textOverflowType)
			{
				case TextOverflowType::Ellipsis:
				{
					ownerWidth = owner.GetParentAvailableChildContentArea().GetSize().x;
					const uint32 width = m_pFontAtlas->CalculateWidth(m_text);
					if (width > ownerWidth)
					{
						UnicodeString text{m_text};
						const UnicodeStringView newText = m_pFontAtlas->TrimStringToFit(text, ownerWidth);
						text.Resize(newText.GetSize());

						m_lines.EmplaceBack(Move(text));

						return;
					}
					else
					{
						// Whole text fits into one line
						m_lines.EmplaceBack(m_text);
						return;
					}
				}
				case TextOverflowType::Clip:
				{
					// Just add the whole text as one line
					// The renderer will automatically cut the line when drawing over the widget
					m_lines.EmplaceBack(m_text);
					return;
				}
				case TextOverflowType::String:
				{
					// TODO: Implement
					NotImplemented("Not currently supported");
					return;
				}
			}
		}

		const Vector<ConstUnicodeStringView> words = SplitTextIntoWords(m_text);
		const uint32 wordCount = words.GetSize();

		static auto finishLine = [](Vector<UnicodeString>& lines, UnicodeString& line, uint32& lineWidth)
		{
			if (!line.IsEmpty())
			{
				lines.EmplaceBack(Move(line));
				lineWidth = 0u;
			}
		};

		static auto addWhitespace = [](const Font::Atlas& fontAtlas, UnicodeString& line, uint32& lineWidth)
		{
			line += ConstUnicodeStringView(MAKE_UNICODE_LITERAL(" "));
			lineWidth += fontAtlas.CalculateWidth(ConstUnicodeStringView(MAKE_UNICODE_LITERAL(" ")));
		};

		static auto addWordToLine = [](const ConstUnicodeStringView word, const uint32 wordWidth, UnicodeString& line, uint32& lineWidth)
		{
			lineWidth += wordWidth;
			line += word;
		};

		static auto breakWordToLines = [](
																		 const Font::Atlas& fontAtlas,
																		 const uint32 ownerWidth,
																		 const ConstUnicodeStringView word,
																		 Vector<UnicodeString>& lines,
																		 UnicodeString& line,
																		 uint32& lineWidth
																	 )
		{
			// Iterate through word and break when the line runs out of space
			for (uint32 j = 0; j < word.GetSize(); ++j)
			{
				const ConstUnicodeStringView character(&word[j], 1);
				const uint32 characterWidth = fontAtlas.CalculateWidth(character);
				if (lineWidth + characterWidth > ownerWidth)
				{
					finishLine(lines, line, lineWidth);

					addWordToLine(character, characterWidth, line, lineWidth);
				}
				else
				{
					addWordToLine(character, characterWidth, line, lineWidth);
				}
			}

			// After done with splitting end a whitespace to finish the word
			addWhitespace(fontAtlas, line, lineWidth);
		};

		uint32 lineWidth = 0u;
		UnicodeString line;
		for (uint32 i = 0; i < wordCount; ++i)
		{
			const ConstUnicodeStringView word = words[i];
			const uint32 wordWidth = m_pFontAtlas->CalculateWidth(word);
			// Word is too big to fit into one line.
			if (wordWidth > ownerWidth)
			{
				if (m_wordWrapType == WordWrapType::BreakWord)
				{
					breakWordToLines(*m_pFontAtlas, ownerWidth, word, m_lines, line, lineWidth);
				}
				else if (m_wordWrapType == WordWrapType::Normal)
				{
					// Finish current line
					finishLine(m_lines, line, lineWidth);

					// Normal style will just use the whole line for the word even if the line is too short
					// The word will be cut off when rendered
					m_lines.EmplaceBack(word);
				}
			}
			// Is there enough space in the current line
			else if (lineWidth + wordWidth > ownerWidth)
			{
				finishLine(m_lines, line, lineWidth);

				addWordToLine(word, wordWidth, line, lineWidth);

				addWhitespace(*m_pFontAtlas, line, lineWidth);
			}
			else
			{
				addWordToLine(word, wordWidth, line, lineWidth);

				addWhitespace(*m_pFontAtlas, line, lineWidth);
			}
		}
		if (line.HasElements())
		{
			m_lines.EmplaceBack(Move(line));
		}

		// Make sure we reclamp the line offset
		SetLineOffset(m_lineOffset);
	}

	bool TextDrawable::ShouldDrawCommands(const Widget&, const Rendering::Pipelines& pipelines) const
	{
		return pipelines.GetFontPipeline().IsValid() & m_fontDescriptorSet.IsValid() && m_fontImageMapping.IsValid() && m_pFontAtlas != nullptr;
	}

	void TextDrawable::RecordDrawCommands(
		const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const Math::Rectangleui renderViewport,
		Math::Vector2f startPositionShaderSpace,
		[[maybe_unused]] const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		Threading::SharedLock textLock(m_textMutex);
		if (m_lines.IsEmpty())
		{
			return;
		}

		const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
		// TODO: Percentage in font-size means % of first other font-size in hierarchy.
		const int32 parentFontSize = 0;
		const int32 fontSize = m_fontSize.Get(parentFontSize, screenProperties);
		const int32 lineHeight = m_lineHeight.Get(fontSize, screenProperties);

		const Math::Rectanglei ownerContentArea = owner.GetContentArea();
		const Math::Vector2i lineSize{ownerContentArea.GetSize().x, int32(ownerContentArea.GetSize().y / m_lines.GetSize())};

		const int32 lineIndexOffset = {m_lineOffset};

		for (const UnicodeString& lineToDraw : m_lines)
		{
			const Math::Vector2ui textSize = m_pFontAtlas ? m_pFontAtlas->CalculateSize(lineToDraw) : Math::Vector2ui(Math::Zero);
			const int32 lineIndex = lineIndexOffset + (int32)m_lines.GetIteratorIndex(&lineToDraw);

			Math::Rectanglei lineContentArea{ownerContentArea.GetPosition() + Math::Vector2i{0, lineHeight * lineIndex}, lineSize};

			auto getPosition = [lineContentArea, textSize](const uint8 axis, const Widgets::Alignment alignment) -> int32
			{
				switch (alignment)
				{
					case Widgets::Alignment::Start:
					case Widgets::Alignment::Stretch:
						return lineContentArea.GetPosition()[axis];
					case Widgets::Alignment::Center:
						return lineContentArea.GetCenterPosition()[axis] -
						       (int32)Math::Ceil(
										 (float)textSize[axis] * 0.5f
									 ); // (int32)Math::Ceil((float)ownerContentAreaGetSize()[axis] * 0.5f) - (int32)Math::Ceil((float)textSize[axis] * 0.5f);
					case Widgets::Alignment::End:
						return lineContentArea.GetEndPosition()[axis] - textSize[axis];
					default:
						ExpectUnreachable();
				}
			};

			const Math::Vector2i textPosition{getPosition(0, m_horizontalAlignment), getPosition(1, m_verticalAlignment)};
			const Math::Vector2f startPositionRatio = startPositionShaderSpace +
			                                          (((Math::Vector2f)textPosition - (Math::Vector2f)owner.GetPosition()) /
			                                           (Math::Vector2f)renderViewport.GetSize()) *
			                                            2.f;

			if (m_linearGradient.IsValid())
			{
				const Font::FontGradientPipeline& fontPipeline = pipelines.GetFontGradientPipeline();
				renderCommandEncoder.BindPipeline(fontPipeline);
				fontPipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					*m_pFontAtlas,
					m_fontDescriptorSet,
					lineToDraw,
					startPositionRatio,
					Math::Vector2f{1.f, 1.f} / (Math::Vector2f)renderViewport.GetSize(),
					m_color,
					owner.GetDepthRatio(),
					m_linearGradient
				);
			}
			else
			{
				const Font::FontPipeline& fontPipeline = pipelines.GetFontPipeline();
				renderCommandEncoder.BindPipeline(fontPipeline);
				fontPipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					*m_pFontAtlas,
					m_fontDescriptorSet,
					lineToDraw,
					startPositionRatio,
					Math::Vector2f{1.f, 1.f} / (Math::Vector2f)renderViewport.GetSize(),
					m_color,
					owner.GetDepthRatio()
				);
			}
		}
	}

	uint32 TextDrawable::GetMaximumPushConstantInstanceCount([[maybe_unused]] const Widget& owner) const
	{
		return m_lines.GetView().Count(
			[](const ConstUnicodeStringView line)
			{
				return line.GetSize();
			}
		);
	}

	static TIdentifierArray<Rendering::Sampler, Rendering::LogicalDeviceIdentifier> FontSamplers;

	// TODO: Need to figure out a way to call Font::Cache::RemoveInstance for m_fontInstanceIdentifier
	// Problem is that it is shared.
	LoadResourcesResult TextDrawable::TryLoadResources(Widget& owner)
	{
		if (m_fontInstanceIdentifier.IsInvalid())
		{
			return LoadResourcesResult::Status::Invalid;
		}

		Assert(owner.HasStartedLoadingResources());
		Rendering::ToolWindow& owningWindow = *owner.GetOwningWindow();
		const Rendering::ScreenProperties screenProperties = owningWindow.GetCurrentScreenProperties();
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();

		Rendering::Renderer& renderer = owningWindow.GetLogicalDevice().GetRenderer();
		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
		Font::Cache& fontCache = fontManager.GetCache();

		Entity::ComponentSoftReference softReference{owner, sceneRegistry};
		Threading::JobBatch loadFontInstanceBatch = fontCache.TryLoadInstance(
			m_fontInstanceIdentifier,
			Font::InstanceProperties{
				m_fontIdentifier,
				GetFontSizeAsPoint(m_fontSize, screenProperties),
				owningWindow.GetDevicePixelRatio(),
				m_fontModifiers,
				m_fontWeight,
				Font::DefaultCharacters
			},
			System::Get<Asset::Manager>(),
			[&fontCache, softReference, &sceneRegistry](const Font::InstanceIdentifier fontInstanceIdentifier)
			{
				if (const Optional<Widget*> pOwner = softReference.Find<Widget>(sceneRegistry))
				{
					if (const Optional<TextDrawable*> pTextDrawable = pOwner->FindDataComponentOfType<TextDrawable>(sceneRegistry))
					{
						const Font::Atlas& fontAtlas = *fontCache.GetInstanceAtlas(fontInstanceIdentifier);
						pTextDrawable->m_pFontAtlas = &fontAtlas;
						pTextDrawable->WrapTextToLines(*pOwner);
					}
				}
			}
		);

		const Optional<const Font::Atlas*> pAtlas = fontCache.GetInstanceAtlas(m_fontInstanceIdentifier);
		const Rendering::TextureIdentifier atlasTextureIdentifier = pAtlas->GetTextureIdentifier();

		Rendering::TextureCache& textureCache = renderer.GetTextureCache();
		Optional<Threading::Job*> pLoadAtlasRenderTextureJob = textureCache.GetOrLoadRenderTexture(
			owningWindow.GetLogicalDevice().GetIdentifier(),
			atlasTextureIdentifier,
			Rendering::ImageMappingType::TwoDimensional,
			Rendering::AllMips,
			Rendering::TextureLoadFlags::Default & ~Rendering::TextureLoadFlags::LoadDummy,
			Rendering::TextureCache::TextureLoadListenerData{
				*this,
				[softReference,
		     &sceneRegistry](TextDrawable&, Rendering::LogicalDevice& logicalDevice, const Rendering::TextureIdentifier, const Rendering::RenderTexture& texture, Rendering::MipMask, const EnumFlags<Rendering::LoadedTextureFlags>)
					-> EventCallbackResult
				{
					if (const Optional<Widget*> pOwner = softReference.Find<Widget>(sceneRegistry))
					{
						if (const Optional<TextDrawable*> pTextDrawable = pOwner->FindDataComponentOfType<TextDrawable>(sceneRegistry))
						{
							Threading::EngineJobRunnerThread& jobRunnerThread =
								static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());

							// Create the descriptor and sampler
							const Rendering::DescriptorSetLayoutView descriptorLayout = pOwner->GetOwningWindow()->GetSampledImageDescriptorLayout();

							Rendering::DescriptorSet descriptorSet;
							const Rendering::DescriptorPoolView descriptorPool =
								jobRunnerThread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
							const bool allocatedDescriptorSets = descriptorPool.AllocateDescriptorSets(
								logicalDevice,
								ArrayView<const Rendering::DescriptorSetLayoutView, uint8>(descriptorLayout),
								ArrayView<Rendering::DescriptorSet, uint8>(descriptorSet)
							);
							Assert(allocatedDescriptorSets);
							if (LIKELY(allocatedDescriptorSets))
							{
								Rendering::ImageMapping imageMapping(
									logicalDevice,
									texture,
									Rendering::ImageMapping::Type::TwoDimensional,
									texture.GetFormat(),
									Rendering::ImageAspectFlags::Color,
									texture.GetAvailableMipRange(),
									Rendering::ArrayRange{0, texture.GetTotalArrayCount()}
								);

								Rendering::Sampler& fontSampler = FontSamplers[logicalDevice.GetIdentifier()];
								if (!fontSampler.IsValid())
								{
									fontSampler = Rendering::Sampler(logicalDevice);
								}

								Array<Rendering::DescriptorSet::ImageInfo, 2> imageInfo{
									Rendering::DescriptorSet::ImageInfo{{}, imageMapping, Rendering::ImageLayout::ShaderReadOnlyOptimal},
									Rendering::DescriptorSet::ImageInfo{fontSampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
								};
								Array<Rendering::DescriptorSet::UpdateInfo, 2> descriptorUpdates{
									Rendering::DescriptorSet::UpdateInfo{
										descriptorSet,
										0,
										0,
										Rendering::DescriptorType::SampledImage,
										ArrayView<const Rendering::DescriptorSet::ImageInfo>(imageInfo[0])
									},
									Rendering::DescriptorSet::UpdateInfo{
										descriptorSet,
										1,
										0,
										Rendering::DescriptorType::Sampler,
										ArrayView<const Rendering::DescriptorSet::ImageInfo>(imageInfo[1])
									}
								};
								Rendering::DescriptorSet::Update(logicalDevice, descriptorUpdates);

								pTextDrawable->m_fontImageMapping.AtomicSwap(imageMapping);
								pTextDrawable->m_fontDescriptorSet.AtomicSwap(descriptorSet);
								Threading::JobRunnerThread* pPreviousDescriptorLoadingThread =
									pTextDrawable->m_pDescriptorLoadingThread.Exchange(&jobRunnerThread);

								pOwner->OnLoadedResources();

								pTextDrawable->UpdateDisplayedText(*pOwner);

								if (pOwner->HasParent())
								{
									pOwner->GetParent().NotifyOnChildPreferredSizeChanged(*pOwner, sceneRegistry);
								}

								if (pPreviousDescriptorLoadingThread != nullptr)
								{
									Threading::EngineJobRunnerThread& previousEngineThread =
										static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
									previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(descriptorSet));

									Threading::EngineJobRunnerThread& thread =
										static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
									thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(imageMapping));
								}
								return EventCallbackResult::Keep;
							}
							else
							{
								Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = pTextDrawable->m_pDescriptorLoadingThread.Exchange(nullptr);
								if (pPreviousDescriptorLoadingThread != nullptr)
								{
									Threading::EngineJobRunnerThread& previousEngineThread =
										static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
									previousEngineThread.GetRenderData()
										.DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(pTextDrawable->m_fontDescriptorSet));

									Threading::EngineJobRunnerThread& thread =
										static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
									thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(pTextDrawable->m_fontImageMapping));
								}

								pOwner->ResetLoadedResources();
								return EventCallbackResult::Remove;
							}
						}
					}
					return EventCallbackResult::Remove;
				}
			}
		);
		if (loadFontInstanceBatch.IsValid() && pLoadAtlasRenderTextureJob != nullptr)
		{
			loadFontInstanceBatch.QueueAsNewFinishedStage(*pLoadAtlasRenderTextureJob);
			return loadFontInstanceBatch;
		}
		else if (pLoadAtlasRenderTextureJob != nullptr)
		{
			return Threading::JobBatch{*pLoadAtlasRenderTextureJob};
		}
		else
		{
			Assert(!loadFontInstanceBatch.IsValid());
			return Threading::JobBatch();
		}
	}

	void TextDrawable::OnDisplayPropertiesChanged(Widget& owner)
	{
		const Font::InstanceIdentifier newInstanceIdentifier = FindOrRegisterFontInstance(
			*owner.GetOwningWindow(),
			m_fontIdentifier,
			GetFontSizeAsPoint(m_fontSize, owner.GetOwningWindow()->GetCurrentScreenProperties()),
			m_fontWeight,
			m_fontModifiers
		);
		if (newInstanceIdentifier != m_fontInstanceIdentifier)
		{
			m_fontInstanceIdentifier = newInstanceIdentifier;
			owner.InvalidateLoadedResources();
		}
	}

	void TextDrawable::OnContentAreaChanged(Widget& owner, const EnumFlags<ContentAreaChangeFlags>)
	{
		UpdateDisplayedText(owner);
	}

	void TextDrawable::OnStyleChanged(
		Widget& owner,
		const Style::CombinedEntry& styleEntry,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		bool changedAny = false;

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::PointSize))
		{
			Style::SizeAxis newFontSize =
				styleEntry.GetWithDefault<Style::SizeAxis>(Style::ValueTypeIdentifier::PointSize, 16_px, matchingModifiers);
			changedAny |= m_fontSize != newFontSize;
			m_fontSize = newFontSize;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::FontWeight))
		{
			const Font::Weight newFontWeight = styleEntry.GetWithDefault<Font::Weight>(
				Style::ValueTypeIdentifier::FontWeight,
				Font::Weight{Font::DefaultFontWeight},
				matchingModifiers
			);
			changedAny |= newFontWeight != m_fontWeight;
			m_fontWeight = newFontWeight;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::FontModifiers))
		{
			const EnumFlags<Font::Modifier> newFontModifiers =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::FontModifiers, EnumFlags<Font::Modifier>(), matchingModifiers);
			changedAny |= newFontModifiers != m_fontModifiers;
			m_fontModifiers = newFontModifiers;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundLinearGradient))
		{
			m_linearGradient =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundLinearGradient, Math::LinearGradient(), matchingModifiers);
			changedAny = true;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::FontAsset))
		{
			const Font::Identifier newFontIdentifier = FindOrRegisterFont(
				styleEntry.GetWithDefault<Asset::Guid>(Style::ValueTypeIdentifier::FontAsset, Asset::Guid(DefaultFontAssetGuid), matchingModifiers)
			);
			changedAny |= newFontIdentifier != m_fontIdentifier;
			m_fontIdentifier = newFontIdentifier;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::HorizontalAlignment))
		{
			const Widgets::Alignment newHorizontalAlignment =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::HorizontalAlignment, Widgets::Alignment::Start, matchingModifiers);
			changedAny |= newHorizontalAlignment != m_horizontalAlignment;
			m_horizontalAlignment = newHorizontalAlignment;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::VerticalAlignment))
		{
			const Widgets::Alignment newVerticalAlignment =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::VerticalAlignment, Widgets::Alignment::Center, matchingModifiers);
			changedAny |= newVerticalAlignment != m_verticalAlignment;
			m_verticalAlignment = newVerticalAlignment;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Text))
		{
			if (m_isTextFromStyle)
			{
				const Optional<const Style::EntryValue*> textStyleEntry = styleEntry.Find(Style::ValueTypeIdentifier::Text, matchingModifiers);
				if (textStyleEntry.IsValid())
				{
					const Optional<const UnicodeString*> newText = textStyleEntry->Get<UnicodeString>();
					if (newText.IsValid() && *newText != GetText())
					{
						changedAny = true;
						SetTextInternal(owner, *newText);
					}
				}
				else if (GetText().HasElements())
				{
					changedAny = true;
					SetTextInternal(owner, ConstUnicodeStringView{});
				}
			}
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Color))
		{
			const Math::Color newColor =
				styleEntry.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::Color, Math::Color(DefaultColor), matchingModifiers);
			changedAny |= newColor != m_color;
			m_color = newColor;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Opacity))
		{
			if (const Optional<const Math::Ratiof*> pOpacity = styleEntry.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
			{
				const float newAlpha = *pOpacity;
				changedAny |= newAlpha != m_color.a;
				m_color.a = newAlpha;
			}
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::WordWrapType))
		{
			if (const Optional<const WordWrapType*> pWordWrap = styleEntry.Find<WordWrapType>(Style::ValueTypeIdentifier::WordWrapType, matchingModifiers))
			{
				const WordWrapType newWordWrap = *pWordWrap;
				changedAny |= newWordWrap != m_wordWrapType;
				m_wordWrapType = newWordWrap;
			}
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::LineHeight))
		{
			Style::SizeAxis newLineHeight =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::LineHeight, Style::SizeAxis{120_percent}, matchingModifiers);
			changedAny |= m_lineHeight != newLineHeight;
			m_lineHeight = newLineHeight;
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::TextOverflowType))
		{
			if (const Optional<const TextOverflowType*> pTextOverflowType = styleEntry.Find<TextOverflowType>(Style::ValueTypeIdentifier::TextOverflowType, matchingModifiers))
			{
				const TextOverflowType newTextOverflowType = *pTextOverflowType;
				changedAny |= newTextOverflowType != m_textOverflowType;
				m_textOverflowType = newTextOverflowType;
			}
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::WhiteSpaceType))
		{
			if (const Optional<const WhiteSpaceType*> pWhiteSpaceType = styleEntry.Find<WhiteSpaceType>(Style::ValueTypeIdentifier::WhiteSpaceType, matchingModifiers))
			{
				const WhiteSpaceType newWhiteSpaceType = *pWhiteSpaceType;
				changedAny |= newWhiteSpaceType != m_whitespaceType;
				m_whitespaceType = newWhiteSpaceType;
			}
		}

		if (changedAny)
		{
			// TODO: Remove the old instance
			const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
			const Font::InstanceIdentifier newInstanceIdentifier = FindOrRegisterFontInstance(
				*owner.GetOwningWindow(),
				m_fontIdentifier,
				GetFontSizeAsPoint(m_fontSize, screenProperties),
				m_fontWeight,
				m_fontModifiers
			);
			if (newInstanceIdentifier != m_fontInstanceIdentifier)
			{
				m_fontInstanceIdentifier = newInstanceIdentifier;
				[[maybe_unused]] const bool didReload = owner.TryReloadResources(owner.GetSceneRegistry());
			}

			WrapTextToLines(owner);
		}
	}

	void TextDrawable::SetText(Widget& owner, const ConstUnicodeStringView text)
	{
		m_isTextFromStyle = false;
		SetTextInternal(owner, text);
	}

	void TextDrawable::SetTextInternal(Widget& owner, const ConstUnicodeStringView text)
	{
		if (m_text == text)
		{
			return;
		}

		const Math::Vector2ui textSize = CalculateTotalSize(owner);

		m_text = text;
		UpdateDisplayedText(owner);

		const Math::Vector2ui newTextSize = CalculateTotalSize(owner);
		if ((textSize != newTextSize).AreAnySet() && owner.HasParent())
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			owner.GetParent().NotifyChildMaximumSizeChanged(owner, sceneRegistry);
		}
	}

	void TextDrawable::SetText(Widget& owner, UnicodeString&& text)
	{
		m_isTextFromStyle = false;
		SetTextInternal(owner, Forward<UnicodeString>(text));
	}

	void TextDrawable::SetTextFromProperty(Widgets::Widget& owner, UnicodeString text)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Text, Move(text)});
	}

	void TextDrawable::SetTextInternal(Widget& owner, UnicodeString&& text)
	{
		if (m_text == text)
		{
			return;
		}

		const Math::Vector2ui textSize = CalculateTotalSize(owner);

		m_text = Forward<UnicodeString>(text);
		UpdateDisplayedText(owner);

		const Math::Vector2ui newTextSize = CalculateTotalSize(owner);
		if ((textSize != newTextSize).AreAnySet() && owner.HasParent())
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			owner.GetParent().NotifyChildMaximumSizeChanged(owner, sceneRegistry);
		}
	}

	void TextDrawable::SetFontFromProperty(Widgets::Widget& owner, Asset::Picker font)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::FontAsset, Asset::Guid(font.GetAssetGuid())});
	}

	Asset::Picker TextDrawable::GetFontFromProperty() const
	{
		return {
			m_fontIdentifier.IsValid() ? System::FindPlugin<Font::Manager>()->GetCache().GetAssetGuid(m_fontIdentifier) : Asset::Guid{},
			FontAssetType::AssetFormat.assetTypeGuid
		};
	}

	void TextDrawable::SetFontSizeFromProperty(Widgets::Widget& owner, Style::SizeAxisExpression size)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::PointSize, *size.Get()});
	}

	void TextDrawable::SetFontWeightFromProperty(Widgets::Widget& owner, Font::Weight weight)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::FontWeight, weight});
	}

	void TextDrawable::SetLineHeightFromProperty(Widgets::Widget& owner, Style::SizeAxisExpression size)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::LineHeight, *size.Get()});
	}

	void TextDrawable::SetHorizontalAlignmentFromProperty(Widgets::Widget& owner, Widgets::Alignment alignment)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::HorizontalAlignment, alignment});
	}

	void TextDrawable::SetVerticalAlignmentFromProperty(Widgets::Widget& owner, Widgets::Alignment alignment)
	{
		owner.EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::VerticalAlignment, alignment});
	}

	void TextDrawable::UpdateDisplayedText(Widget& owner)
	{
		Threading::UniqueLock lock(m_textMutex);

		if (m_pFontAtlas)
		{
			WrapTextToLines(owner);
		}
	}

	uint32 TextDrawable::CalculatePerLineHeight(const Widget& owner) const
	{
		const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
		// TODO: Percentage in font-size means % of first other font-size in hierarchy.
		const int32 parentFontSize = 0;
		const int32 fontSize = m_fontSize.Get(parentFontSize, screenProperties);
		return m_lineHeight.Get(fontSize, screenProperties);
	}

	uint32 TextDrawable::CalculateTotalLineHeight(const Widget& owner) const
	{
		return CalculatePerLineHeight(owner) * m_lines.GetSize();
	}

	uint32 TextDrawable::CalculateTotalWidth(const Widget&) const
	{
		if (m_pFontAtlas != nullptr && m_pFontAtlas->HasLoadedGlyphs())
		{
			uint32 width = 0;
			for (const ConstUnicodeStringView line : m_lines)
			{
				const uint32 lineWidth = m_pFontAtlas->CalculateWidth(line);
				width = Math::Max(width, lineWidth);
			}
			return width;
		}
		else
		{
			return 0;
		}
	}

	Math::Vector2ui TextDrawable::CalculateTotalSize(const Widget& owner) const
	{
		return {CalculateTotalWidth(owner), CalculateTotalLineHeight(owner)};
	}

	[[maybe_unused]] const bool wasTextDrawableTypeRegistered = Reflection::Registry::RegisterType<TextDrawable>();
	[[maybe_unused]] const bool wasTextDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<TextDrawable>>::Make());
}
namespace ngine::Widgets
{
	//! Helper widget to spawn a text
	//! Never instantiated, substituted with an asset on disk
	struct TextWidget final : public Widget
	{
		using BaseType = Widget;
		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::TextWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::TextWidget>(
			"9f908f18-be28-4b44-b452-66153ab120e9"_guid,
			MAKE_UNICODE_LITERAL("Text"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{},
			Events{},
			Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"b1de7612-e16b-9b8d-9ad3-c9db7a591c41"_asset,
				"138411d8-ced1-431d-a5ef-ba44f2960dea"_guid,
				"214e0b97-53b2-4101-b7e4-ab19833b4a3b"_asset,
				Reflection::GetTypeGuid<Widgets::Widget>()
			}}
		);
	};
}

namespace ngine::Widgets
{
	[[maybe_unused]] const bool wasTextTypeRegistered = Reflection::Registry::RegisterType<TextWidget>();
	[[maybe_unused]] const bool wasTextComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<TextWidget>>::Make());
}
