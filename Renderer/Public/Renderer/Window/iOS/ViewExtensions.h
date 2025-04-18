#pragma once

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>

@interface UIView (Extension)
- (nullable __kindof UIView*)findAsType:(_Nonnull Class)type;
@end

@implementation UIView (Extension)
- (nullable __kindof UIView*)findAsType:(_Nonnull Class)type
{
	if ([self isKindOfClass:type])
	{
		return self;
	}
	else
	{
		for (UIView* view in self.subviews)
		{
			__kindof UIView* result = [view findAsType:type];
			if (result != nil)
			{
				return result;
			}
		}
	}
	return nil;
}
@end
#elif PLATFORM_APPLE_MACOS
@interface NSView (Extension)
- (nullable __kindof NSView*)findAsType:(_Nonnull Class)type;
@end

@implementation NSView (Extension)
- (nullable __kindof NSView*)findAsType:(_Nonnull Class)type
{
	if ([self isKindOfClass:type])
	{
		return self;
	}
	else
	{
		for (NSView* view in self.subviews)
		{
			__kindof NSView* result = [view findAsType:type];
			if (result != nil)
			{
				return result;
			}
		}
	}
	return nil;
}
@end
#endif
