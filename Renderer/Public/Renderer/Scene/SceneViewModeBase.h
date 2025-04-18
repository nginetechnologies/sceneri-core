#pragma once

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine
{
	struct Guid;
	struct SceneBase;

	namespace Input
	{
		struct Monitor;
	}

	namespace Rendering
	{
		struct SceneViewBase;
	}

	struct SceneViewModeBase
	{
		enum class Flags : uint8
		{
			EditingScene = 1 << 0
		};

		virtual ~SceneViewModeBase() = default;

		[[nodiscard]] virtual Guid GetTypeGuid() const = 0;

		virtual void OnActivated(Rendering::SceneViewBase&)
		{
		}
		virtual void OnDeactivated(Optional<SceneBase*>, Rendering::SceneViewBase&)
		{
		}
		[[nodiscard]] virtual void OnSceneAssigned(SceneBase&, Rendering::SceneViewBase&)
		{
		}
		virtual void OnSceneUnloading(SceneBase&, Rendering::SceneViewBase&)
		{
		}
		virtual void OnSceneLoaded(SceneBase&, Rendering::SceneViewBase&)
		{
		}

		[[nodiscard]] virtual Optional<Input::Monitor*> GetInputMonitor() const
		{
			return {};
		}

		[[nodiscard]] virtual EnumFlags<Flags> GetFlags() const
		{
			return {};
		}
	};

	ENUM_FLAG_OPERATORS(SceneViewModeBase::Flags);
}
