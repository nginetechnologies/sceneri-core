#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIWindow.h>
#elif PLATFORM_APPLE_MACOS
#import <AppKit/NSWindow.h>
#endif

@interface Window
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	: UIWindow
#elif PLATFORM_APPLE_MACOS
	: NSWindow
#endif
@end
