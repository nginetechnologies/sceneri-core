#pragma once

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#endif

#import <PassKit/PassKit.h>
#import <AuthenticationServices/AuthenticationServices.h>

typedef NS_ENUM(NSInteger, AppleCachedSignInQueryResult) {
	AppleCachedSignInQueryResultRevoked,
	AppleCachedSignInQueryResultAuthorized,
	AppleCachedSignInQueryResultNotFound,
	AppleCachedSignInQueryResultTransferred,
};

typedef void (^CheckCachedAppleSignInCallback)(const AppleCachedSignInQueryResult result);

typedef NS_ENUM(NSInteger, AppleSignInResult) {
	AppleSignInResultLoggedIn,
	AppleSignInResultFailure,
};
typedef void (^AppleSignInCallback)(const AppleSignInResult result, NSString* userIdentifier, NSString* authorizationToken);

typedef void (^AppleSignInCredentialsRevokedCallback)();

typedef void (^ApplePayCompletionCallback)(bool success);

typedef void (^PKPaymentAuthorizationCompletionCallback)(PKPaymentAuthorizationStatus status);
typedef void (^ApplePayCallback)(NSString* applePayData, NSUInteger amount);

@interface ViewController :

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	UIViewController <
#elif PLATFORM_APPLE_MACOS
	NSViewController <
#endif
		ASAuthorizationControllerDelegate,
		ASAuthorizationControllerPresentationContextProviding,
		PKPaymentAuthorizationViewControllerDelegate>
{
@public
	NSString* m_cachedSignInWithAppleUser;
	ApplePayCallback m_applePayCallback;
	NSUInteger m_applePayCurrentAmount;
	AppleSignInCallback m_cachedSignInCallback;
	CheckCachedAppleSignInCallback m_cachedCacheCheckCallback;
	AppleSignInCredentialsRevokedCallback m_appleSignInCredentialsRevokedCallback;

	PKPaymentAuthorizationCompletionCallback m_pkPaymentCompletionCallback;
}
// clang-format off
- (void)checkOut:(ApplePayCallback)callback :(NSUInteger)amount;
// clang-format on
- (void)callPkPaymentAuthorizationCompletionCallback:(PKPaymentAuthorizationStatus)status;

- (void)checkCachedAppleSignInState:(CheckCachedAppleSignInCallback)callback
										 userIdentifier:(NSString*)userIdentifier
											 refreshToken:(NSString*)refreshToken;
- (void)setAppleSignInCredentialsRevokedCallback:(AppleSignInCredentialsRevokedCallback)callback;
- (void)startAppleSignIn:(AppleSignInCallback)callback;

@end
