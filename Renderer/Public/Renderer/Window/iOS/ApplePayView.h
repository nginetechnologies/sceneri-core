#pragma once

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#elif PLATFORM_APPLE_MACOS
#import <AppKit/NSViewController.h>
#endif

#import <PassKit/PassKit.h>

@interface ApplePayView
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	: UIViewController <PKPaymentAuthorizationViewControllerDelegate>
#else
	: NSViewController <PKPaymentAuthorizationViewControllerDelegate>
#endif

- (void)checkOut;

@end
