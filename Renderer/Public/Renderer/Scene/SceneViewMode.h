#pragma once

#include "SceneViewModeBase.h"
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Length.h>

namespace ngine
{
	struct Scene3D;

	namespace Input
	{
		struct Monitor;
	}

	namespace Rendering
	{
		struct SceneView;
	}

	namespace Entity
	{
		struct CameraComponent;
	}

	struct SceneViewMode : public SceneViewModeBase
	{
		virtual ~SceneViewMode() = default;

		[[nodiscard]] virtual Math::Matrix4x4f CreateProjectionMatrix(
			const Math::Anglef fieldOfView, const Math::Vector2f renderResolution, const Math::Lengthf nearPlane, const Math::Lengthf farPlane
		) const
		{
			return Math::Matrix4x4f::CreatePerspectiveInversedDepth(
				fieldOfView,
				renderResolution.x / renderResolution.y,
				nearPlane.GetUnits(),
				farPlane.GetUnits()
			);
		}
	};
}
