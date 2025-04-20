#include "Stages/WidgetDrawingStage.h"

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Scene/SceneView2D.h>

#include <Widgets/Widget.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/Data/TextDrawable.h>
#include <Widgets/Data/ImageDrawable.h>
#include <Widgets/Primitives/CircleDrawable.h>
#include <Widgets/Primitives/GridDrawable.h>
#include <Widgets/Primitives/LineDrawable.h>
#include <Widgets/Primitives/RectangleDrawable.h>
#include <Widgets/Primitives/RoundedRectangleDrawable.h>

#include <Common/System/Query.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/QuadtreeNode.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Widgets
{
	[[nodiscard]] Tag::Identifier GetRenderItemTag(Tag::Registry& registry)
	{
		return registry.FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);
	}

	QuadtreeTraversalStage::QuadtreeTraversalStage(Rendering::LogicalDevice& logicalDevice, Rendering::SceneView2D& sceneView)
		: Stage(logicalDevice, Threading::JobPriority::OctreeCulling)
		, m_sceneView(sceneView)
		, m_perFrameStagingBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				StagingBufferSize,
				Rendering::StagingBuffer::Flags::TransferSource | Rendering::StagingBuffer::Flags::TransferDestination
			)
		, m_renderItemTagIdentifier(GetRenderItemTag(System::Get<Tag::Registry>()))
	{
	}

	QuadtreeTraversalStage::~QuadtreeTraversalStage()
	{
		m_perFrameStagingBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());
	}

	bool QuadtreeTraversalStage::ShouldRecordCommands() const
	{
		return true;
	}

	void QuadtreeTraversalStage::RecordCommands(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
		m_perFrameStagingBuffer.Start();

		Rendering::SceneView2D& sceneView = m_sceneView;
		sceneView.StartQuadtreeTraversal(graphicsCommandEncoder, m_perFrameStagingBuffer);

		const Optional<Scene2D*> pScene = sceneView.GetScene();
		if (LIKELY(pScene.IsValid()))
		{
			Entity::SceneRegistry& sceneRegistry = pScene->GetEntitySceneRegistry();
			const Math::Rectanglef area{sceneView.GetMatrices().GetLocation2D(), (Math::Vector2f)sceneView.GetMatrices().GetRenderResolution()};

			SceneQuadtreeNode* pNode = &pScene->GetRootComponent().GetQuadTree();
			SceneQuadtreeNode* pNodeChild = nullptr;
			do
			{
				if (pNode->ContainsTag(m_renderItemTagIdentifier))
				{
					SceneQuadtreeNode::ElementView elements = pNode->GetElementView();
					for (auto& element : elements)
					{
						Assert(pNode->ContainsTag(m_renderItemTagIdentifier));
						ProcessTransformedComponent(sceneRegistry, element.m_element, area, graphicsCommandEncoder);
					}

					pNode->IterateChildren(
						[this, &sceneRegistry, area, graphicsCommandEncoder, pNodeChild](SceneQuadtreeNode::BaseType& childNode)
						{
							if (&childNode != pNodeChild)
							{
								if (static_cast<SceneQuadtreeNode&>(childNode).ContainsTag(m_renderItemTagIdentifier) && childNode.GetChildContentArea().Overlaps(area))
								{
									ProcessHierarchy(sceneRegistry, static_cast<SceneQuadtreeNode&>(childNode), area, graphicsCommandEncoder);
								}
							}
						}
					);
				}

				pNodeChild = pNode;
				pNode = static_cast<SceneQuadtreeNode*>(pNode->GetParent());
			} while (pNode != nullptr);

			sceneView.OnTraversalFinished(*pScene);

			sceneView.NotifyRenderStages(*pScene, graphicsCommandEncoder, m_perFrameStagingBuffer);
		}
	}

	void QuadtreeTraversalStage::ProcessTransformedComponent(
		Entity::SceneRegistry& sceneRegistry,
		Entity::Component2D& transformedComponent,
		const Math::Rectanglef area,
		const Rendering::CommandEncoderView
	)
	{
		const Optional<Widget*> pWidget = transformedComponent.As<Widget>(sceneRegistry);
		if (UNLIKELY_ERROR(pWidget.IsInvalid()))
		{
			return;
		}

		Widget& __restrict widget = *pWidget;
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const Optional<Entity::Data::Flags*> pComponentFlags = componentFlagsSceneData.GetComponentImplementation(widget.GetIdentifier());
		const EnumFlags<Entity::ComponentFlags> componentFlags = pComponentFlags != nullptr
		                                                           ? (EnumFlags<Entity::ComponentFlags>)*pComponentFlags
		                                                           : EnumFlags<Entity::ComponentFlags>{
																																	 Entity::ComponentFlags::IsDisabled | Entity::ComponentFlags::IsDestroying
																																 };

		if (componentFlags.IsNotSet(Entity::ComponentFlags::IsDestroying) && widget.HasDataComponentOfType(sceneRegistry, sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>().GetIdentifier()))
		{
			Math::Rectanglei maskedArea = widget.GetMaskedContentArea(sceneRegistry, widget.GetOwningWindow(), (Math::Rectanglei)area);
			if (maskedArea.GetPosition().x < 0)
			{
				maskedArea.m_size.x += maskedArea.m_position.x;
				maskedArea.m_position.x = 0;
			}
			if (maskedArea.GetPosition().y < 0)
			{
				maskedArea.m_size.y += maskedArea.m_position.y;
				maskedArea.m_position.y = 0;
			}

			const bool isVisible = [](Widget& __restrict widget, const Math::Rectanglei maskedArea)
			{
				const EnumFlags<Widget::Flags> flags = widget.GetFlags() & (Widget::Flags::IsIgnoredFromStyle);
				return flags.AreNoneSet(Widget::Flags::IsIgnoredFromAnySource) && maskedArea.HasSize();
			}(widget, maskedArea);

			m_sceneView.ProcessComponent(sceneRegistry, transformedComponent.GetIdentifier(), transformedComponent, isVisible);
		}
	}

	void QuadtreeTraversalStage::ProcessHierarchy(
		Entity::SceneRegistry& sceneRegistry,
		SceneQuadtreeNode& node,
		const Math::Rectanglef area,
		const Rendering::CommandEncoderView graphicsCommandEncoder
	)
	{
		{
			SceneQuadtreeNode::ElementView elements = node.GetElementView();
			for (auto& element : elements)
			{
				Assert(node.ContainsTag(m_renderItemTagIdentifier));
				ProcessTransformedComponent(sceneRegistry, element.m_element, area, graphicsCommandEncoder);
			}
		}

		node.IterateChildren(
			[this, &sceneRegistry, area, graphicsCommandEncoder](SceneQuadtreeNode::BaseType& childNode)
			{
				if (static_cast<SceneQuadtreeNode&>(childNode).ContainsTag(m_renderItemTagIdentifier) && childNode.GetChildContentArea().Overlaps(area))
				{
					// Assert(childNode.GetContentArea().Overlaps(childNode.GetChildContentArea()));
					Assert(
						childNode.GetParent() == nullptr || childNode.GetParent()->GetChildContentArea().Overlaps(childNode.GetChildContentArea())
					);
					ProcessHierarchy(sceneRegistry, static_cast<SceneQuadtreeNode&>(childNode), area, graphicsCommandEncoder);
				}
			}
		);
	}

	WidgetDrawingStage::WidgetDrawingStage(
		Rendering::LogicalDevice& logicalDevice, Rendering::SceneView2D& sceneView, Rendering::ToolWindow& toolWindow
	)
		: Stage(logicalDevice, Threading::JobPriority::WidgetDrawing)
		, m_sceneView(sceneView)
		, m_drawingPipelines(logicalDevice, Array{toolWindow.GetSampledImageDescriptorLayout()})
	{
	}

	void WidgetDrawingStage::OnBeforeRenderPassDestroyed()
	{
		m_drawingPipelines.PrepareForResize(m_logicalDevice);
	}

	Threading::JobBatch WidgetDrawingStage::AssignRenderPass(
		const Rendering::RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui fullRenderArea,
		const uint8 subpassIndex
	)
	{
		return m_drawingPipelines
		  .Create(m_logicalDevice, m_logicalDevice.GetShaderCache(), renderPass, outputArea, fullRenderArea, subpassIndex);
	}

	WidgetDrawingStage::~WidgetDrawingStage()
	{
		m_drawingPipelines.Destroy(m_logicalDevice);
	}

	bool WidgetDrawingStage::ShouldRecordCommands() const
	{
		return m_drawingPipelines.IsValid() && m_sceneView.GetVisibleRenderItems().AreAnySet();
	}

	void WidgetDrawingStage::OnBeforeRecordCommands(const Rendering::CommandEncoderView)
	{
		if (WasSkipped())
		{
			return;
		}

		Rendering::SceneView2D& sceneView = m_sceneView;
		Scene2D& scene = *sceneView.GetScene();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		const Math::Rectanglef area{sceneView.GetMatrices().GetLocation2D(), (Math::Vector2f)sceneView.GetMatrices().GetRenderResolution()};

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();

		for (Entity::RenderItemIdentifier::IndexType renderItemIndex :
		     sceneView.GetVisibleRenderItems().GetSetBitsIterator(0, scene.GetMaximumUsedRenderItemCount()))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Optional<Entity::HierarchyComponentBase*> pComponent = sceneView.GetVisibleRenderItemComponent(renderItemIdentifier);
			if (UNLIKELY(!pComponent.IsValid()))
			{
				continue;
			}
			if (UNLIKELY(pComponent->IsDestroying(sceneRegistry)))
			{
				continue;
			}

			const Optional<Widget*> pWidget = pComponent->As<Widget>(sceneRegistry);
			if (UNLIKELY(pWidget.IsInvalid()))
			{
				continue;
			}
			Widget& __restrict widget = *pWidget;
			if (!widget.HasLoadedResources())
			{
				if (!widget.LoadResources(sceneRegistry))
				{
					// Try loading resources again another time
					continue;
				}
			}
			else if (!widget.AreResourcesUpToDate())
			{
				// Lazily request new resources
				widget.LoadResources(sceneRegistry);
			}

			Math::Rectanglei maskedArea = widget.GetMaskedContentArea(sceneRegistry, widget.GetOwningWindow(), (Math::Rectanglei)area);
			if (maskedArea.GetPosition().x < 0)
			{
				maskedArea.m_size.x += maskedArea.m_position.x;
				maskedArea.m_position.x = 0;
			}
			if (maskedArea.GetPosition().y < 0)
			{
				maskedArea.m_size.y += maskedArea.m_position.y;
				maskedArea.m_position.y = 0;
			}

			// TODO: Unify widget drawing with render item stages
			// Then simplify so we can use Component2D directly instead

			DrawableType drawableType;
			if (const Optional<Data::TextDrawable*> pTextDrawable = widget.FindDataComponentOfType(textDrawableSceneData))
			{
				drawableType = DrawableType::Text;
				if (!pTextDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::ImageDrawable*> pImageDrawable = widget.FindDataComponentOfType(imageDrawableSceneData))
			{
				drawableType = DrawableType::Image;
				if (!pImageDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::Primitives::CircleDrawable*> pCircleDrawable = widget.FindDataComponentOfType(circleDrawableSceneData))
			{
				drawableType = DrawableType::Circle;
				if (!pCircleDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::Primitives::GridDrawable*> pGridDrawable = widget.FindDataComponentOfType(gridDrawableSceneData))
			{
				drawableType = DrawableType::Grid;
				if (!pGridDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::Primitives::LineDrawable*> pLineDrawable = widget.FindDataComponentOfType(lineDrawableSceneData))
			{
				drawableType = DrawableType::Line;
				if (!pLineDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::Primitives::RectangleDrawable*> pRectangleDrawable = widget.FindDataComponentOfType(rectangleDrawableSceneData))
			{
				drawableType = DrawableType::Rectangle;
				if (!pRectangleDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else if (const Optional<Data::Primitives::RoundedRectangleDrawable*> pRoundedRectangleDrawable = widget.FindDataComponentOfType(roundedRectangleDrawableSceneData))
			{
				drawableType = DrawableType::RoundedRectangle;
				if (!pRoundedRectangleDrawable->ShouldDrawCommands(widget, m_drawingPipelines))
				{
					continue;
				}
			}
			else
			{
				continue;
			}

			const float depth = widget.GetDepthRatio();
			QueuedDraw* pIt = std::lower_bound(
				m_queuedDraws.begin().Get(),
				m_queuedDraws.end().Get(),
				depth,
				[](const QueuedDraw& existingElement, const float newDepth) -> bool
				{
					return newDepth > existingElement.depth;
				}
			);
			m_queuedDraws.Emplace(pIt, Memory::Uninitialized, QueuedDraw{widget, drawableType, depth, maskedArea});
		}
	}

	uint32 WidgetDrawingStage::GetMaximumPushConstantInstanceCount() const
	{
		Rendering::SceneView2D& sceneView = m_sceneView;
		Scene2D& scene = *sceneView.GetScene();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();

		uint32 count{0};
		for (const QueuedDraw& __restrict queuedDraw : m_queuedDraws)
		{
			switch (queuedDraw.drawableType)
			{
				case DrawableType::Circle:
				{
					if (const Optional<Data::Primitives::CircleDrawable*> pCircleDrawable = queuedDraw.widget->FindDataComponentOfType(circleDrawableSceneData))
					{
						count += pCircleDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::Grid:
				{
					if (const Optional<Data::Primitives::GridDrawable*> pGridDrawable = queuedDraw.widget->FindDataComponentOfType(gridDrawableSceneData))
					{
						count += pGridDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::Line:
				{
					if (const Optional<Data::Primitives::LineDrawable*> pLineDrawable = queuedDraw.widget->FindDataComponentOfType(lineDrawableSceneData))
					{
						count += pLineDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::Rectangle:
				{
					if (const Optional<Data::Primitives::RectangleDrawable*> pRectangleDrawable = queuedDraw.widget->FindDataComponentOfType(rectangleDrawableSceneData))
					{
						count += pRectangleDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::RoundedRectangle:
				{
					if (const Optional<Data::Primitives::RoundedRectangleDrawable*> pRoundedRectangleDrawable = queuedDraw.widget->FindDataComponentOfType(roundedRectangleDrawableSceneData))
					{
						count += pRoundedRectangleDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::Image:
				{
					if (const Optional<Data::ImageDrawable*> pImageDrawable = queuedDraw.widget->FindDataComponentOfType(imageDrawableSceneData))
					{
						count += pImageDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
				case DrawableType::Text:
				{
					if (const Optional<Data::TextDrawable*> pTextDrawable = queuedDraw.widget->FindDataComponentOfType(textDrawableSceneData))
					{
						count += pTextDrawable->GetMaximumPushConstantInstanceCount(queuedDraw.widget);
					}
				}
				break;
			}
		}
		return count;
	}

	void WidgetDrawingStage::RecordRenderPassCommands(
		Rendering::RenderCommandEncoder& renderCommandEncoder,
		const Rendering::ViewMatrices&,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		Rendering::SceneView2D& sceneView = m_sceneView;
		Scene2D& scene = *sceneView.GetScene();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();

		for (const QueuedDraw& __restrict queuedDraw : m_queuedDraws)
		{
			const Math::Rectanglei widgetContentArea = queuedDraw.widget->GetContentArea(sceneRegistry);
			Math::Rectangleui drawnArea = (Math::Rectangleui)queuedDraw.area;
			drawnArea = drawnArea.Mask(renderArea);

			renderCommandEncoder.SetViewport(drawnArea);

			const Math::Vector2i relativeStartPosition = widgetContentArea.GetPosition() - (Math::Vector2i)drawnArea.GetPosition();

			const Math::Vector2f startPositionShaderSpace = Math::Vector2f{-1.f, -1.f} +
			                                                ((Math::Vector2f)relativeStartPosition / (Math::Vector2f)drawnArea.GetSize()) * 2.f;
			const Math::Vector2f endPositionShaderSpace = startPositionShaderSpace +
			                                              ((Math::Vector2f)widgetContentArea.GetSize() / (Math::Vector2f)drawnArea.GetSize()) *
			                                                2.f;

			switch (queuedDraw.drawableType)
			{
				case DrawableType::Circle:
				{
					if (const Optional<Data::Primitives::CircleDrawable*> pCircleDrawable = queuedDraw.widget->FindDataComponentOfType(circleDrawableSceneData))
					{
						pCircleDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::Grid:
				{
					if (const Optional<Data::Primitives::GridDrawable*> pGridDrawable = queuedDraw.widget->FindDataComponentOfType(gridDrawableSceneData))
					{
						pGridDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::Line:
				{
					if (const Optional<Data::Primitives::LineDrawable*> pLineDrawable = queuedDraw.widget->FindDataComponentOfType(lineDrawableSceneData))
					{
						pLineDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::Rectangle:
				{
					if (const Optional<Data::Primitives::RectangleDrawable*> pRectangleDrawable = queuedDraw.widget->FindDataComponentOfType(rectangleDrawableSceneData))
					{
						pRectangleDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::RoundedRectangle:
				{
					if (const Optional<Data::Primitives::RoundedRectangleDrawable*> pRoundedRectangleDrawable = queuedDraw.widget->FindDataComponentOfType(roundedRectangleDrawableSceneData))
					{
						pRoundedRectangleDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::Image:
				{
					if (const Optional<Data::ImageDrawable*> pImageDrawable = queuedDraw.widget->FindDataComponentOfType(imageDrawableSceneData))
					{
						pImageDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
				case DrawableType::Text:
				{
					if (const Optional<Data::TextDrawable*> pTextDrawable = queuedDraw.widget->FindDataComponentOfType(textDrawableSceneData))
					{
						pTextDrawable->RecordDrawCommands(
							queuedDraw.widget,
							renderCommandEncoder,
							(Math::Rectangleui)drawnArea,
							startPositionShaderSpace,
							endPositionShaderSpace,
							m_drawingPipelines
						);
					}
				}
				break;
			}
		}
	}

	void WidgetDrawingStage::OnAfterRecordCommands(const Rendering::CommandEncoderView)
	{
		m_queuedDraws.Clear();
	}
}
