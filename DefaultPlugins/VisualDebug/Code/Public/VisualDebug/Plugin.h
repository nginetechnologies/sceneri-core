#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Math/ForwardDeclarations/Quaternion.h>
#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Length.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/Transform.h>
#include <Common/Time/ForwardDeclarations/Duration.h>

#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>

namespace ngine
{
	struct Scene3D;
}

namespace ngine::VisualDebug
{
	enum class Color : uint8
	{
		Red,
		Blue,
		Green,
		Purple,
		Orange,
		Count
	};

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "B0ABCB76-705B-4244-9474-F7B29131FC70"_guid;

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		void AddArrow(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Lengthf shaftHeight = 0.75_meters,
			const Math::Radiusf shaftRadius = 0.05_meters,
			const Math::Radiusf tipRadius = 0.15_meters,
			const Math::Lengthf tipHeight = 0.25_meters,
			const uint16 sideCount = 8u
		);
		void AddBox(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Vector3f dimensions = Math::Vector3f{1.f, 1.f, 1.f}
		);
		void AddCapsule(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters,
			const Math::Lengthf height = 1_meters,
			const uint16 sideCount = 8u,
			const uint16 segmentCount = 2u
		);
		void AddCone(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters,
			const Math::Lengthf height = 1_meters,
			const uint16 sideCount = 8u
		);
		void AddPyramid(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters,
			const Math::Lengthf height = 1_meters
		);
		void AddCylinder(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters,
			const Math::Lengthf height = 1_meters,
			const uint16 sideCount = 8u,
			const uint16 segmentCount = 2u
		);
		void AddArc(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Anglef angle = 90_degrees,
			const Math::Lengthf halfHeight = 0.5_meters,
			const Math::Radiusf outerRadius = 1.0_meters,
			const Math::Radiusf innerRadius = 0.5_meters,
			const uint16 sideCount = 16u
		);
		void AddPlane(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters
		);
		void AddLine(
			Scene3D& scene,
			const Math::WorldLine line,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.02_meters
		);
		void AddDirection(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::Vector3f direction,
			const Time::Durationf duration,
			const Color color = Color::Red
		);
		void AddSphere(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters
		);
		void AddTorus(
			Scene3D& scene,
			const Math::WorldCoordinate coordinate,
			const Math::WorldQuaternion rotation,
			const Time::Durationf duration,
			const Color color = Color::Red,
			const Math::Radiusf radius = 0.5_meters,
			const Math::Lengthf thickness = 0.2_meters,
			const uint16 sideCount = 8u
		);
		void AddText3D(
			Scene3D& scene,
			const Math::WorldTransform transform,
			const ConstUnicodeStringView text,
			const Math::Color color,
			const Time::Durationf duration
		);

		// todo: support for scoped drawing
		// VisualDebug::Scope drawingScope = visualDebug.CreateScope();
		// drawingScope.AddSphere();
		// When the scope exits, all items are remoevd
		// Also needs a Clear function
	};
}
