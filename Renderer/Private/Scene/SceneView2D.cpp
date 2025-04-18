#include "Scene/SceneView2D.h"

#include <Engine/Engine.h>
#include <Engine/Scene/Scene2D.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Scene/SceneViewDrawer.h>

#include <Common/System/Query.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	SceneView2D::SceneView2D(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags)
		: BaseType(logicalDevice, drawer, output, flags)
	{
		OnCreated();
	}

	SceneView2D::~SceneView2D() = default;

	PURE_STATICS Optional<Scene2D*> SceneView2D::GetScene() const
	{
		return static_cast<Scene2D*>(m_pCurrentScene.Get());
	}

	void SceneView2D::AssignScene(Scene2D& scene)
	{
		OnSceneAssigned(scene);
	}

	void SceneView2D::DetachCurrentScene()
	{
		OnSceneDetached();
	}

	void SceneView2D::StartQuadtreeTraversal(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		if (const Optional<Scene2D*> pScene = GetScene())
		{
			SceneViewBase::StartTraversal();
			m_viewMatrices.StartFrame(
				System::Get<Engine>().GetCurrentFrameIndex(),
				pScene->GetFrameCounter(),
				pScene->GetTime().GetSeconds(),
				m_logicalDevice,
				m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
				graphicsCommandEncoder,
				perFrameStagingBuffer
			);
		}
	}

	void SceneView2D::OnCameraPropertiesChanged(const Math::Rectanglef viewArea, const Math::WorldRotation2D rotation)
	{
		// TODO: Implement view size here and update 2D shaders

		m_viewMatrices.OnCameraPropertiesChanged(
			Math::Matrix4x4f{Math::Identity},                                        // view matrix, currently not applicable to 2D
			Math::Matrix4x4f{Math::Identity},                                        // projection matrix, currently not applicable to 2D
			Math::Vector3f{viewArea.GetPosition().x, viewArea.GetPosition().y, 0.f}, // view location (todo: include depth?)
			Math::Matrix3x3f(Math::CreateRotationAroundXAxis, rotation),             // view rotation
			m_drawer.GetRenderResolution(),
			m_output.GetOutputArea().GetSize(),
			System::Get<Engine>().GetCurrentFrameIndex()
		);
	}

	void SceneView2D::OnCameraTransformChanged(const Math::Vector2f coordinate, const Math::WorldRotation2D rotation)
	{
		m_viewMatrices.OnCameraTransformChanged(
			Math::Matrix4x4f{Math::Identity},                            // view matrix, currently not applicable to 2D
			Math::Vector3f{coordinate.x, coordinate.y, 0.f},             // view location (todo: include depth?)
			Math::Matrix3x3f(Math::CreateRotationAroundXAxis, rotation), // view rotation
			System::Get<Engine>().GetCurrentFrameIndex()
		);
	}
}
