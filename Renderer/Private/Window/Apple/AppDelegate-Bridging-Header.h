#include <Common/Platform/ConfigureCompiler.h>
#include <Common/Platform/ConfigureArchitecture.h>
#include <Common/Platform/ConfigurePlatform.h>
#include <Common/Platform/ConfigureVectorization.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import "../../../Renderer/Public/Renderer/Window/iOS/AppDelegate.h"
#import "../../../Renderer/Public/Renderer/Window/iOS/ViewController.h"
#import "../../../Renderer/Public/Renderer/Window/iOS/MetalView.h"
#import "../../../Renderer/Public/Renderer/Window/iOS/SceneDelegate.h"
#endif
