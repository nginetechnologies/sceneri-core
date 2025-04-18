#pragma once

#include "ControllerManager.h"
#include "GestureDetector.h"

#include <jni.h>

struct android_app;

struct android_java_context
{
	jclass gameActivityClass;
};

namespace ngine::Platform::Android
{
	struct GameActivityWindow;

	struct Application
	{
		[[nodiscard]] static Application& GetInstance();

		void Start(android_app* pApp);

		android_java_context m_javaContext;
		ControllerManager m_controllerManager;
		GestureDetector m_gestureDetector;

		Optional<GameActivityWindow*> m_pGameActivityWindow;
	};
}
