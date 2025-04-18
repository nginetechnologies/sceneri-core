#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>

@interface WindowScene : UIWindowScene
@end

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate, UIDropInteractionDelegate>
{
}
@end
#endif
