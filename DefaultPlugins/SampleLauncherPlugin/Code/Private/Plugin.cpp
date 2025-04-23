#include "Plugin.h"

#include "Windows/MainAppWindow.h"

#include <Engine/Engine.h>
#include <Engine/IO/Filesystem.h>

#include <Renderer/Window/DocumentData.h>
#include <Renderer/Renderer.h>

#include <Backend/Plugin.h>

#include <Common/Platform/Environment.h>
#include <Common/System/Query.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
#import <UIKit/UIWindowScene.h>
#import <UIKit/UIScreen.h>
#endif

namespace ngine::App::Core
{
	void Plugin::OnLoaded(Application& application)
	{
		Engine& engine = static_cast<Engine&>(application);

		if constexpr (Platform::CanSwitchEnvironmentAtRuntime)
		{
			if (const OptionalIterator<const CommandLine::Argument> pEnvironmentArgument = engine.GetCommandLineArguments().FindArgument(MAKE_NATIVE_LITERAL("environment"), CommandLine::Prefix::Minus))
			{
				const String environmentName(pEnvironmentArgument->value.GetView());
				Platform::SwitchEnvironment(Platform::GetEnvironment(environmentName));
			}
		}
#if PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_IOS
		// Reuse logical devices
		static Optional<Rendering::LogicalDevice*> pReusableLogicalDevice;
		if (pReusableLogicalDevice.IsInvalid())
		{
			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			// TODO: Handle multiple GPUs on Mac?
			Rendering::PhysicalDevices::DeviceView physicalDevices = renderer.GetPhysicalDevices().GetView();
			if (physicalDevices.HasElements())
			{
				Rendering::PhysicalDevice& physicalDevice = physicalDevices[0];
				pReusableLogicalDevice = renderer.CreateLogicalDeviceFromPhysicalDevice(physicalDevice);
			}
		}
		Optional<Rendering::LogicalDevice*> pLogicalDevice = pReusableLogicalDevice;
#else
		Optional<Rendering::LogicalDevice*> pLogicalDevice;
#endif

		Rendering::Window::GetRequestWindowCreationCallback() =
			[&engine, pLogicalDevice]([[maybe_unused]] Rendering::Window::CreationRequestParameters&& windowCreationRequest
		  ) -> Optional<Rendering::Window*>
		{
			Math::Rectanglei windowArea{};

#if PLATFORM_APPLE_VISIONOS
			windowArea.m_size = Math::Zero;
#else
			Math::Vector2i size = (Math::Vector2i)Rendering::Window::GetMainScreenUsableBounds();
			if (const OptionalIterator<const CommandLine::Argument> pWidth = engine.GetCommandLineArguments().FindArgument(MAKE_NATIVE_LITERAL("width"), CommandLine::Prefix::Minus))
			{
				size.x = pWidth->value.GetView().ToIntegral<int32>();
			}
			if (const OptionalIterator<const CommandLine::Argument> pHeight = engine.GetCommandLineArguments().FindArgument(MAKE_NATIVE_LITERAL("height"), CommandLine::Prefix::Minus))
			{
				size.y = pHeight->value.GetView().ToIntegral<int32>();
			}
			windowArea.m_size = size;
#endif

			UI::Document::MainWindow& mainWindow = UI::Document::MainWindow::Create(Rendering::Window::Initializer {
				pLogicalDevice, EngineInfo::Name, windowArea, Rendering::Window::CreationFlags{},
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
					windowCreationRequest.m_pWindowScene,
#elif PLATFORM_ANDROID
				windowCreationRequest.m_pGameActivityWindow,
#elif PLATFORM_WEB
				windowCreationRequest.m_canvasSelector,
#endif
					windowCreationRequest.m_jobBatch
			});
			auto openDocuments = [&mainWindow, &engine]()
			{
				if constexpr (PACKAGED_BUILD && ConstStringView(PACKAGED_PROJECT_FILE_PATH).HasElements())
				{
					PUSH_CLANG_WARNINGS
					DISABLE_CLANG_WARNING("-Wunreachable-code");

					IO::Path filePath = IO::Path::Combine(System::Get<IO::Filesystem>().GetEnginePath(), MAKE_PATH(PACKAGED_PROJECT_FILE_PATH));
					filePath.MakeNativeSlashes();
					Assert(filePath.Exists());

					Threading::JobBatch jobBatch = mainWindow.OpenDocuments(
						Array<Widgets::DocumentData, 1>{Widgets::DocumentData{filePath}}.GetView(),
						Rendering::Window::OpenDocumentFlags{}
					);
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);

					POP_CLANG_WARNINGS
				}
				else if (const OptionalIterator<const CommandLine::Argument> pProjectArgument = engine.GetCommandLineArguments().FindArgument(MAKE_NATIVE_LITERAL("project"), CommandLine::Prefix::Minus))
				{
					const Asset::Guid projectGuid = Guid::TryParse(pProjectArgument->value.GetView());

					if (projectGuid.IsValid())
					{
						Threading::JobBatch jobBatch = mainWindow.OpenDocuments(
							Array<Widgets::DocumentData, 1>{Widgets::DocumentData{projectGuid}}.GetView(),
							Rendering::Window::OpenDocumentFlags{}
						);
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
					else
					{
						IO::Path filePath(pProjectArgument->value.GetView());
						if (filePath.IsRelative())
						{
							filePath = IO::Path::Combine(System::Get<IO::Filesystem>().GetEnginePath(), filePath);
						}

						filePath.MakeNativeSlashes();
						if (filePath.GetFileNameWithoutExtensions() != MAKE_PATH("Project"))
						{
							filePath = IO::Path::Combine(filePath, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension));
						}
						Assert(filePath.Exists());

						Threading::JobBatch jobBatch = mainWindow.OpenDocuments(
							Array<Widgets::DocumentData, 1>{Widgets::DocumentData{filePath}}.GetView(),
							Rendering::Window::OpenDocumentFlags{}
						);
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
				}
			};
			if (const Optional<Networking::Backend::Plugin*> pBackend = System::FindPlugin<Networking::Backend::Plugin>())
			{
				pBackend->GetGame().QueuePostSessionRegistrationCallback(
					[openDocuments = Move(openDocuments)](const EnumFlags<Networking::Backend::SignInFlags>)
					{
						openDocuments();
					}
				);
			}
			else
			{
				openDocuments();
			}

			mainWindow.MakeVisible();
			mainWindow.GiveFocus();

			return &mainWindow;
		};
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::App::Core::Plugin>();
#else
extern "C" APPCORE_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::App::Core::Plugin(application);
}
#endif
