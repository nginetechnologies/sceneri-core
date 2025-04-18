#if PLATFORM_ANDROID
#include "GameActivityWindow.h"
#include "AndroidApplication.h"

#include <Common/Math/Vector2.h>

#include <Engine/Engine.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Window/DocumentData.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/IO/File.h>
#include <Common/System/Query.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Math/Radius.h>
#include <Common/EnumFlags.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <android/sensor.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/keycodes.h>
#include <android/input.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/native_window.h>
#include <game-activity/GameActivity.h>

namespace ngine::Platform::Android
{
	GameActivityWindow::GameActivityWindow(android_app* pAndroidApp)
		: m_pApplication(pAndroidApp)
		, m_pGameActivity(pAndroidApp->activity)
		, m_pNativeWindow(pAndroidApp->window)
	{
		m_dotsPerInch = (static_cast<float>(AConfiguration_getDensity(pAndroidApp->config)) / static_cast<float>(ACONFIGURATION_DENSITY_MEDIUM)
		                ) *
		                100.f;
	}

	void GameActivityWindow::OnWindowRecreated(android_app* pAndroidApp)
	{
		m_pNativeWindow = pAndroidApp->window;
	}

	void GameActivityWindow::SetWindow(Rendering::Window& window)
	{
		m_pWindow = window;
	}

	ANativeWindow* GameActivityWindow::GetNativeWindow() const
	{
		return m_pNativeWindow;
	}

	Math::Vector2ui GameActivityWindow::GetClientAreaSize() const
	{
		return Math::Vector2ui{(uint32)ANativeWindow_getWidth(m_pNativeWindow), (uint32)ANativeWindow_getHeight(m_pNativeWindow)};
	}

	float GameActivityWindow::GetDotsPerInch() const
	{
		return m_dotsPerInch;
	}

	void GameActivityWindow::ShowVirtualKeyboard(const EnumFlags<InputTypeFlags> inputTypeFlags) const
	{
		GameActivity_setImeEditorInfo(m_pGameActivity, (GameTextInputType)inputTypeFlags.GetUnderlyingValue(), IME_ACTION_DONE, IME_NULL);
		GameActivity_showSoftInput(m_pGameActivity, GAMEACTIVITY_SHOW_SOFT_INPUT_IMPLICIT);
	}

	void GameActivityWindow::HideVirtualKeyboard() const
	{
		GameActivity_hideSoftInput(m_pGameActivity, GAMEACTIVITY_HIDE_SOFT_INPUT_IMPLICIT_ONLY);
	}

	void GameActivityWindow::SignInWithGoogleCachedCredentials(SignInWithGoogleCallback&& callback)
	{
		Assert(!m_signInWithGoogleCallback.IsValid());
		m_signInWithGoogleCallback = Forward<SignInWithGoogleCallback>(callback);

		JNIEnv* pEnv;
		m_pApplication->activity->vm->AttachCurrentThread(&pEnv, NULL);

		Application& application = Application::GetInstance();

		jmethodID function = pEnv->GetMethodID(application.m_javaContext.gameActivityClass, "SignInWithGoogleCached", "()V");

		pEnv->CallVoidMethod(m_pApplication->activity->javaGameActivity, function);

		m_pApplication->activity->vm->DetachCurrentThread();
	}

	void GameActivityWindow::SignInWithGoogle(SignInWithGoogleCallback&& callback)
	{
		Assert(!m_signInWithGoogleCallback.IsValid());
		m_signInWithGoogleCallback = Forward<SignInWithGoogleCallback>(callback);

		JNIEnv* pEnv;
		m_pApplication->activity->vm->AttachCurrentThread(&pEnv, NULL);

		Application& application = Application::GetInstance();

		jmethodID function = pEnv->GetMethodID(application.m_javaContext.gameActivityClass, "SignInWithGoogle", "()V");

		pEnv->CallVoidMethod(m_pApplication->activity->javaGameActivity, function);

		m_pApplication->activity->vm->DetachCurrentThread();
	}

	void GameActivityWindow::SignOutOfAllProviders() const
	{
		JNIEnv* pEnv;
		m_pApplication->activity->vm->AttachCurrentThread(&pEnv, NULL);

		Application& application = Application::GetInstance();

		jmethodID function = pEnv->GetMethodID(application.m_javaContext.gameActivityClass, "SignOutFromAllProviders", "()V");
		pEnv->CallVoidMethod(m_pApplication->activity->javaGameActivity, function);

		m_pApplication->activity->vm->DetachCurrentThread();
	}

	void GameActivityWindow::OnReceivedDeepLink(const ConstStringView link)
	{
		Platform::Android::Application& application = Platform::Android::Application::GetInstance();
		Platform::Android::GameActivityWindow& gameActivityWindow = *application.m_pGameActivityWindow;
		if (const Optional<Rendering::Window*> pWindow = gameActivityWindow.GetWindow())
		{
			EnumFlags<Rendering::Window::OpenDocumentFlags> openDocumentFlags;
			FixedCapacityVector<Widgets::DocumentData> documents(Memory::Reserve, 1);

			if (link.StartsWith("http://") || link.StartsWith("https://"))
			{
				IO::URI uri{link};
				if (Optional<IO::URI> unescapedURI = IO::URI::Unescape(uri))
				{
					uri = *unescapedURI;
				}

				documents.EmplaceBack(Widgets::DocumentData{Move(uri)});
			}
			else
			{
				using namespace ngine;
				const IO::PathView pathView{link};
				if (pathView.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension && pathView.GetFileNameWithoutExtensions() != MAKE_PATH("Project"))
				{
					documents.EmplaceBack(Widgets::DocumentData{
						IO::Path::Combine(pathView, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
					});
				}
				else
				{
					documents.EmplaceBack(Widgets::DocumentData{IO::Path(pathView)});
				}

				// Always open local files for editing
				openDocumentFlags |= Rendering::Window::OpenDocumentFlags::EnableEditing;
			}

			if (documents.HasElements())
			{
				Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
				jobManager.QueueCallback(
					[pWindow, documents = Move(documents), openDocumentFlags](Threading::JobRunnerThread& thread)
					{
						Threading::JobBatch jobBatch = pWindow->OpenDocuments(documents, openDocumentFlags);
						if (jobBatch.IsValid())
						{
							thread.Queue(jobBatch);
						}
					},
					Threading::JobPriority::UserInterfaceAction
				);
			}
		}
		else
		{
			Rendering::Window::QueueOnWindowThread(
				[this, link = String(link)]()
				{
					OnReceivedDeepLink(link);
				}
			);
		}
	}
}

extern "C"
{
	JNIEXPORT void JNICALL
	Java_com_sceneri_App_NativeEngineWrapper_OnSignedInWithGoogle(JNIEnv* pEnv, jobject, jstring userIdentifier, jstring idToken)
	{
		using namespace ngine;
		Platform::Android::Application& application = Platform::Android::Application::GetInstance();
		Platform::Android::GameActivityWindow& gameActivityWindow = *application.m_pGameActivityWindow;

		const char* userIdentifierString = pEnv->GetStringUTFChars(userIdentifier, nullptr);
		const char* idTokenString = pEnv->GetStringUTFChars(idToken, nullptr);
		Assert(userIdentifierString != nullptr && idTokenString != nullptr);
		if (userIdentifierString == nullptr || idTokenString == nullptr)
		{
			gameActivityWindow.m_signInWithGoogleCallback({}, {});
			gameActivityWindow.m_signInWithGoogleCallback = {};
			return;
		}

		gameActivityWindow.m_signInWithGoogleCallback(
			ConstStringView{userIdentifierString, (ConstStringView::SizeType)strlen(userIdentifierString)},
			ConstStringView{idTokenString, (ConstStringView::SizeType)strlen(idTokenString)}
		);
		gameActivityWindow.m_signInWithGoogleCallback = {};

		pEnv->ReleaseStringUTFChars(userIdentifier, userIdentifierString);
		pEnv->ReleaseStringUTFChars(idToken, idTokenString);
	}

	JNIEXPORT void JNICALL Java_com_sceneri_App_NativeEngineWrapper_OnSignInWithGoogleFailed(JNIEnv*, jobject)
	{
		using namespace ngine;
		Platform::Android::Application& application = Platform::Android::Application::GetInstance();
		Platform::Android::GameActivityWindow& gameActivityWindow = *application.m_pGameActivityWindow;

		gameActivityWindow.m_signInWithGoogleCallback({}, {});
		gameActivityWindow.m_signInWithGoogleCallback = {};
	}

	JNIEXPORT void JNICALL Java_com_sceneri_App_NativeEngineWrapper_OnReceiveUrlIntent(JNIEnv* pEnv, jobject, jstring javaUrlString)
	{
		using namespace ngine;

		const char* urlStringPointer = pEnv->GetStringUTFChars(javaUrlString, nullptr);
		if (urlStringPointer == nullptr)
		{
			return;
		}
		const ConstStringView urlString{urlStringPointer, (ConstStringView::SizeType)strlen(urlStringPointer)};
		String link{urlString};

		Rendering::Window::QueueOnWindowThread(
			[link = Move(link)]()
			{
				Platform::Android::Application& application = Platform::Android::Application::GetInstance();
				Platform::Android::GameActivityWindow& gameActivityWindow = *application.m_pGameActivityWindow;
				gameActivityWindow.OnReceivedDeepLink(link);
			}
		);

		pEnv->ReleaseStringUTFChars(javaUrlString, urlStringPointer);
	}
}
#endif
