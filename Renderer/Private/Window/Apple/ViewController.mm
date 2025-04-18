#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
#include <Engine/Engine.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Window/iOS/MetalView.h>
#include <Renderer/Window/iOS/ViewController.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Common/System/Query.h>

@interface ViewController ()
@end

@implementation ViewController

- (void)viewDidLoad
{
	[super viewDidLoad];
	NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
	[center addObserver:self
						 selector:@selector(handleSignInWithAppleStateChanged:)
								 name:ASAuthorizationAppleIDProviderCredentialRevokedNotification
							 object:nil];

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	self.modalPresentationStyle = UIModalPresentationFullScreen;

	UIKeyCommand* cursorLeft = [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:0 action:@selector(commandActionProxy:)];
	UIKeyCommand* cursorRight = [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:0 action:@selector(commandActionProxy:)];
	[self addKeyCommand:cursorLeft];
	[self addKeyCommand:cursorRight];
#elif PLATFORM_APPLE_MACOS
	[[NSNotificationCenter defaultCenter] addObserver:self
																					 selector:@selector(windowDidResize:)
																							 name:NSWindowDidResizeNotification
																						 object:nil];
#endif
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];
	[[NSNotificationCenter defaultCenter] addObserver:self
																					 selector:@selector(keyboardWillShow:)
																							 name:UIKeyboardWillShowNotification
																						 object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
																					 selector:@selector(keyboardWillHide:)
																							 name:UIKeyboardWillHideNotification
																						 object:nil];
}
- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
	[[NSNotificationCenter defaultCenter] removeObserver:self name:UIKeyboardWillShowNotification object:nil];
	[[NSNotificationCenter defaultCenter] removeObserver:self name:UIKeyboardWillHideNotification object:nil];
}
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)keyboardWillShow:(NSNotification*)notification
{
	NSDictionary* info = [notification userInfo];
	NSValue* value = [info objectForKey:UIKeyboardFrameEndUserInfoKey];
	CGRect keyboardRect = [value CGRectValue];
	CGSize keyboardSize = keyboardRect.size;
	// CGSize keyboardSize = [[[notification userInfo] objectForKey:UIKeyboardFrameBeginUserInfoKey] CGRectValue].size;

	[UIView animateWithDuration:0.3
									 animations:^{
										 CGRect f = self.view.frame;
										 f.origin.y = -keyboardSize.height;
										 self.view.frame = f;
									 }];
}

- (void)keyboardWillHide:(NSNotification*)notification
{
	[UIView animateWithDuration:0.3
									 animations:^{
										 CGRect f = self.view.frame;
										 f.origin.y = 0.0f;
										 self.view.frame = f;
									 }];
}

- (void)commandActionProxy:(UIKeyCommand*)command
{
}

- (BOOL)shouldAutorotate
{
	MetalView* metalView = (MetalView*)self.view;
	if (metalView != nil)
	{
		using namespace ngine;
		Rendering::Window* pWindow = (Rendering::Window*)metalView.engineWindow;
		if (pWindow != nullptr)
		{
			return pWindow->AllowScreenAutoRotation();
		}
	}
	return YES;
}

- (NSUInteger)supportedInterfaceOrientations
{
	MetalView* metalView = (MetalView*)self.view;
	if (metalView != nil)
	{
		using namespace ngine;
		Rendering::Window* pWindow = (Rendering::Window*)metalView.engineWindow;
		if (pWindow != nullptr)
		{
			using OrientationFlags = Rendering::Window::OrientationFlags;
			const EnumFlags<OrientationFlags> disallowedOrientations = pWindow->GetDisallowedScreenOrientations();
			const EnumFlags<OrientationFlags> allowedOrientations = ~disallowedOrientations;

			UIInterfaceOrientationMask allowedOrientationMask{0};
			allowedOrientationMask |= UIInterfaceOrientationMaskPortrait * allowedOrientations.IsSet(OrientationFlags::Portrait);
			allowedOrientationMask |= UIInterfaceOrientationMaskPortraitUpsideDown *
			                          allowedOrientations.IsSet(OrientationFlags::PortraitUpsideDown);
			allowedOrientationMask |= UIInterfaceOrientationMaskLandscapeLeft * allowedOrientations.IsSet(OrientationFlags::LandscapeLeft);
			allowedOrientationMask |= UIInterfaceOrientationMaskLandscapeRight * allowedOrientations.IsSet(OrientationFlags::LandscapeRight);
			return allowedOrientationMask;
		}
	}

	return UIInterfaceOrientationMaskAll;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation
{
	MetalView* metalView = (MetalView*)self.view;
	if (metalView != nil)
	{
		using namespace ngine;
		Rendering::Window* pWindow = (Rendering::Window*)metalView.engineWindow;
		if (pWindow != nullptr)
		{
			using OrientationFlags = Rendering::Window::OrientationFlags;
			const EnumFlags<OrientationFlags> disallowedOrientations = pWindow->GetDisallowedScreenOrientations();
			const EnumFlags<OrientationFlags> allowedOrientations = ~disallowedOrientations;

			const OrientationFlags orientation = *allowedOrientations.GetFirstSetFlag();
			switch (orientation)
			{
				case OrientationFlags::Portrait:
					return UIInterfaceOrientationPortrait;
				case OrientationFlags::PortraitUpsideDown:
					return UIInterfaceOrientationPortraitUpsideDown;
				case OrientationFlags::LandscapeRight:
					return UIInterfaceOrientationLandscapeRight;
				case OrientationFlags::LandscapeLeft:
					return UIInterfaceOrientationLandscapeLeft;
				default:
					return UIInterfaceOrientationPortrait;
			}
		}
	}

	return UIInterfaceOrientationUnknown;
}
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
	[super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

	if ([UIApplication sharedApplication].applicationState != UIApplicationStateBackground)
	{
		using namespace ngine;
		MetalView* metalView = (MetalView*)self.view;

		ngine::Rendering::Window* pWindow = metalView.engineWindow != nil ? (ngine::Rendering::Window*)metalView.engineWindow : nullptr;

		const float scaleFactor = pWindow != nullptr ? pWindow->GetPhysicalDevicePixelRatio() :
#if PLATFORM_APPLE_VISIONOS
		                                             1.5f;
#else
		                                             (float)[[metalView window] screen].nativeScale;
#endif
		size.width *= scaleFactor;
		size.height *= scaleFactor;

		[metalView onViewportResizingStart:size];
	}
}
#else
- (void)windowDidResize:(NSNotification*)notification
{
	NSSize newSize = self.view.window.frame.size;

	using namespace ngine;
	MetalView* metalView = (MetalView*)self.view;

	ngine::Rendering::Window* pWindow = metalView.engineWindow != nil ? (ngine::Rendering::Window*)metalView.engineWindow : nullptr;

	const float scaleFactor = pWindow != nullptr ? pWindow->GetPhysicalDevicePixelRatio()
	                                             : 1.f; //(float)[[metalView window] screen].userSpaceScaleFactor;
	newSize.width *= scaleFactor;
	newSize.height *= scaleFactor;

	[metalView onViewportResizingStart:newSize];
}
#endif

- (void)handleSignInWithAppleStateChanged:(id)noti
{
	if (m_appleSignInCredentialsRevokedCallback != nil)
	{
		m_appleSignInCredentialsRevokedCallback();
	}
}

// A mechanism for generating requests to authenticate users based on their Apple ID.
ASAuthorizationAppleIDProvider* appleIDProvider = nil;

- (void)setAppleSignInCredentialsRevokedCallback:(AppleSignInCredentialsRevokedCallback)callback
{
	m_appleSignInCredentialsRevokedCallback = callback;
}

- (void)checkCachedAppleSignInState:(CheckCachedAppleSignInCallback)callback
										 userIdentifier:(NSString*)userIdentifier
											 refreshToken:(NSString*)refreshToken
{
	using namespace ngine;

	if (appleIDProvider == nil)
	{
		appleIDProvider = [ASAuthorizationAppleIDProvider new];
	}

	Assert(m_cachedCacheCheckCallback == nil);
	m_cachedCacheCheckCallback = callback;

	[appleIDProvider getCredentialStateForUserID:(NSString* _Nonnull)userIdentifier
																		completion:^(
																			ASAuthorizationAppleIDProviderCredentialState credentialState,
																			[[maybe_unused]] NSError* _Nullable error
																		) {
																			switch (credentialState)
																			{
																				case ASAuthorizationAppleIDProviderCredentialRevoked:
																					self->m_cachedCacheCheckCallback(AppleCachedSignInQueryResultRevoked);
																					break;
																				case ASAuthorizationAppleIDProviderCredentialAuthorized:
																					self->m_cachedCacheCheckCallback(AppleCachedSignInQueryResultAuthorized);
																					break;
																				case ASAuthorizationAppleIDProviderCredentialNotFound:
																					self->m_cachedCacheCheckCallback(AppleCachedSignInQueryResultNotFound);
																					break;
																				case ASAuthorizationAppleIDProviderCredentialTransferred:
																					self->m_cachedCacheCheckCallback(AppleCachedSignInQueryResultTransferred);
																					break;
																			}
																			self->m_cachedCacheCheckCallback = nil;
																		}];
}

- (void)startAppleSignIn:(AppleSignInCallback)callback
{
	using namespace ngine;

	if (appleIDProvider == nil)
	{
		appleIDProvider = [ASAuthorizationAppleIDProvider new];
	}

	// Creates a new Apple ID authorization request.
	ASAuthorizationAppleIDRequest* request = appleIDProvider.createRequest;
	// The contact information to be requested from the user during authentication.
	request.requestedScopes = @[ ASAuthorizationScopeEmail ];
	request.user = nil;
	{
		const FlatString<37> sessionGuid = System::Get<Engine>().GetSessionGuid().ToString();
		request.nonce = [[NSString alloc] initWithBytes:sessionGuid.GetData() length:sessionGuid.GetSize() encoding:NSUTF8StringEncoding];
	}
	request.state = @"Initial sign-in";

	// A controller that manages authorization requests created by a provider.
	ASAuthorizationController* controller = [[ASAuthorizationController alloc] initWithAuthorizationRequests:@[ request ]];

	// A delegate that the authorization controller informs about the success or failure of an authorization attempt.
	controller.delegate = self;

	// A delegate that provides a display context in which the system can present an authorization interface to the
	// user.
	controller.presentationContextProvider = self;

	m_cachedSignInCallback = callback;
	// starts the authorization flows named during controller initialization.
	[controller performRequests];
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)didReceiveMemoryWarning
{
	[super didReceiveMemoryWarning];
	// Dispose of any resources that can be recreated.
}
#endif

- (void)authorizationController:(ASAuthorizationController*)controller didCompleteWithAuthorization:(ASAuthorization*)authorization
{
	if ([authorization.credential isKindOfClass:[ASAuthorizationAppleIDCredential class]])
	{
		ASAuthorizationAppleIDCredential* appleIDCredential = authorization.credential;
		NSString* userIdentifier = appleIDCredential.user;

		if ([appleIDCredential.state compare:@"Initial sign-in" options:NSLiteralSearch] == NSOrderedSame)
		{
			const void* authorizationCodeBytes = [appleIDCredential.authorizationCode bytes];
			NSString* authorizationCode = [[NSString alloc] initWithBytes:authorizationCodeBytes
																														 length:[appleIDCredential.authorizationCode length]
																													 encoding:NSUTF8StringEncoding];
			if (m_cachedSignInCallback != nil)
			{
				m_cachedSignInCallback(AppleSignInResultLoggedIn, userIdentifier, authorizationCode);
			}
		}
		else if (m_cachedSignInCallback != nil)
		{
			m_cachedSignInCallback(AppleSignInResultFailure, nil, nil);
		}
	}
	else if ([authorization.credential isKindOfClass:[ASPasswordCredential class]])
	{
		/*ASPasswordCredential *passwordCredential = authorization.credential;
		NSString *user = passwordCredential.user;
		NSString *password = passwordCredential.password;*/
	}
}

- (void)authorizationController:(ASAuthorizationController*)controller didCompleteWithError:(NSError*)error
{
	if (m_cachedSignInCallback != nil)
	{
		m_cachedSignInCallback(AppleSignInResultFailure, nil, nil);
	}
}

//! Tells the delegate from which window it should present content to the user.
- (ASPresentationAnchor)presentationAnchorForAuthorizationController:(ASAuthorizationController*)controller
{
	return (ASPresentationAnchor)self.view.window;
}

- (void)paymentAuthorizationViewController:(PKPaymentAuthorizationViewController*)controller
											 didAuthorizePayment:(PKPayment*)payment
																completion:(void (^)(PKPaymentAuthorizationStatus status))completion
{
	// do an async call to the server to complete the payment.
	// See PKPayment class reference for object parameters that can be passed
	NSString* paymentData = [[NSString alloc] initWithData:payment.token.paymentData encoding:NSUTF8StringEncoding];
	m_pkPaymentCompletionCallback = completion;
	m_applePayCallback(paymentData, m_applePayCurrentAmount);

	// When the async call is done, send the callback.
	// Available cases are:
	//    PKPaymentAuthorizationStatusSuccess, // Merchant auth'd (or expects to auth) the transaction successfully.
	//    PKPaymentAuthorizationStatusFailure, // Merchant failed to auth the transaction.
	//
	//    PKPaymentAuthorizationStatusInvalidBillingPostalAddress,  // Merchant refuses service to this billing address.
	//    PKPaymentAuthorizationStatusInvalidShippingPostalAddress, // Merchant refuses service to this shipping address.
	//    PKPaymentAuthorizationStatusInvalidShippingContact        // Supplied contact information is insufficient.
}

- (void)paymentAuthorizationViewControllerDidFinish:(PKPaymentAuthorizationViewController*)controller
{
	// dismiss the payment window
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	[controller dismissViewControllerAnimated:TRUE completion:nil];
#else
	[controller dismissController:nil];
#endif
}

- (void)callPkPaymentAuthorizationCompletionCallback:(PKPaymentAuthorizationStatus)status
{
	m_pkPaymentCompletionCallback(status);
}

// clang-format off
- (void)checkOut:(ApplePayCallback)callback :(NSUInteger)amount
// clang-format on
{
	if ([PKPaymentAuthorizationViewController canMakePayments])
	{
		PKPaymentRequest* request = [[PKPaymentRequest alloc] init];

		NSString* amountStr = [NSString stringWithFormat:@"%i", (int)amount];
		NSString* amountStrDesc = [NSString stringWithFormat:@"%i Sceneros", (int)amount];
		double price = amount * 0.01;
		NSString* priceStr = [NSString stringWithFormat:@"%f", price];
		PKPaymentSummaryItem* amountString = [PKPaymentSummaryItem summaryItemWithLabel:amountStrDesc
																																						 amount:[NSDecimalNumber decimalNumberWithString:amountStr]];
		PKPaymentSummaryItem* tax = [PKPaymentSummaryItem summaryItemWithLabel:@"tax" amount:[NSDecimalNumber decimalNumberWithString:@"0.00"]];
		PKPaymentSummaryItem* total = [PKPaymentSummaryItem summaryItemWithLabel:@"Total"
																																			amount:[NSDecimalNumber decimalNumberWithString:priceStr]];
		request.paymentSummaryItems = @[ amountString, tax, total ];
		request.countryCode = @"US";
		request.currencyCode = @"USD";
		request.supportedNetworks = @[ PKPaymentNetworkAmex, PKPaymentNetworkMasterCard, PKPaymentNetworkVisa, PKPaymentNetworkDiscover ];
		request.merchantIdentifier = @"merchant.ngine.Editor";
		request.merchantCapabilities = PKMerchantCapability3DS;

		PKPaymentAuthorizationViewController* paymentPane = [[PKPaymentAuthorizationViewController alloc] initWithPaymentRequest:request];
		paymentPane.delegate = self;

		self->m_applePayCallback = callback;
		self->m_applePayCurrentAmount = amount;
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		[self presentViewController:paymentPane animated:TRUE completion:nil];
#else
		[self presentViewControllerAsModalWindow:paymentPane];
#endif
	}
	else
	{
		NSLog(@"This device doesn't support Pay");
	}
}

- (void)dealloc
{
#if !__has_feature(objc_arc)
	[super dealloc];
#endif
	[[NSNotificationCenter defaultCenter] removeObserver:self name:ASAuthorizationAppleIDProviderCredentialRevokedNotification object:nil];
}

@end
#endif
