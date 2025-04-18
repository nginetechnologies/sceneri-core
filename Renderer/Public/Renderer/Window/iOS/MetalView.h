#pragma once

#import <GameController/GameController.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#elif PLATFORM_APPLE_MACOS
#import <AppKit/NSView.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

@class GestureDelegate;

@interface MetalView
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	: UIView <UIContextMenuInteractionDelegate, UIKeyInput>
#elif PLATFORM_APPLE_MACOS
	: NSView
#endif
{
	GestureDelegate* m_pGestureDelegate;
	void* _engineWindow;
}

@property(nonatomic, unsafe_unretained) CAMetalLayer* metalLayer;
@property(nonatomic) void* engineWindow;
@property(nonatomic, assign) void* controllerManager;
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
@property(nonatomic, assign) UIKeyboardType keyboardType;
#endif
@property(nonatomic, assign) BOOL enablesReturnKeyAutomatically;

- (void)updateDrawableSize;
- (void)onViewportResizingStart:(CGSize)size;
- (void)onViewportResizingFinished:(CGSize)size;
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)secondaryTouchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event;
- (void)secondaryTouchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event;
- (void)secondaryTouchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event;
- (void)secondaryTouchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event;
- (void)showOnScreenKeyboard:(unsigned int)keyboardType;
- (void)hideOnScreenKeyboard;
#endif

@end
