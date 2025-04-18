#pragma once

#include "SceneViewBase.h"

#include <Common/Math/ForwardDeclarations/Rotation2D.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>

namespace ngine
{
	struct Scene2D;
}

namespace ngine::Rendering
{
	struct SceneView2D final : public SceneViewBase
	{
		using BaseType = SceneViewBase;
		SceneView2D(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags = {});
		virtual ~SceneView2D();

		[[nodiscard]] PURE_STATICS Optional<Scene2D*> GetScene() const;

		void AssignScene(Scene2D& scene);
		void DetachCurrentScene();

		void StartQuadtreeTraversal(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer);

		void OnCameraPropertiesChanged(const Math::Rectanglef viewArea, const Math::WorldRotation2D rotation);
		void OnCameraTransformChanged(const Math::Vector2f coordinate, const Math::WorldRotation2D rotation);
	};
}
