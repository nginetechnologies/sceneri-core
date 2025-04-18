#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
#include <Renderer/Window/iOS/MetalView.h>
#include <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/ViewExtensions.h>
#include <Renderer/Window/iOS/Window.h>

@interface Window ()

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (BOOL)shouldShowTouchIndicators;
- (void)removeTouchIndicatorWithHash:(NSUInteger)hash;

/** Sets whether touches should always show regardless of whether the display is mirroring. Defaults to NO. */
@property(nonatomic, assign) BOOL alwaysShowTouches;
#endif

@end

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
@interface TouchIndicatorView : UIImageView

@property(nonatomic, assign, getter=isFadingOut) BOOL fadingOut;
@property(nonatomic, assign) float touchRadius;

@end

@implementation TouchIndicatorView
@end
#endif

@implementation Window

- (BOOL)canBecomeKeyWindow
{
	return true;
}
- (BOOL)canBecomeMainWindow
{
	return true;
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (UIImage*)makeTouchImage:(float)radius
{
	UIBezierPath* clipPath = [UIBezierPath bezierPathWithRect:CGRectMake(0, 0, radius * 2, radius * 2)];

	UIGraphicsBeginImageContextWithOptions(clipPath.bounds.size, NO, 0);

	UIBezierPath* drawPath = [UIBezierPath bezierPathWithArcCenter:CGPointMake(radius, radius)
																													radius:radius
																											startAngle:0
																												endAngle:2 * M_PI
																											 clockwise:YES];
	drawPath.lineWidth = 2.0;

	[[UIColor darkGrayColor] setStroke];
	[[UIColor lightGrayColor] setFill];

	[drawPath stroke];
	[drawPath fill];
	[clipPath addClip];

	UIImage* touchImage = UIGraphicsGetImageFromCurrentImageContext();
	UIGraphicsEndImageContext();
	return touchImage;
}

- (BOOL)shouldShowTouchIndicators;
{
	if (self.alwaysShowTouches)
	{
		return YES;
	}

#if !PLATFORM_APPLE_MACCATALYST && !PLATFORM_APPLE_VISIONOS
	for (UIScreen* screen in [UIScreen screens])
	{
		if ([screen isCaptured])
		{
			return YES;
		}

		if ([screen mirroredScreen] != nil)
		{
			return YES;
		}
	}
#endif

	return NO;
}

- (void)removeTouchIndicatorWithHash:(NSUInteger)hash;
{
	TouchIndicatorView* touchView = (TouchIndicatorView*)[self viewWithTag:hash];
	if (![touchView isKindOfClass:[TouchIndicatorView class]])
	{
		return;
	}

	if ([touchView isFadingOut])
	{
		return;
	}

	const float fadeDuration = 0.3f;

	BOOL animationsWereEnabled = [UIView areAnimationsEnabled];
	[UIView setAnimationsEnabled:YES];
	[UIView animateWithDuration:fadeDuration
									 animations:^{
									 }
									 completion:^([[maybe_unused]] BOOL finished){
									 }];

	touchView.frame = CGRectMake(
		touchView.center.x - touchView.frame.size.width,
		touchView.center.y - touchView.frame.size.height,
		touchView.frame.size.width * 2,
		touchView.frame.size.height * 2
	);

	touchView.alpha = 0.0;

	[UIView setAnimationsEnabled:animationsWereEnabled];

	touchView.fadingOut = YES;
	[touchView performSelector:@selector(removeFromSuperview) withObject:nil afterDelay:fadeDuration];
}

NSMutableSet<UITouch*>* secondaryTouches = [NSMutableSet new];

- (void)sendEvent:(UIEvent*)event
{
	// Workaround for iOS skipping secondary pointer events
	// Catch them early here and send directly to our view
	if (@available(macCatalyst 13.4, iOS 13.4, *))
	{
		if (event.type == UIEventTypeTouches)
		{

			UIViewController* vc = (UIViewController*)[self rootViewController];
			NSSet<UITouch*>* touches = [event allTouches];

			NSMutableSet<UITouch*>* began = [NSMutableSet new];
			NSMutableSet<UITouch*>* moved = [NSMutableSet new];
			NSMutableSet<UITouch*>* ended = [NSMutableSet new];
			NSMutableSet<UITouch*>* cancelled = [NSMutableSet new];

			for (UITouch* touch in touches)
			{
				if (event.buttonMask == UIEventButtonMaskSecondary && touch.phase == UITouchPhaseBegan)
				{
					[began addObject:touch];
					[secondaryTouches addObject:touch];
				}

				if (event.buttonMask == UIEventButtonMaskSecondary && touch.phase == UITouchPhaseMoved)
				{
					[moved addObject:touch];
				}

				if (touch.phase == UITouchPhaseEnded && [secondaryTouches containsObject:touch])
				{
					[ended addObject:touch];
					[secondaryTouches removeObject:touch];
				}

				if (touch.phase == UITouchPhaseCancelled && [secondaryTouches containsObject:touch])
				{
					[cancelled addObject:touch];
					[secondaryTouches removeObject:touch];
				}
			}

			MetalView* pView = [vc.view findAsType:[MetalView class]];
			if ([began count])
			{
				[pView secondaryTouchesBegan:began withEvent:event];
			}
			if ([moved count])
			{
				[pView secondaryTouchesMoved:moved withEvent:event];
			}
			if ([ended count])
			{
				[pView secondaryTouchesEnded:ended withEvent:event];
			}
			if ([cancelled count])
			{
				[pView secondaryTouchesCancelled:cancelled withEvent:event];
			}
		}
	}

	if (event.type == UIEventTypeTouches)
	{
		NSSet<UITouch*>* touches = [event allTouches];

		for (UITouch* touch in touches)
		{
			switch (touch.phase)
			{
				case UITouchPhaseBegan:
				case UITouchPhaseMoved:
				case UITouchPhaseStationary:
				{
					TouchIndicatorView* touchView = (TouchIndicatorView*)[self viewWithTag:touch.hash];

					if (touch.phase != UITouchPhaseStationary && touchView != nil && [touchView isFadingOut])
					{
						[touchView removeFromSuperview];
						touchView = nil;
					}

					if ([self shouldShowTouchIndicators])
					{
						const float touchRadius = (float)touch.majorRadius;

						if (touchView == nil && touch.phase != UITouchPhaseStationary)
						{
							UIImage* image = [self makeTouchImage:touchRadius];
							touchView = [[TouchIndicatorView alloc] initWithImage:image];
							touchView.touchRadius = touchRadius;
							[self addSubview:touchView];
						}

						if (![touchView isFadingOut])
						{
							touchView.alpha = 0.4;
							touchView.center = [touch locationInView:self];
							touchView.tag = touch.hash;

							if (touchView.touchRadius != touchRadius)
							{
								touchView.touchRadius = touchRadius;
								[touchView setImage:[self makeTouchImage:touchRadius]];
							}
						}
					}
				}
				break;
				case UITouchPhaseEnded:
				case UITouchPhaseCancelled:
					[self removeTouchIndicatorWithHash:touch.hash];
					break;
				default:
					break;
			}
		}
	}

	[super sendEvent:event];
}
#endif

@end
#endif
