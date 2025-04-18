#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#include "Renderer/Window/iOS/AppDelegate.h"
#elif PLATFORM_APPLE_MACOS
#import <Cocoa/Cocoa.h>
#include "Renderer/Window/iOS/AppDelegate.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#elif PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <emscripten/em_js.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#endif

#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/Threading/Thread.h>
#include <Common/System/Query.h>

#include <Engine/Threading/JobRunnerThread.h>

#if USE_SDL
#include <SDL2/SDL_main.h>
#undef None
#undef Status
#endif

#include <Renderer/Window/Window.h>

extern "C"
{
	PUSH_CLANG_WARNINGS
	DISABLE_CLANG_WARNING("-Wmissing-variable-declarations")

	extern DLL_EXPORT unsigned long NvOptimusEnablement = 1;
	extern DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;

	POP_CLANG_WARNINGS
}

#if PLATFORM_WINDOWS
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR commandLine, int)
{
	using namespace ngine;

	UniquePtr<EngineSystems> pEngineSystems =
		CreateEngine(CommandLine::InitializationParameters(NativeStringView(commandLine, static_cast<uint32>(wcslen(commandLine)))));
	pEngineSystems->m_startupJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
		[](Threading::JobRunnerThread& thread)
		{
			Threading::JobBatch jobBatch;
			Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{jobBatch});
			if (jobBatch.IsValid())
			{
				thread.Queue(jobBatch);
			}
		},
		Threading::JobPriority::LoadPlugin
	));
	Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
	thread.Queue(pEngineSystems->m_startupJobBatch);

	pEngineSystems->m_engine.RunMainLoop();
	return 0;
}

#elif PLATFORM_APPLE_VISIONOS

// Rest done in App.swift

#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_MACOS

#if PLATFORM_APPLE_IOS
int main(int argumentCount, char* pArguments[])
#else
int main(int argumentCount, const char* _Nonnull pArguments[_Nonnull])
#endif
{
	using namespace ngine;

	Vector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	uint16 argumentIndex = 0;
	if (argumentCount > 0)
	{
		const IO::Path executablePath = IO::Path::GetExecutablePath();
		const ConstNativeStringView firstArgument{pArguments[argumentIndex], (uint16)strlen(pArguments[argumentIndex])};
		if (executablePath.GetView().GetStringView().EndsWith(firstArgument))
		{
			argumentIndex++;
		}
	}

	for (; argumentIndex < argumentCount; ++argumentIndex)
	{
		arguments.EmplaceBack(pArguments[argumentIndex], (uint16)strlen(pArguments[argumentIndex]));
	}

	ngine::CommandLine::InitializationParameters::GetGlobalParameters() = CommandLine::InitializationParameters(arguments.GetView());

	@autoreleasepool
	{
#if PLATFORM_APPLE_IOS
		return UIApplicationMain(argumentCount, pArguments, NSStringFromClass([::Application class]), NSStringFromClass([AppDelegate class]));
#elif PLATFORM_APPLE_MACOS
		return NSApplicationMain(argumentCount, pArguments);
#endif
	}
}
#elif PLATFORM_ANDROID
struct android_app;

extern "C"
{
	void android_main(android_app* pApp)
	{
		using namespace ngine;
		Platform::Android::Application& androidApplication = Platform::Android::Application::GetInstance();
		androidApplication.Start(pApp);
	}
}
#elif PLATFORM_EMSCRIPTEN
int main(int argumentCount, char* pArguments[])
{
	using namespace ngine;

	Vector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	for (int i = 0; i < argumentCount; ++i)
	{
		arguments.EmplaceBack(pArguments[i], (uint16)strlen(pArguments[i]));
	}

	const CommandLine::InitializationParameters commandLineParameters(arguments.GetView());
	UniquePtr<EngineSystems> pEngineSystems = CreateEngine(commandLineParameters);
	pEngineSystems->m_startupJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
		[](Threading::JobRunnerThread& thread)
		{
			Threading::JobBatch jobBatch;
			Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{"canvas", jobBatch});
			if (jobBatch.IsValid())
			{
				thread.Queue(jobBatch);
			}
		},
		Threading::JobPriority::LoadPlugin
	));
	Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
	thread.Queue(pEngineSystems->m_startupJobBatch);

	static auto isProcessingFrame = []()
	{
		const Optional<Threading::JobManager*> pJobManager = System::Find<Threading::JobManager>();
		Assert(pJobManager.IsValid());

		for (uint8 frameIndex = 0; frameIndex < Rendering::MaximumConcurrentFrameCount; ++frameIndex)
		{
			for (Threading::JobRunnerThread& thread : pJobManager->GetJobThreads())
			{
				Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
				Rendering::JobRunnerData& jobRunnerData = engineThread.GetRenderData();

				if (jobRunnerData.IsProcessingFrame(frameIndex))
				{
					return true;
				}
			}
		}
		return false;
	};

	if (emscripten_has_threading_support())
	{
		while (true)
		{
			if (isProcessingFrame())
			{
				em_proxying_queue* queue = emscripten_proxy_get_system_queue();
				emscripten_proxy_execute_queue(queue);
				continue;
			}

			const bool continueRunning = pEngineSystems->m_engine.DoTick();
			if (!continueRunning)
			{
				break;
			}
		}
	}
	else
	{
		emscripten_set_main_loop_arg(
			[](void* pUserData)
			{
				emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 16);

				EngineSystems& engineSystems = *reinterpret_cast<EngineSystems*>(pUserData);
				const bool continueRunning = engineSystems.m_engine.DoTick();
				if (!continueRunning)
				{
					emscripten_cancel_main_loop();
				}
			},
			pEngineSystems.Get(),
			60,
			true
		);
	}

	pEngineSystems->m_engine.OnBeforeQuit();

	return 0;
}
#else
int main(int argumentCount, char* pArguments[])
{
	using namespace ngine;

	Vector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	for (int i = 0; i < argumentCount; ++i)
	{
		arguments.EmplaceBack(pArguments[i], (uint16)strlen(pArguments[i]));
	}

	const CommandLine::InitializationParameters commandLineParameters(arguments.GetView());
	UniquePtr<EngineSystems> pEngineSystems = CreateEngine(commandLineParameters);
	pEngineSystems->m_startupJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
		[](Threading::JobRunnerThread& thread)
		{
			Threading::JobBatch jobBatch;
			Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{jobBatch});
			if (jobBatch.IsValid())
			{
				thread.Queue(jobBatch);
			}
		},
		Threading::JobPriority::LoadPlugin
	));
	Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
	thread.Queue(pEngineSystems->m_startupJobBatch);

	Engine& engine = pEngineSystems->m_engine;
	engine.RunMainLoop();
	return 0;
}
#endif
