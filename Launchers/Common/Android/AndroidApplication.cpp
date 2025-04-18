#if PLATFORM_ANDROID
#include "AndroidApplication.h"
#include "GameActivityWindow.h"

#include <Engine/Engine.h>
#include <Engine/EngineSystems.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/IO/File.h>
#include <Common/System/Query.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Math/Radius.h>
#include <Common/EnumFlags.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <android/sensor.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/keycodes.h>
#include <android/input.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters& commandLineParameters);

namespace ngine::Platform::Android
{
	Application& /* static */ Application::GetInstance()
	{
		static Application application;
		return application;
	}

	static ngine::Optional<ngine::Rendering::Window*> s_pWindow;
	static ngine::UniquePtr<GameActivityWindow> s_pActivityWindow;
	static ngine::UniquePtr<ngine::EngineSystems> s_pEngineSystems;

	void android_handle_cmd([[maybe_unused]] android_app* pApp, [[maybe_unused]] int32_t cmd);
	void android_wait_until_initialized(android_app* pApp);
	void android_process(android_app* pApp);

	void java_print_runtime_memory(android_app* pApp);

	void Initialize(android_app* pApp);
	void ProcessInput(android_app* pApp);
	void ProcessTextInput(android_app* pApp);
	void Destroy(android_app* pApp);

	ngine::IO::Path GetAndroidCacheDirectory(android_app* pApp);

	void Application::Start(android_app* pApp)
	{
		Initialize(pApp);

		android_wait_until_initialized(pApp);

		while (true)
		{
			android_process(pApp);

			ngine::Rendering::Window::ProcessWindowMessages();

			ProcessInput(pApp);
			ProcessTextInput(pApp);

			if (!s_pEngineSystems->m_engine.DoTick())
			{
				break;
			}
		}

		s_pEngineSystems->m_engine.OnBeforeQuit();

		Destroy(pApp);
	}

	void android_handle_cmd([[maybe_unused]] android_app* pApp, [[maybe_unused]] int32_t cmd)
	{
		switch ((NativeAppGlueAppCmd)cmd)
		{
			case UNUSED_APP_CMD_INPUT_CHANGED:
				break;
			case APP_CMD_INIT_WINDOW:
			{
				using namespace ngine;
				if (s_pActivityWindow.IsValid())
				{
					s_pActivityWindow->OnWindowRecreated(pApp);

					System::Get<Engine>().ModifyFrameGraph(
						[&window = *s_pWindow]()
						{
							window.OnSwitchToForeground();
							window.RecreateSurface();

							window.OnReceivedKeyboardFocus();
						}
					);
				}
				else
				{
					Application& application = Application::GetInstance();

					s_pActivityWindow = UniquePtr<Android::GameActivityWindow>::Make(pApp);
					application.m_pGameActivityWindow = s_pActivityWindow;
					Rendering::Window::GetOnWindowCreated().Add(
						*pApp,
						[](android_app&, Rendering::Window& window)
						{
							s_pWindow = &window;

							Application& application = Application::GetInstance();
							application.m_pGameActivityWindow->SetWindow(window);
						}
					);

					Threading::JobBatch jobBatch;
					Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{s_pActivityWindow.Get(), jobBatch});

					if (jobBatch.IsValid())
					{
						Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
						thread.Queue(jobBatch);
					}

					application.m_gestureDetector.Initialize(*s_pWindow);
					application.m_controllerManager.Initialize(*s_pWindow);
				}
			}
			break;
			case APP_CMD_WINDOW_RESIZED:
			case APP_CMD_CONFIG_CHANGED:
			{
				if (s_pWindow != nullptr)
				{
					const ngine::Math::Vector2ui newClientAreaSize{
						(ngine::uint32)ANativeWindow_getWidth(pApp->window),
						(ngine::uint32)ANativeWindow_getHeight(pApp->window)
					};
					s_pWindow->TryOrQueueResize(newClientAreaSize);
				}
			}
			break;
			case APP_CMD_CONTENT_RECT_CHANGED:
				break;
			case APP_CMD_TERM_WINDOW:
			{
				if (s_pWindow != nullptr)
				{
					using namespace ngine;
					s_pWindow->OnLostKeyboardFocus();
					s_pWindow->OnSwitchToBackground();
					Rendering::LogicalDevice& logicalDevice = s_pWindow->GetLogicalDevice();
					logicalDevice.WaitUntilIdle();
					Assert(!s_pWindow->IsInForeground());
				}
			}
			break;
			case APP_CMD_WINDOW_REDRAW_NEEDED:
				break;
			case APP_CMD_GAINED_FOCUS:
				break;
			case APP_CMD_LOST_FOCUS:
				break;
			case APP_CMD_LOW_MEMORY:
				// Assert(false, "TODO");
				break;
			case APP_CMD_START:
				break;
			case APP_CMD_RESUME:
				break;
			case APP_CMD_SAVE_STATE:
				break;
			case APP_CMD_PAUSE:
				break;
			case APP_CMD_STOP:
				break;
			case APP_CMD_DESTROY:
				s_pEngineSystems->m_engine.Quit();
				break;
			case APP_CMD_WINDOW_INSETS_CHANGED:
				break;
			case APP_CMD_SOFTWARE_KB_VIS_CHANGED:
				break;
			case APP_CMD_EDITOR_ACTION:
				break;
			case APP_CMD_KEY_EVENT:
				break;
			case APP_CMD_TOUCH_EVENT:
				break;
		}
	}

	void android_wait_until_initialized(android_app* pApp)
	{
		while (s_pWindow.IsInvalid())
		{
			android_process(pApp);
		}
	}

	void android_process(android_app* pApp)
	{
		int events;
		android_poll_source* pSource;
		if (ALooper_pollAll(0, nullptr, &events, (void**)&pSource) >= 0)
		{
			if (pSource)
			{
				pSource->process(pApp, pSource);
			}
		}
	}

	void java_print_runtime_memory(android_app* pApp)
	{
		JNIEnv* pEnv;
		pApp->activity->vm->AttachCurrentThread(&pEnv, NULL);

		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		jmethodID getRuntimeMemorySize = pEnv->GetMethodID(application.m_javaContext.gameActivityClass, "GetRuntimeMemorySize", "()J");
		jlong result = pEnv->CallLongMethod(pApp->activity->javaGameActivity, getRuntimeMemorySize);
		LogMessage("Java virtual machine runtime free memory size: {}", result);

		pApp->activity->vm->DetachCurrentThread();
	}

	void Initialize(android_app* pApp)
	{
		Assert(s_pEngineSystems.IsInvalid(), "Engine can't be started twice!");
		using namespace ngine;

		pApp->onAppCmd = android_handle_cmd;

		java_print_runtime_memory(pApp);

		IO::Internal::GetAndroidAssetManager() = pApp->activity->assetManager;
		IO::Internal::GetCacheDirectory() = GetAndroidCacheDirectory(pApp);
		IO::Internal::GetAppDataDirectory() = IO::Path(
			IO::Path::StringType(pApp->activity->internalDataPath, (IO::Path::StringType::SizeType)strlen(pApp->activity->internalDataPath))
		);

		CommandLine::InitializationParameters commandLineParameters;
		s_pEngineSystems = CreateEngine(commandLineParameters);
		Threading::Atomic<bool> startedEngine{false};
		s_pEngineSystems->m_startupJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[&startedEngine](Threading::JobRunnerThread&)
			{
				startedEngine = true;
			},
			Threading::JobPriority::LoadPlugin
		));
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		thread.Queue(s_pEngineSystems->m_startupJobBatch);
		while (!startedEngine)
		{
			thread.DoRunNextJob();
		}

		pApp->userData = &s_pEngineSystems->m_engine;

		android_app_set_motion_event_filter(pApp, NULL); // NULL to allow game controllers
		android_app_set_key_event_filter(pApp, pApp->keyEventFilter);

		GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_TOUCH_MINOR);
		GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_TOUCH_MAJOR);
		GameActivityPointerAxes_enableAxis(AMOTION_EVENT_AXIS_PRESSURE);
	}

	void Destroy(android_app* pApp)
	{
		JNIEnv* pEnv;
		pApp->activity->vm->AttachCurrentThread(&pEnv, NULL);
		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		pEnv->DeleteGlobalRef(application.m_javaContext.gameActivityClass);
		pApp->activity->vm->DetachCurrentThread();
	}

	void ProcessInput(android_app* pApp)
	{
		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		application.m_controllerManager.Update();

		const ngine::int32 displayDensity = AConfiguration_getDensity(pApp->config);

		if (android_input_buffer* inputBuffer = android_app_swap_input_buffers(pApp))
		{
			if (inputBuffer->motionEventsCount)
			{
				for (uint64_t i = 0; i < inputBuffer->motionEventsCount; ++i)
				{
					application.m_gestureDetector.ProcessMotionEvent(inputBuffer->motionEvents[i], displayDensity);
					application.m_controllerManager.ProcessMotionEvent(inputBuffer->motionEvents[i]);
				}
				android_app_clear_motion_events(inputBuffer);
			}
			if (inputBuffer->keyEventsCount)
			{
				for (uint64_t i = 0; i < inputBuffer->keyEventsCount; ++i)
				{
					application.m_controllerManager.ProcessKeyEvent(inputBuffer->keyEvents[i]);
				}
				android_app_clear_key_events(inputBuffer);
			}
		}
	}

	void ProcessTextInput(android_app* pApp)
	{
		using namespace ngine;

		if (pApp->textInputState)
		{
			GameActivity_getTextInputState(
				pApp->activity,
				[](void* pContext, const GameTextInputState* pState)
				{
					static int32 sOldTextLength = 0;

					android_app* pApp = (android_app*)pContext;
					if (!pApp || !pState)
					{
						return;
					}

					const int32 newTextLength = static_cast<uint32>(pState->text_length);
					if (newTextLength < sOldTextLength)
					{
						s_pWindow->DeleteTextInInputFocusBackwards();
					}
					else if (newTextLength > sOldTextLength)
					{
						const uint32 diff = Math::Abs(newTextLength - sOldTextLength);
						const char* pModifiedUTF8 = pState->text_UTF8 + sOldTextLength;
						UnicodeString input = Internal::FromModifiedUTF8(ConstStringView{pModifiedUTF8, diff});
						s_pWindow->InsertTextIntoInputFocus(Move(input));
					}
					sOldTextLength = newTextLength;

					// Clear the text input flag.
					pApp->textInputState = 0;
				},
				pApp
			);
		}
	}

	ngine::IO::Path GetAndroidCacheDirectory(android_app* pApp)
	{
		using namespace ngine;

		IO::Path path;
		{
			JNIEnv* env;
			pApp->activity->vm->AttachCurrentThread(&env, NULL);

			jclass activityClass = env->FindClass("android/app/NativeActivity");
			jmethodID getCacheDir = env->GetMethodID(activityClass, "getCacheDir", "()Ljava/io/File;");
			jobject cache_dir = env->CallObjectMethod(pApp->activity->javaGameActivity, getCacheDir);

			jclass fileClass = env->FindClass("java/io/File");
			jmethodID getPath = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
			jstring path_string = (jstring)env->CallObjectMethod(cache_dir, getPath);

			const char* path_chars = env->GetStringUTFChars(path_string, NULL);
			ConstStringView temp_folder(path_chars, (String::SizeType)strlen(path_chars));

			path = IO::Path(IO::Path::StringType(temp_folder));

			env->ReleaseStringUTFChars(path_string, path_chars);
			pApp->activity->vm->DetachCurrentThread();
		}

		return Move(path);
	}
}

extern "C"
{
	JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* pVm, void* pReserved)
	{
		UNUSED(pReserved);

		JNIEnv* pEnv;
		if (pVm->GetEnv((void**)&pEnv, JNI_VERSION_1_6) != JNI_OK)
		{
			return JNI_ERR; // JNI version not supported.
		}

		jclass gameActivityClass = pEnv->FindClass("com/sceneri/App/NativeGameActivity");
		if (gameActivityClass == nullptr)
		{
			return JNI_ERR;
		}

		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		application.m_javaContext.gameActivityClass = static_cast<jclass>(pEnv->NewGlobalRef(gameActivityClass));

		return JNI_VERSION_1_6;
	}

	JNIEXPORT void JNICALL Java_com_sceneri_App_NativeEngineWrapper_OnDeviceAdded(JNIEnv*, jobject, jint deviceId, jlong activeAxisIds)
	{
		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		application.m_controllerManager.AddController(deviceId);
		application.m_controllerManager.EnableAxisIds(activeAxisIds);
		application.m_controllerManager.DisableVirtualController();
	}

	JNIEXPORT void JNICALL Java_com_sceneri_App_NativeEngineWrapper_OnDeviceRemoved(JNIEnv*, jobject, jint deviceId)
	{
		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		application.m_controllerManager.RemoveController(deviceId);
		if (!application.m_controllerManager.GetControllerCount())
		{
			application.m_controllerManager.EnableVirtualController();
		}
	}

	JNIEXPORT void JNICALL Java_com_sceneri_App_NativeEngineWrapper_OnDeviceChanged(JNIEnv*, jobject, jint deviceId)
	{
		LogMessage("Unhandled OnDeviceChanged {}", deviceId);
	}

	JNIEXPORT JNICALL void
	Java_com_sceneri_App_NativeEngineWrapper_SetViewControllerSettings(JNIEnv*, jobject, jint scaledTouchSlope, jint scaledMinimumScalingSpan)
	{
		ngine::Platform::Android::Application& application = ngine::Platform::Android::Application::GetInstance();
		application.m_gestureDetector.SetViewControllerSettings(scaledTouchSlope, scaledMinimumScalingSpan);
	}
}
#endif
