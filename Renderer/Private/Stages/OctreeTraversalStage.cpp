#include "Stages/OctreeTraversalStage.h"

#include <Renderer/Scene/SceneView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Commands/CommandEncoderView.h>

#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/OctreeNode.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Scene/SceneOctreeNode.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/System/Query.h>

namespace ngine::Rendering
{
	[[nodiscard]] Tag::Identifier GetRenderItemTag(Tag::Registry& registry)
	{
		return registry.FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);
	}

	OctreeTraversalStage::OctreeTraversalStage(SceneView& sceneView)
		: Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::OctreeCulling)
		, m_sceneView(sceneView)
		, m_renderItemTagIdentifier(GetRenderItemTag(System::Get<Tag::Registry>()))
		, m_perFrameStagingBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				StagingBufferSize,
				StagingBuffer::Flags::TransferSource | StagingBuffer::Flags::TransferDestination
			)
	{
	}

	OctreeTraversalStage::~OctreeTraversalStage()
	{
		m_perFrameStagingBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());
	}

	bool OctreeTraversalStage::ShouldRecordCommands() const
	{
		return m_sceneView.HasActiveCamera();
	}

	void OctreeTraversalStage::RecordCommands(const CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		const DebugMarker debugMarker{graphicsCommandEncoder, m_sceneView.GetLogicalDevice(), "Octree Traversal", "#FF0000"_color};
#endif

		m_lastFrameIndex = m_sceneView.GetCurrentFrameIndex();
		m_perFrameStagingBuffer.Start();

		m_sceneView.StartOctreeTraversal(graphicsCommandEncoder, m_perFrameStagingBuffer);

		const Entity::CameraComponent& camera = m_sceneView.GetActiveCameraComponent();

		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetScene().GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::OctreeNode>& octreeNodeSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::OctreeNode>();
		Optional<Entity::Data::OctreeNode*> pCameraOctreeNode = octreeNodeSceneData.GetComponentImplementation(camera.GetIdentifier());

		const SceneOctreeNode* pNode = LIKELY(pCameraOctreeNode != nullptr) ? &pCameraOctreeNode->Get()
		                                                                    : &m_sceneView.GetScene().GetRootComponent().GetRootNode();
		const SceneOctreeNode* pNodeChild = nullptr;
		do
		{
			if (pNode->ContainsTag(m_renderItemTagIdentifier))
			{
				for (Entity::Component3D& transformedComponent : pNode->GetComponentsView())
				{
					ProcessTransformedComponentInOctree(transformedComponent, graphicsCommandEncoder, sceneRegistry);
				}

				for (const Optional<SceneOctreeNode*> pChildNode : pNode->GetChildren())
				{
					if (pChildNode != nullptr && pChildNode != pNodeChild)
					{
						if (pChildNode->ContainsTag(m_renderItemTagIdentifier
						    )) // Math::Overlaps<Math::WorldCoordinate>(cullingFrustum, childNode.GetDynamicBoundingBox(), worldToCameraSpace))
						{
							ProcessHierarchyInOctree(*pChildNode, graphicsCommandEncoder, sceneRegistry);
						}
					}
				}
			}

			pNodeChild = pNode;
			pNode = pNode->GetParent();
		} while (pNode != nullptr);

		m_sceneView.OnOctreeTraversalFinished(graphicsCommandEncoder, m_perFrameStagingBuffer);
	}

	bool OctreeTraversalStage::HasStartedCulling() const
	{
		return m_lastFrameIndex == m_sceneView.GetCurrentFrameIndex();
	}

	void OctreeTraversalStage::OnFinishedExecution(Threading::JobRunnerThread&)
	{
		m_lastFrameIndex = m_sceneView.GetCurrentFrameIndex();
	}

	void OctreeTraversalStage::ProcessTransformedComponentInOctree(

		Entity::Component3D& transformedComponent,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		Entity::SceneRegistry& sceneRegistry
	)
	{
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const EnumFlags<Entity::ComponentFlags> componentFlags =
			flagsSceneData.GetComponentImplementationUnchecked(transformedComponent.GetIdentifier());
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>();
		if (sceneRegistry.HasDataComponentOfType(transformedComponent.GetIdentifier(), renderItemIdentifierSceneData.GetIdentifier()))
		{
			m_sceneView.ProcessComponentFromOctreeTraversal(transformedComponent);
		}
		else if (componentFlags.IsSet(Entity::ComponentFlags::IsRootScene))
		{
			const Entity::RootSceneComponent& rootSceneComponent = static_cast<const Entity::RootSceneComponent&>(transformedComponent);

			const SceneOctreeNode& octreeNode = rootSceneComponent.GetRootNode();

			if (octreeNode.ContainsTag(m_renderItemTagIdentifier
			    )) // Math::Overlaps<Math::WorldCoordinate>(cullingFrustum, rootSceneComponent.GetDynamicOctreeWorldBoundingBox(),
			       // worldToCameraSpaceMatrix))
			{
				ProcessHierarchyInOctree(octreeNode, graphicsCommandEncoder, sceneRegistry);
			}
		}
	}

	void OctreeTraversalStage::ProcessHierarchyInOctree(
		const SceneOctreeNode& node, const Rendering::CommandEncoderView graphicsCommandEncoder, Entity::SceneRegistry& sceneRegistry
	)
	{
		{
			const SceneOctreeNode::ComponentsView components = node.GetComponentsView();
			for (Entity::Component3D& transformedComponent : components)
			{
				ProcessTransformedComponentInOctree(transformedComponent, graphicsCommandEncoder, sceneRegistry);
			}
		}

		for (const Optional<SceneOctreeNode*> pChildNode : node.GetChildren())
		{
			if (pChildNode != nullptr)
			{
				if (pChildNode->ContainsTag(m_renderItemTagIdentifier
				    )) // Math::Overlaps<Math::WorldCoordinate>(cullingFrustum, childNode.GetDynamicBoundingBox(), worldToCameraSpaceMatrix))
				{
					ProcessHierarchyInOctree(*pChildNode, graphicsCommandEncoder, sceneRegistry);
				}
			}
		}
	}
}
