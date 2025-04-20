#include "Data/ImageDrawable.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Widget.inl>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/SizeCorners.h>
#include <Widgets/LoadResourcesResult.h>

#include "Pipelines/Pipelines.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Common/System/Query.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Assets/Texture/RenderTexture.h>

namespace ngine::Widgets::Data
{
	ImageDrawable::ImageDrawable(Initializer&& initializer)
		: m_color(initializer.m_color)
		, m_textureIdentifier(initializer.m_textureIdentifier)
		, m_roundingRadius(initializer.m_roundingRadius)
	{
		initializer.GetParent().ResetLoadedResources();
	}

	void ImageDrawable::OnDestroying(Widget& owner)
	{
		const Rendering::LogicalDevice& logicalDevice = owner.GetOwningWindow()->GetLogicalDevice();

		Threading::UniqueLock lock(m_textureLoadMutex);
		if (m_textureIdentifier.IsValid())
		{
			[[maybe_unused]] const bool wasRemoved = System::Get<Rendering::Renderer>().GetTextureCache().RemoveRenderTextureListener(
				owner.GetOwningWindow()->GetLogicalDevice().GetIdentifier(),
				m_textureIdentifier,
				&owner
			);
		}

		if (Threading::EngineJobRunnerThread* pDescriptorSetLoadingThread = m_pDescriptorSetLoadingThread.Exchange(nullptr))
		{
			pDescriptorSetLoadingThread->GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_descriptorSet));

			Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
			thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(m_textureView));
		}
		m_sampler.Destroy(logicalDevice);
	}

	bool ImageDrawable::ShouldDrawCommands([[maybe_unused]] const Widget& owner, const Rendering::Pipelines& pipelines) const
	{
		Assert(owner.HasLoadedResources());
		return pipelines.GetRoundedImagePipeline().IsValid() & pipelines.GetColoredIconPipeline().IsValid() & m_textureView.IsValid() &
		       m_descriptorSet.IsValid();
	}

	void ImageDrawable::RecordDrawCommands(
		const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const Math::Rectangleui renderViewport,
		const Math::Vector2f startPositionShaderSpace,
		const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		Assert(owner.HasLoadedResources());
		const Rendering::DescriptorSetView descriptorSet = m_descriptorSet.AtomicLoad();
		if (UNLIKELY_ERROR(!descriptorSet.IsValid()))
		{
			return;
		}

		const Math::Vector2i ownerSize = owner.GetSize();
		const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
		const float aspectRatio = (float)ownerSize.x / (float)ownerSize.y;

		auto calculateRoundingRadius = [ownerSize, aspectRatio, screenProperties](const Widgets::Style::SizeAxisExpression& size)
		{
			const Math::Vector2f roundingRadius = Math::Min(
				Math::Vector2f{size.GetPoint((float)ownerSize.x, screenProperties), size.GetPoint((float)ownerSize.y, screenProperties)} /
					(Math::Vector2f)ownerSize,
				Math::Vector2f{0.5f}
			);

			if (roundingRadius.x > roundingRadius.y)
			{
				return roundingRadius.x * aspectRatio;
			}
			else
			{
				return roundingRadius.y;
			}
		};

		const float roundingRadius = calculateRoundingRadius(m_roundingRadius);

		const Math::WorldRotation2D angle = owner.GetWorldRotation(owner.GetSceneRegistry());
		if (roundingRadius > 0.f)
		{
			const Rendering::RoundedImagePipeline& roundedImagePipeline = pipelines.GetRoundedImagePipeline();
			renderCommandEncoder.BindPipeline(roundedImagePipeline);

			roundedImagePipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace,
				endPositionShaderSpace,
				angle,
				m_color,
				roundingRadius,
				aspectRatio,
				owner.GetDepthRatio(),
				descriptorSet
			);
		}
		else
		{
			const Rendering::ColoredIconPipeline& coloredIconPipeline = pipelines.GetColoredIconPipeline();
			renderCommandEncoder.BindPipeline(coloredIconPipeline);

			coloredIconPipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace,
				endPositionShaderSpace,
				angle,
				m_color,
				owner.GetDepthRatio(),
				descriptorSet
			);
		}
	}

	LoadResourcesResult ImageDrawable::TryLoadResources(Widget& owner)
	{
		Assert(owner.HasStartedLoadingResources());

		if (!m_textureIdentifier.IsValid())
		{
			return LoadResourcesResult::Status::Invalid;
		}

		return Threading::JobBatch(System::Get<Rendering::Renderer>()
		                             .GetTextureCache()
		                             .GetOrLoadRenderTexture(
																	 owner.GetOwningWindow()->GetLogicalDevice().GetIdentifier(),
																	 m_textureIdentifier,
																	 Rendering::ImageMappingType::TwoDimensional,
																	 Rendering::MipMask::FromSizeBlended((Math::Vector2ui)owner.GetSize()),
																	 Rendering::TextureLoadFlags::Default & ~Rendering::TextureLoadFlags::LoadDummy,
																	 Rendering::TextureCache::TextureLoadListenerData(
																		 owner,
																		 [](
																			 Widget& owner,
																			 Rendering::LogicalDevice& logicalDevice,
																			 const Rendering::TextureIdentifier identifier,
																			 const Rendering::RenderTexture& texture,
																			 Rendering::MipMask changedMipValues,
																			 const EnumFlags<Rendering::LoadedTextureFlags> flags
																		 ) -> EventCallbackResult
																		 {
																			 if(const Optional<Data::ImageDrawable*> pImageDrawable = owner.FindDataComponentOfType<Data::ImageDrawable>(owner.GetSceneRegistry()))
																			 {
																				 if (!pImageDrawable->m_sampler.IsValid())
																				 {
																					 pImageDrawable->m_sampler =
																						 Rendering::Sampler(logicalDevice, Rendering::AddressMode::ClampToEdge);
																				 }

																				 switch (
																					 pImageDrawable
																						 ->OnTextureLoadedAsync(owner, logicalDevice, identifier, texture, changedMipValues, flags)
																				 )
																				 {
																					 case EventCallbackResult::Keep:
																						 break;
																					 case EventCallbackResult::Remove:
																						 return EventCallbackResult::Remove;
																				 }
																			 }

																			 owner.OnLoadedResources();
																			 return EventCallbackResult::Keep;
																		 }
																	 )
																 ));
	}

	[[nodiscard]] inline Rendering::TextureIdentifier
	GetStyleTextureIdentifier(const Style::CombinedEntry& style, const Style::CombinedMatchingEntryModifiersView matchingModifiers)
	{
		if (const Optional<const Asset::Guid*> textureAssetGuid = style.Find<Asset::Guid>(Style::ValueTypeIdentifier::AssetIdentifier, matchingModifiers))
		{
			return System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(*textureAssetGuid);
		}
		return Rendering::TextureIdentifier();
	}

	void ImageDrawable::OnStyleChanged(
		Widget& owner,
		const Style::CombinedEntry& styleEntry,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Color))
		{
			m_color = styleEntry.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::Color, "#ffffff"_colorf, matchingModifiers);
			if (const Optional<const Math::Ratiof*> pOpacity = styleEntry.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
			{
				m_color.a = *pOpacity;
			}
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::RoundingRadius))
		{
			m_roundingRadius = styleEntry
			                     .GetWithDefault<Style::SizeAxisCorners>(
														 Style::ValueTypeIdentifier::RoundingRadius,
														 Style::SizeAxisCorners{0_ratio},
														 matchingModifiers
													 )
			                     .m_topLeft;
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::AssetIdentifier))
		{
			const Rendering::TextureIdentifier newTextureIdentifier = GetStyleTextureIdentifier(styleEntry, matchingModifiers);
			if (newTextureIdentifier != m_textureIdentifier)
			{
				ChangeImage(owner, newTextureIdentifier);
			}
		}
	}

	EventCallbackResult ImageDrawable::
		OnTextureLoadedAsync(Widget& owner, Rendering::LogicalDevice& logicalDevice, [[maybe_unused]] const Rendering::TextureIdentifier identifier, const Rendering::RenderTexture& texture, [[maybe_unused]] const Rendering::MipMask changedMipValues, const EnumFlags<Rendering::LoadedTextureFlags>)
	{
		Threading::UniqueLock lock(m_textureLoadMutex);

		Rendering::DescriptorSet descriptorSet;
		const Rendering::DescriptorSetLayoutView descriptorSetLayout = owner.GetOwningWindow()->GetSampledImageDescriptorLayout();
		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
		Threading::EngineJobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread.Exchange(nullptr);

		const Rendering::DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
		[[maybe_unused]] const bool allocatedDescriptorSets = descriptorPool.AllocateDescriptorSets(
			logicalDevice,
			ArrayView<const Rendering::DescriptorSetLayoutView, uint8>(descriptorSetLayout),
			ArrayView<Rendering::DescriptorSet, uint8>(descriptorSet)
		);

		Assert(allocatedDescriptorSets);
		if (LIKELY(allocatedDescriptorSets))
		{
			Rendering::ImageMapping imageMapping(
				logicalDevice,
				texture,
				Rendering::ImageMappingType::TwoDimensional,
				texture.GetFormat(),
				Rendering::ImageAspectFlags::Color,
				texture.GetAvailableMipRange(),
				Rendering::ArrayRange{0, 1}
			);
			Assert(imageMapping.IsValid());

			Array<Rendering::DescriptorSet::ImageInfo, 2> imageInfo{
				Rendering::DescriptorSet::ImageInfo{{}, imageMapping, Rendering::ImageLayout::ShaderReadOnlyOptimal},
				Rendering::DescriptorSet::ImageInfo{m_sampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
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

			Assert(descriptorSet.IsValid());
			m_descriptorSet.AtomicSwap(descriptorSet);
			m_textureView.AtomicSwap(imageMapping);

			Threading::EngineJobRunnerThread* pExpected = nullptr;
			[[maybe_unused]] const bool wasExchanged = m_pDescriptorSetLoadingThread.CompareExchangeStrong(pExpected, &thread);
			Assert(wasExchanged);

			if (pPreviousDescriptorLoadingThread != nullptr)
			{
				Threading::EngineJobRunnerThread& previousEngineThread =
					static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
				previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(descriptorSet));

				thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(imageMapping));
			}
		}
		else
		{
			if (pPreviousDescriptorLoadingThread != nullptr)
			{
				Threading::EngineJobRunnerThread& previousEngineThread =
					static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
				previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_descriptorSet));

				thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(m_textureView));
			}

			owner.InvalidateLoadedResources();
			return EventCallbackResult::Remove;
		}

		return EventCallbackResult::Keep;
	}

	void ImageDrawable::ChangeImage(Widget& owner, const Rendering::TextureIdentifier textureIdentifier)
	{
		Assert(!owner.IsDestroying());
		Assert(m_textureIdentifier != textureIdentifier);
		if (m_textureIdentifier.IsValid())
		{
			[[maybe_unused]] const bool wasRemoved = System::Get<Rendering::Renderer>().GetTextureCache().RemoveRenderTextureListener(
				owner.GetOwningWindow()->GetLogicalDevice().GetIdentifier(),
				m_textureIdentifier,
				&owner
			);
		}

		if (!owner.HasLoadedResources())
		{
			m_textureIdentifier = textureIdentifier;
			owner.InvalidateLoadedResources();
			return;
		}

		// Disable drawing of old texture to indicate new one is loading in
		Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = owner.GetOwningWindow()->GetLogicalDevice().GetIdentifier();
		if (Threading::EngineJobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread.Exchange(nullptr))
		{
			Threading::EngineJobRunnerThread& previousEngineThread =
				static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
			previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDeviceIdentifier, Move(m_descriptorSet));

			const Optional<Threading::EngineJobRunnerThread*> pThread = Threading::EngineJobRunnerThread::GetCurrent();
			if (pThread.IsValid())
			{
				pThread->GetRenderData().DestroyImageMapping(logicalDeviceIdentifier, Move(m_textureView));
			}
			else
			{
				System::Get<Threading::JobManager>().QueueCallback(
					[logicalDeviceIdentifier, textureView = Move(m_textureView)](Threading::JobRunnerThread& thread) mutable
					{
						Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
						engineThread.GetRenderData().DestroyImageMapping(logicalDeviceIdentifier, Move(textureView));
					},
					Threading::JobPriority::DeallocateResourcesMax
				);
			}
		}

		m_textureIdentifier = textureIdentifier;
		owner.InvalidateLoadedResources();
	}

	[[maybe_unused]] const bool wasImageDrawableTypeRegistered = Reflection::Registry::RegisterType<ImageDrawable>();
	[[maybe_unused]] const bool wasImageDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ImageDrawable>>::Make());
}
