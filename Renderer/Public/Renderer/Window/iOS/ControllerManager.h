#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Optional.h>

#import <GameController/GameController.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#define USE_VIRTUAL_GAME_CONTROLLER PLATFORM_APPLE_IOS && !PLATFORM_APPLE_MACCATALYST && !PLATFORM_APPLE_VISIONOS
#else
#define USE_VIRTUAL_GAME_CONTROLLER 0
#endif

#if USE_VIRTUAL_GAME_CONTROLLER
#define USE_CUSTOM_VIRTUAL_GAME_CONTROLLER 1
#define USE_APPLE_VIRTUAL_GAME_CONTROLLER 0
#else
#define USE_CUSTOM_VIRTUAL_GAME_CONTROLLER 0
#define USE_APPLE_VIRTUAL_GAME_CONTROLLER 0
#endif

@class GestureDelegate;

namespace ngine
{
	struct Engine;

	namespace Rendering
	{
		struct Window;
	}
	namespace Input
	{
		struct ActionMonitor;
	}

	class ControllerManager
	{
	public:
		void Initialize(Engine& engine, Rendering::Window* pWindow);

#if USE_VIRTUAL_GAME_CONTROLLER
		void CreateVirtualController();
		void DestroyVirtualController();
#endif
#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
		void ResetVirtualController();

		bool IsVirtualController(GCController* pController) const;
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
		void SetGestureDelegate(GestureDelegate* pGestureDelegate);
#endif

		void AddController(GCController* pController);
		ArrayView<GCController*> GetControllers();
		void RemoveController(GCController* pController);

		void AssignMonitor(const Input::ActionMonitor& actionMonitor);
	private:
		Optional<Engine*> m_pEngine{nullptr};
		Optional<Rendering::Window*> m_pWindow{nullptr};
		Optional<const Input::ActionMonitor*> m_pActionMonitor{nullptr};
		InlineVector<GCController*, 1> m_controllers;

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
		GCVirtualController* m_pVirtualController{nullptr};
		GCController* m_pVirtualControllerHandler{nullptr};
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
		GestureDelegate* m_pGestureDelegate{nullptr};
#endif
	};
}
