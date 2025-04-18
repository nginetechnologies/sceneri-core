#pragma once

#import <StoreKit/SKPaymentQueue.h>
#import <StoreKit/SKPaymentTransaction.h>
#import <StoreKit/SKProductsRequest.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIApplication.h>
#elif PLATFORM_APPLE_MACOS
#import <Cocoa/Cocoa.h>
#endif

#if PLATFORM_APPLE_VISIONOS
#import <CompositorServices/CompositorServices.h>
#endif

@class SKTransaction;

@interface Application
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	: UIApplication
#elif PLATFORM_APPLE_MACOS
	: NSApplication
#endif
@end

typedef void (^RegisteredForRemoteNotificationsCallback)(NSData*);
#if PLATFORM_APPLE_VISIONOS
typedef void (^InitializeLayerRendererCallback)(cp_layer_renderer_t, void* pLogicalDevice);
typedef void (^ImmersiveSpaceCallback)();
#endif

@interface AppDelegate :
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	UIResponder <
		UIApplicationDelegate
#elif PLATFORM_APPLE_MACOS
	NSResponder <NSApplicationDelegate
#endif
#if !PLATFORM_APPLE_VISIONOS
		,
		SKProductsRequestDelegate,
		SKPaymentTransactionObserver
#endif
		>
{
	bool m_isSandboxPurchasingEnvironment;
	void* m_pEngineSystems;
@public
	RegisteredForRemoteNotificationsCallback OnRegisteredForRemoteNotifications;
}

#if !PLATFORM_APPLE_VISIONOS
- (void)requestInAppPurchaseProducts:(NSArray<NSString*>*)products;
- (void)queuePurchase:(SKProduct*)product quantity:(unsigned int)quantity;
- (void)finishTransaction:(SKPaymentTransaction*)transaction;
#endif

#if PLATFORM_APPLE_VISIONOS
- (void)setInitializeLayerRendererCallback:(InitializeLayerRendererCallback)callbackBlock;
- (void)initializeLayerRenderer:(cp_layer_renderer_t)layerRenderer pLogicalDevice:(void*)pLogicalDevice;
- (void)startFullImmersiveSpace:(ImmersiveSpaceCallback)callbackBlock pLogicalDevice:(void*)pLogicalDevice;
- (void)startMixedImmersiveSpace:(ImmersiveSpaceCallback)callbackBlock pLogicalDevice:(void*)pLogicalDevice;
- (void)stopImmersiveSpace:(ImmersiveSpaceCallback)callbackBlock;
#endif

@end
