#pragma once

#include "SceneViewBase.h"

#include <Renderer/Constants.h>
#include <Renderer/Assets/Material/RenderMaterialCache.h>
#include <Renderer/Scene/TransformBuffer.h>

#include <Common/Assert/Assert.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Primitives/Line.h>
#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Math/Primitives/ForwardDeclarations/CullingFrustum.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine
{
	namespace Entity
	{
		struct Component3D;
		struct CameraComponent;

		namespace Data
		{
			struct WorldTransform;
			struct BoundingBox;
		}
	}

	namespace Widgets::Document
	{
		struct Scene3D;
	}

	struct Scene3D;
}

namespace ngine::Rendering
{
	struct LogicalDevice;

	struct OctreeTraversalStage;
	struct LateStageVisibilityCheckStage;
	struct Stage;
	struct Framegraph;
	struct RenderItemStage;
	struct MaterialsStage;

	struct PerFrameStagingBuffer;

	struct CommandEncoderView;
	struct SceneData;

	struct SceneView final : public SceneViewBase
	{
	public:
		SceneView(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags = {});
		SceneView(
			LogicalDevice& logicalDevice,
			SceneViewDrawer& drawer,
			Widgets::Document::Scene3D& sceneDocument,
			RenderOutput& output,
			const EnumFlags<Flags> flags = {}
		);
		SceneView(const SceneView&) = delete;
		SceneView& operator=(const SceneView&) = delete;
		SceneView(SceneView&& other) = delete;
		SceneView& operator=(SceneView&&) = delete;
		~SceneView();

		[[nodiscard]] Optional<Widgets::Document::Scene3D*> GetOwningWidget()
		{
			return m_pSceneWidget;
		}

		[[nodiscard]] bool HasActiveCamera() const
		{
			return m_pActiveCameraComponent != nullptr;
		}

		[[nodiscard]] Entity::CameraComponent& GetActiveCameraComponent() const;
		[[nodiscard]] Optional<Entity::CameraComponent*> GetActiveCameraComponentSafe() const;
		[[nodiscard]] PURE_STATICS Math::WorldCoordinate GetWorldLocation() const;

		[[nodiscard]] PURE_STATICS uint8 GetCurrentFrameIndex() const;

		void AssignScene(Scene3D& scene);
		void DetachCurrentScene();
		void AssignCamera(Entity::CameraComponent& newCamera);
		[[nodiscard]] PURE_STATICS Optional<Scene3D*> GetSceneChecked() const;
		[[nodiscard]] PURE_STATICS Scene3D& GetScene() const;
		[[nodiscard]] PURE_STATICS Optional<SceneData*> GetSceneData() const;

		[[nodiscard]] PURE_STATICS Math::Vector2ui GetRenderResolution() const;

		[[nodiscard]] PURE_STATICS Optional<Math::Vector2f> WorldToViewRatio(const Math::WorldCoordinate coordinate) const
		{
			const ViewMatrices& __restrict viewMatrices = m_viewMatrices;

			Math::Vector4f transformedLocation = Math::Vector4f{coordinate.x, coordinate.y, coordinate.z, 1.f} *
			                                     viewMatrices.GetMatrix(ViewMatrices::Type::ViewProjection);
			if (transformedLocation.w <= 0.f)
			{
				return Invalid;
			}
			transformedLocation *= Math::MultiplicativeInverse(transformedLocation.w);

			return Math::Vector2f{(1.f + transformedLocation.x) * 0.5f, (1.f + transformedLocation.y) * 0.5f};
		}

		[[nodiscard]] PURE_STATICS Optional<Math::Vector2f> WorldToView(const Math::WorldCoordinate coordinate) const
		{
			const ViewMatrices& __restrict viewMatrices = m_viewMatrices;

			Math::Vector4f transformedLocation = Math::Vector4f{coordinate.x, coordinate.y, coordinate.z, 1.f} *
			                                     viewMatrices.GetMatrix(ViewMatrices::Type::ViewProjection);
			if (transformedLocation.w == 0.f)
			{
				return Invalid;
			}
			transformedLocation *= Math::MultiplicativeInverse(transformedLocation.w);
			return (Math::Vector2f{1.f} + Math::Vector2f{transformedLocation.x, transformedLocation.y}) * (Math::Vector2f)GetRenderResolution() *
			       0.5f;
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate ViewToWorld(const Math::Vector2f viewCoordinatesRatio, float depthRatio) const
		{
			const ViewMatrices& __restrict viewMatrices = m_viewMatrices;

			// Inverse depth ratio since we have inverse depth for our projection matrix active
			depthRatio = 1.f - depthRatio;

			const Math::Vector4f location = Math::Vector4f{viewCoordinatesRatio.x, viewCoordinatesRatio.y, depthRatio, 1.f} *
			                                viewMatrices.GetMatrix(ViewMatrices::Type::InvertedViewProjection);
			return Math::WorldCoordinate{location.x, location.y, location.z} * Math::MultiplicativeInverse(location.w);
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate ViewToWorld(const Math::Vector2ui viewCoordinates, const float depthRatio) const
		{
			const Math::Vector2f coordinateRatio = Math::Vector2f((Math::Vector2ui)viewCoordinates) / Math::Vector2f(GetRenderResolution());
			return ViewToWorld(coordinateRatio * 2.f - Math::Vector2f{1.f, 1.f}, depthRatio);
		}

		[[nodiscard]] PURE_STATICS Math::Vector2f GetCoordinateRatio(const Math::Vector2ui viewCoordinates) const
		{
			return Math::Vector2f((Math::Vector2ui)viewCoordinates) / Math::Vector2f(GetRenderResolution());
		}

		[[nodiscard]] PURE_STATICS Math::WorldLine
		ViewToWorldLine(const Math::Vector2ui viewCoordinates, const Math::WorldCoordinateUnitType length) const
		{
			const Math::WorldLine line = ViewToWorldLine(
				(Math::Vector2f((Math::Vector2ui)viewCoordinates) / Math::Vector2f(GetRenderResolution())) * 2.f - Math::Vector2f{1.f, 1.f}
			);
			return Math::WorldLine{line.GetStart(), line.GetStart() + line.GetDirection() * length};
		}

		[[nodiscard]] PURE_STATICS Math::WorldLine ViewToWorldLine(const Math::Vector2ui viewCoordinates) const
		{
			return ViewToWorldLine(
				(Math::Vector2f((Math::Vector2ui)viewCoordinates) / Math::Vector2f(GetRenderResolution())) * 2.f - Math::Vector2f{1.f, 1.f}
			);
		}

		[[nodiscard]] PURE_STATICS Math::WorldLine ViewToWorldLine(const Math::Vector2f viewCoordinatesRatio) const
		{
			const Math::WorldCoordinate nearPlaneLocation = ViewToWorld(viewCoordinatesRatio, 0.f);
			const Math::WorldCoordinate farPlaneLocation = ViewToWorld(viewCoordinatesRatio, 1.f);

			return Math::WorldLine{nearPlaneLocation, farPlaneLocation};
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate ViewToWorldConstrainedToAxis(
			const Math::WorldCoordinate nearPlaneLocation, const Math::WorldCoordinate axisOrigin, const Math::Vector3f axis
		) const
		{
			const Math::WorldCoordinate cameraLocation = GetWorldLocation();

			// find plane between camera and initial position and direction
			const Math::Vector3f cameraToOrigin = cameraLocation - axisOrigin;
			const Math::Vector3f lineViewPlane = cameraToOrigin.Cross(axis).GetNormalized();

			// Now we project the ray from origin to the source point to the screen space line plane
			const Math::Vector3f cameraToSrc = (nearPlaneLocation - cameraLocation).GetNormalized();

			const Math::Vector3f perpPlane = (cameraToSrc.Cross(lineViewPlane));

			// finally, project along the axis to perpPlane
			return axisOrigin + (perpPlane.Dot(cameraToOrigin) / perpPlane.Dot(axis)) * axis;
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate ViewToWorldConstrainedToAxis(
			const Math::Vector2ui viewCoordinates, const Math::WorldCoordinate axisOrigin, const Math::Vector3f axis
		) const
		{
			const Math::WorldCoordinate nearPlaneLocation = ViewToWorld(viewCoordinates, 0.f);
			return ViewToWorldConstrainedToAxis(nearPlaneLocation, axisOrigin, axis);
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate
		ViewToWorldConstrainedToPlane(const Math::WorldLine line, const Math::WorldCoordinate axisOrigin, const Math::Vector3f normal) const
		{
			const float fac = (axisOrigin - line.GetStart()).Dot(normal) / line.GetDirection().Dot(normal);
			Math::WorldCoordinate result = line.GetStart() + line.GetDirection() * fac;

			result -= normal * normal.Dot(result);
			result += normal * axisOrigin.Dot(normal);

			return result;
		}

		[[nodiscard]] PURE_STATICS Math::WorldCoordinate ViewToWorldConstrainedToPlane(
			const Math::Vector2ui viewCoordinates, const Math::WorldCoordinate axisOrigin, const Math::Vector3f normal
		) const
		{
			const Math::WorldLine line = ViewToWorldLine(viewCoordinates, 1.f);
			return ViewToWorldConstrainedToPlane(line, axisOrigin, normal);
		}

		[[nodiscard]] Math::CullingFrustum<float> GetCullingFrustum() const;

		void OnBeforeResizeRenderOutput();
		void OnAfterResizeRenderOutput();

		void OnRenderItemAdded(Entity::HierarchyComponentBase& renderItem);
		void OnRenderItemRemoved(const Entity::RenderItemIdentifier renderItemIdentifier);
		void OnRenderItemEnabled(Entity::HierarchyComponentBase& renderItem);
		void OnRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier);
		void OnRenderItemTransformChanged(Entity::HierarchyComponentBase& renderItem);
		void OnRenderItemStageMaskEnabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const;
		void OnRenderItemStageMaskDisabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const;
		void OnRenderItemStageMaskChanged(
			const Entity::RenderItemIdentifier renderItemIdentifier,
			const RenderItemStageMask& enabledStages,
			const RenderItemStageMask& disabledStages
		) const;
		void OnRenderItemStageMaskReset(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages);
		void OnRenderItemStageEnabled(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		) const;
		void OnRenderItemStageDisabled(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		) const;
		void OnRenderItemStageReset(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		) const;

		[[nodiscard]] PURE_STATICS bool HasStartedCulling() const;
		[[nodiscard]] PURE_STATICS Framegraph& GetFramegraph() const;
		[[nodiscard]] PURE_STATICS Stage& GetOctreeTraversalStage() const;
		[[nodiscard]] PURE_STATICS Stage& GetLateStageVisibilityCheckStage() const;

		void ProcessEnabledRenderItem(Entity::HierarchyComponentBase& component);
		void ProcessLateStageAddedRenderItem(Entity::HierarchyComponentBase& component);
		void ProcessLateStageChangedRenderItemTransform(Entity::HierarchyComponentBase& component);

		void StartOctreeTraversal(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer);
		void StartLateStageOctreeVisibilityCheck();
		using TraversalResult = SceneViewBase::TraversalResult;
		TraversalResult ProcessComponentFromOctreeTraversal(Entity::HierarchyComponentBase& component);
		void NotifyOctreeTraversalRenderStages(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		);

		void
		OnOctreeTraversalFinished(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer);

		[[nodiscard]] DescriptorSetView GetTransformBufferDescriptorSet() const
		{
			return m_transformBuffer.GetTransformDescriptorSet();
		}
		[[nodiscard]] DescriptorSetLayoutView GetTransformBufferDescriptorSetLayout() const
		{
			return m_transformBuffer.GetTransformDescriptorSetLayout();
		}

		[[nodiscard]] MaterialsStage& GetMaterialsStage()
		{
			return *m_pMaterialsStage;
		}

		[[nodiscard]] RenderMaterialCache& GetRenderMaterialCache()
		{
			return m_renderMaterialCache;
		}

		void RecalculateViewMatrices()
		{
			OnActiveCameraPropertiesChanged();
		}
	protected:
		friend Widgets::Document::Scene3D;
		friend Entity::CameraComponent;

		virtual void OnEnableFramegraph(SceneBase& scene, const Optional<Entity::HierarchyComponentBase*> pCamera) override;
		virtual void OnDisableFramegraph(SceneBase& scene, const Optional<Entity::HierarchyComponentBase*> pCamera) override;

		void OnActiveCameraPropertiesChanged();
		void OnActiveCameraTransformChanged();
		void OnActiveCameraDestroyed();

		void OnCameraAssignedInternal(Entity::CameraComponent& newCamera, const Optional<Entity::CameraComponent*> pPreviousCamera);
	private:
		Optional<Widgets::Document::Scene3D*> m_pSceneWidget = Invalid;

		RenderMaterialCache m_renderMaterialCache;
		UniquePtr<MaterialsStage> m_pMaterialsStage;

		UniqueRef<OctreeTraversalStage> m_pOctreeTraversalStage;
		UniqueRef<LateStageVisibilityCheckStage> m_pLateStageVisibilityCheckStage;
	public:
		TransformBuffer m_transformBuffer;
	};
}
