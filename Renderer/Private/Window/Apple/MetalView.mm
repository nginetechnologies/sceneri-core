#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
#import <Renderer/Window/iOS/AppDelegate.h>
#include <Renderer/Window/iOS/ControllerManager.h>
#import <Renderer/Window/iOS/MetalView.h>
#import <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/ViewExtensions.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIGestureRecognizerSubclass.h>
#endif
#if PLATFORM_APPLE_MACOS
#import <Carbon/Carbon.h>
#endif

#include <Engine/Engine.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Entity/ComponentSoftReference.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Window/DragAndDropData.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/Math/Angle.h>
#include <Common/Math/Length.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/UnorderedSet.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/Log.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
@interface PanGestureRecognizer : UIPanGestureRecognizer
{
@public
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_fingerIdentifiers;
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_eventFingerIdentifiers;
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)reset;

@property(nonatomic, assign) CGFloat majorRadius;
@property(nonatomic, assign) CGPoint touchLocation;
@property(nonatomic, assign) bool allowWithVirtualController;

@end

@interface TapGestureRecognizer : UITapGestureRecognizer
{
@public
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_fingerIdentifiers;
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_eventFingerIdentifiers;
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;

@property(nonatomic, assign) CGFloat majorRadius;
@property(nonatomic, assign) CGPoint touchLocation;
@property(nonatomic, assign) bool allowWithVirtualController;
@end

@interface LongPressGestureRecognizer : UILongPressGestureRecognizer
{
@public
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_fingerIdentifiers;
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_eventFingerIdentifiers;
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)reset;

@property(nonatomic) CGFloat majorRadius;
@property(nonatomic, assign) CGPoint touchLocation;
@property(nonatomic) UIEventButtonMask buttonMaskRequired;
@property(nonatomic) bool allowWithVirtualController;

@end

@interface GestureDelegate : NSObject <UIGestureRecognizerDelegate>
@property(nonatomic, assign) bool hasVirtualController;
@end

@interface PinchGestureRecognizer : UIPinchGestureRecognizer
{
@public
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_fingerIdentifiers;
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_eventFingerIdentifiers;
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;

@end

@interface RotationGestureRecognizer : UIRotationGestureRecognizer
{
@public
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_fingerIdentifiers;
	ngine::Vector<ngine::Input::FingerIdentifier, ngine::uint8> m_eventFingerIdentifiers;
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
- (void)reset;

@end

@implementation GestureDelegate

- (BOOL)gestureRecognizerShouldEnable:(UIGestureRecognizer* _Nonnull)gestureRecognizer
{
	if (self.hasVirtualController)
	{
		if ([gestureRecognizer isKindOfClass:[PanGestureRecognizer class]])
		{
			return ((PanGestureRecognizer*)gestureRecognizer).allowWithVirtualController;
		}
	}

	return YES;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer* _Nonnull)gestureRecognizer
{
	return [self gestureRecognizerShouldEnable:gestureRecognizer];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer* _Nonnull)gestureRecognizer
	shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer* _Nonnull)otherGestureRecognizer
{
	/*if (([gestureRecognizer isKindOfClass:[PinchGestureRecognizer class]] || [gestureRecognizer
	isKindOfClass:[UIPanGestureRecognizer class]])
	     && ([otherGestureRecognizer isKindOfClass:[PinchGestureRecognizer class]] || [otherGestureRecognizer
	isKindOfClass:[UIPanGestureRecognizer class]]))  { return false;
	}*/

	const bool hasLongPressRecognizer = [gestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]] ||
	                                    [otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]];
	const bool hasPanRecognizer = [gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] ||
	                              [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]];
	if (hasLongPressRecognizer & hasPanRecognizer)
	{
		return false;
	}
	const bool hasPinchRecognizer = [gestureRecognizer isKindOfClass:[UIPinchGestureRecognizer class]] ||
	                                [otherGestureRecognizer isKindOfClass:[UIPinchGestureRecognizer class]];
	if (hasLongPressRecognizer & hasPinchRecognizer)
	{
		return false;
	}

	return YES;
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer* _Nonnull)gestureRecognizer shouldReceiveTouch:(UITouch* _Nonnull)touch
{
	return [self gestureRecognizerShouldEnable:gestureRecognizer];
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer* _Nonnull)gestureRecognizer shouldReceivePress:(UIPress* _Nonnull)press
{
	return [self gestureRecognizerShouldEnable:gestureRecognizer];
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer* _Nonnull)gestureRecognizer shouldReceiveEvent:(UIEvent* _Nonnull)event
{
	return [self gestureRecognizerShouldEnable:gestureRecognizer];
}

@end

@implementation TapGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	{
		UITouch* touch = [touches anyObject];
		if (touch.type != UITouchTypeIndirectPointer)
		{
			self.majorRadius = [touch majorRadius];
		}
		else
		{
			self.majorRadius = 1;
		}
	}

	self.touchLocation = [self locationInView:self.view];

	for (UITouch* touch in touches)
	{
		m_fingerIdentifiers.EmplaceBack([touch hash]);
	}
	m_eventFingerIdentifiers = m_fingerIdentifiers;

	[super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	{
		UITouch* touch = [touches anyObject];
		if (touch.type != UITouchTypeIndirectPointer)
		{
			self.majorRadius = [touch majorRadius];
		}
		else
		{
			self.majorRadius = 1;
		}
	}

	[super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesEnded:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesCancelled:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)reset
{
	m_fingerIdentifiers.Clear();
	m_eventFingerIdentifiers.Clear();
}

- (CGPoint)locationInView:(nullable UIView*)view
{
	return [super locationInView:view];
}

@end

@implementation LongPressGestureRecognizer

- (instancetype _Nonnull)initWithTarget:(nullable id)target action:(nullable SEL)action;
{
	self.buttonMaskRequired = UIEventButtonMask{};
	return [super initWithTarget:target action:action];
}

UITouch* _Nullable firstTouch = nullptr;

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	{
		UITouch* touch = [touches anyObject];
		firstTouch = touch;
		self.majorRadius = [touch majorRadius];
		self.touchLocation = [self locationInView:self.view];
	}

	if ((event.buttonMask & [self buttonMaskRequired]) != [self buttonMaskRequired])
	{
		self.state = UIGestureRecognizerStateFailed;
		return;
	}

	bool containsTouchType = false;
	for (UITouch* pTouch : touches)
	{
		for (const NSNumber* allowedTouchType in [self allowedTouchTypes])
		{
			if ((ngine::uint32)pTouch.type == allowedTouchType.unsignedIntValue)
			{
				containsTouchType = true;
			}
		}
	}
	if (!containsTouchType)
	{
		self.state = UIGestureRecognizerStateFailed;
		return;
	}

	for (UITouch* touch in touches)
	{
		m_fingerIdentifiers.EmplaceBack([touch hash]);
	}
	m_eventFingerIdentifiers = m_fingerIdentifiers;

	if (event.buttonMask & UIEventButtonMaskSecondary)
	{
		self.state = UIGestureRecognizerStateBegan;
	}
	else
	{
		[super touchesBegan:touches withEvent:event];
	}
}

- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	UITouch* touch = [touches anyObject];
	self.majorRadius = [touch majorRadius];
	self.touchLocation = [self locationInView:self.view];

	if (self.state != UIGestureRecognizerStatePossible)
	{
		self.state = UIGestureRecognizerStateChanged;
		[super touchesMoved:touches withEvent:event];
		self.state = UIGestureRecognizerStateChanged;
	}
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesEnded:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesCancelled:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)reset
{
	m_fingerIdentifiers.Clear();
	m_eventFingerIdentifiers.Clear();
}

- (CGPoint)locationInView:(nullable UIView*)view
{
	return [firstTouch locationInView:view];
}

@end

@implementation PanGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	using namespace ngine;

	bool containsTouchType = false;
	for (UITouch* pTouch : touches)
	{
		for (const NSNumber* allowedTouchType in [self allowedTouchTypes])
		{
			if ((uint32)pTouch.type == allowedTouchType.unsignedIntValue)
			{
				containsTouchType = true;
			}
		}
	}
	if (!containsTouchType)
	{
		self.state = UIGestureRecognizerStateFailed;
		return;
	}

	{
		UITouch* touch = [touches anyObject];
		self.majorRadius = [touch majorRadius];
		self.touchLocation = [self locationInView:self.view];
	}

	for (UITouch* touch in touches)
	{
		m_fingerIdentifiers.EmplaceBack([touch hash]);
	}
	m_eventFingerIdentifiers = m_fingerIdentifiers;

	[super touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	UITouch* touch = [touches anyObject];
	self.majorRadius = [touch majorRadius];
	self.touchLocation = [self locationInView:self.view];

	[super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesEnded:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesCancelled:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)reset
{
	m_fingerIdentifiers.Clear();
	m_eventFingerIdentifiers.Clear();
}

@end

@implementation PinchGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	using namespace ngine;

	for (UITouch* touch in touches)
	{
		m_fingerIdentifiers.EmplaceBack([touch hash]);
	}
	m_eventFingerIdentifiers = m_fingerIdentifiers;

	[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesEnded:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesCancelled:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)reset
{
	m_fingerIdentifiers.Clear();
	m_eventFingerIdentifiers.Clear();
}

@end

@implementation RotationGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	using namespace ngine;

	for (UITouch* touch in touches)
	{
		m_fingerIdentifiers.EmplaceBack([touch hash]);
	}
	m_eventFingerIdentifiers = m_fingerIdentifiers;

	[super touchesBegan:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesEnded:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event;
{
	[super touchesCancelled:touches withEvent:event];

	for (UITouch* touch in touches)
	{
		[[maybe_unused]] const bool wasRemoved = m_fingerIdentifiers.RemoveFirstOccurrence([touch hash]);
		Assert(wasRemoved);
	}
}

- (void)reset
{
	m_fingerIdentifiers.Clear();
	m_eventFingerIdentifiers.Clear();
}

@end
#endif

@implementation MetalView

+ (Class _Nonnull)layerClass
{
	return [CAMetalLayer class];
}
- (CALayer* _Nonnull)makeBackingLayer
{
	return [CAMetalLayer layer];
}

- (instancetype _Nonnull)init
{
	if ((self = [super init]))
	{
		[self initialize];
	}

	return self;
}

- (ngine::ControllerManager&)getControllerManager
{
	return *(ngine::ControllerManager*)self->_controllerManager;
}

- (instancetype _Nullable)initWithCoder:(NSCoder* _Nonnull)coder
{
	if ((self = [super initWithCoder:coder]))
	{
		[self initialize];
	}
	return self;
}

- (void)dealloc
{
	delete (ngine::ControllerManager*)self->_controllerManager;

	//[super dealloc];
}

@synthesize engineWindow = _engineWindow;

- (void)setEngineWindow:(void* _Nonnull)engineWindow
{
	_engineWindow = engineWindow;

	using namespace ngine;
	[self getControllerManager].Initialize(System::Get<Engine>(), (Rendering::Window*)engineWindow);
#if USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
	[self getControllerManager].SetGestureDelegate(m_pGestureDelegate);
#endif

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::GamepadDeviceType& gamepadDeviceType =
		inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
	Threading::Job& stage = Threading::CreateCallback(
		[&controllerManager = [self getControllerManager],
	   &inputManager,
	   &gamepadDeviceType,
	   &window = *(Rendering::Window*)engineWindow](Threading::JobRunnerThread&)
		{
			for (GCController* pController : controllerManager.GetControllers())
			{
				const Input::DeviceIdentifier gamepadIdentifier =
					gamepadDeviceType.GetOrRegisterInstance(uintptr(pController), inputManager, &window);
				for (id elementIdentifier in pController.physicalInputProfile.elements)
				{
					if (Optional<Input::GamepadMapping> gamepadMapping = GetGamepadMappingFromSource((__bridge void*)elementIdentifier))
					{
						const Input::GamepadMapping& mapping = *gamepadMapping;
						if (mapping.m_type == Input::GamepadMapping::Type::Button)
						{
							if (pController.physicalInputProfile.buttons[elementIdentifier].isPressed)
							{
								gamepadDeviceType.OnButtonDown(gamepadIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
							}
							else
							{
								gamepadDeviceType.OnButtonUp(gamepadIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
							}
						}
						else if (mapping.m_type == Input::GamepadMapping::Type::AnalogInput)
						{
							const float value = pController.physicalInputProfile.buttons[elementIdentifier].value;

							gamepadDeviceType
								.OnAnalogInput(gamepadIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Analog>(), value);
						}
						else if (mapping.m_type == Input::GamepadMapping::Type::Axis)
						{
							GCControllerDirectionPad* dpad = pController.physicalInputProfile.dpads[elementIdentifier];
							const Math::Vector2f axisValues{dpad.xAxis.value, dpad.yAxis.value};

							gamepadDeviceType
								.OnAxisInput(gamepadIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Axis>(), axisValues);
						}
						else if (mapping.m_type == Input::GamepadMapping::Type::DirectionalPad)
						{
							GCControllerDirectionPad* dpad = pController.physicalInputProfile.dpads[elementIdentifier];
							if (dpad.up.isPressed)
							{
								gamepadDeviceType.OnButtonDown(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadUp);
							}
							else
							{
								gamepadDeviceType.OnButtonUp(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadUp);
							}

							if (dpad.down.isPressed)
							{
								gamepadDeviceType.OnButtonDown(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadDown);
							}
							else
							{
								gamepadDeviceType.OnButtonUp(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadDown);
							}

							if (dpad.left.isPressed)
							{
								gamepadDeviceType.OnButtonDown(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadLeft);
							}
							else
							{
								gamepadDeviceType.OnButtonUp(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadLeft);
							}

							if (dpad.right.isPressed)
							{
								gamepadDeviceType.OnButtonDown(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadRight);
							}
							else
							{
								gamepadDeviceType.OnButtonUp(gamepadIdentifier, Input::GamepadInput::Button::DirectionPadRight);
							}
						}
					}
				}
			}
			return Threading::CallbackResult::Finished;
		},
		Threading::JobPriority::UserInputPolling,
		"Apple Gamepad Polling"
	);

	Engine& engine = System::Get<Engine>();
	engine.ModifyFrameGraph(
		[&stage]()
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			inputManager.GetPollForInputStage().AddSubsequentStage(stage);
			stage.AddSubsequentStage(inputManager.GetPolledForInputStage());
		}
	);
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)initialize
{
	using namespace ngine;

	ngine::ControllerManager* pControllerManager = new ngine::ControllerManager();
	self->_controllerManager = pControllerManager;

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	/* Resize properly when rotated. */
	self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleLeftMargin |
	                        UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin |
	                        UIViewAutoresizingFlexibleBottomMargin;
#elif PLATFORM_APPLE_MACOS
	/* Resize properly when rotated. */
	self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin |
	                        NSViewMaxYMargin;
#endif

	self.metalLayer = (CAMetalLayer*)self.layer;
	self.metalLayer.opaque = YES;

#if PLATFORM_APPLE_MACOS
	self.allowedTouchTypes = NSTouchTypeMaskIndirect | NSTouchTypeMaskDirect;

	[self registerForDraggedTypes:@[ NSPasteboardTypeURL, NSPasteboardTypeFileURL ]];
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	GestureDelegate* delegate = [[GestureDelegate alloc] init];
	m_pGestureDelegate = delegate;

	{
		UIContextMenuInteraction* contextMenuInteraction = [[UIContextMenuInteraction alloc] initWithDelegate:self];
		[self addInteraction:contextMenuInteraction];
	}

	{
		UIHoverGestureRecognizer* hover = [[UIHoverGestureRecognizer alloc] initWithTarget:self action:@selector(OnPointerHover:)];
		[self addGestureRecognizer:hover];
	}
#endif

#if PLATFORM_APPLE_VISIONOS
	self.userInteractionEnabled = true;

	{
		LongPressGestureRecognizer* longPress = [[LongPressGestureRecognizer alloc] initWithTarget:self action:@selector(OnLongPress:)];
		longPress.delaysTouchesBegan = true;
		longPress.delegate = delegate;
		// longPress.allowedTouchTypes = @[ @(UITouchTypeDirect), @(UITouchTypePencil) ];
		longPress.numberOfTouchesRequired = 1;
		longPress.cancelsTouchesInView = true;
		longPress.minimumPressDuration = 0.4;
		[self addGestureRecognizer:longPress];

		TapGestureRecognizer* tap = [[TapGestureRecognizer alloc] initWithTarget:self action:@selector(OnTap:)];
		// tap.buttonMaskRequired = UIEventButtonMaskPrimary;
		tap.delegate = delegate;
		// tap.allowedTouchTypes = @[ @(UITouchTypeDirect), @(UITouchTypePencil) ];
		tap.cancelsTouchesInView = false;
		[self addGestureRecognizer:tap];
		[tap requireGestureRecognizerToFail:longPress];
	}

	{
		TapGestureRecognizer* doubleTap = [[TapGestureRecognizer alloc] initWithTarget:self action:@selector(OnDoubleTap:)];
		doubleTap.delaysTouchesBegan = true;
		doubleTap.numberOfTapsRequired = 2;
		// doubleTap.buttonMaskRequired = UIEventButtonMaskPrimary;
		// doubleTap.allowedTouchTypes = @[ @(UITouchTypeDirect) ];
		doubleTap.delegate = delegate;
		doubleTap.cancelsTouchesInView = false;
		[self addGestureRecognizer:doubleTap];
	}

	{
		PanGestureRecognizer* onPan = [[PanGestureRecognizer alloc] initWithTarget:self action:@selector(OnPan:)];
		onPan.delaysTouchesBegan = false;
		onPan.delegate = delegate;
		// onPan.allowedTouchTypes = @[ @(UITouchTypeDirect), @(UITouchTypePencil) ];
		// onPan.allowedScrollTypesMask = UIScrollTypeMaskContinuous;
		onPan.cancelsTouchesInView = false;
		onPan.allowWithVirtualController = false;
		[self addGestureRecognizer:onPan];
	}
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	{
		PanGestureRecognizer* onTrackpadScroll = [[PanGestureRecognizer alloc] initWithTarget:self action:@selector(OnPointerScroll:)];
		onTrackpadScroll.delaysTouchesBegan = true;
		onTrackpadScroll.delegate = delegate;
		onTrackpadScroll.allowedTouchTypes = @[ @(UITouchTypeIndirect) ];
		onTrackpadScroll.allowedScrollTypesMask = UIScrollTypeMaskContinuous;
		onTrackpadScroll.maximumNumberOfTouches = 0;
		onTrackpadScroll.cancelsTouchesInView = false;
		onTrackpadScroll.allowWithVirtualController = false;
		[self addGestureRecognizer:onTrackpadScroll];
	}

	{
		PanGestureRecognizer* onMouseScroll = [[PanGestureRecognizer alloc] initWithTarget:self action:@selector(OnPointerScroll:)];
		onMouseScroll.delaysTouchesBegan = true;
		onMouseScroll.delegate = delegate;
		onMouseScroll.allowedTouchTypes = @[ @(UITouchTypeIndirectPointer) ];
		onMouseScroll.allowedScrollTypesMask = UIScrollTypeMaskDiscrete;
		onMouseScroll.maximumNumberOfTouches = 0;
		onMouseScroll.cancelsTouchesInView = false;
		onMouseScroll.allowWithVirtualController = false;
		[self addGestureRecognizer:onMouseScroll];
	}

	{
		PinchGestureRecognizer* onPinch = [[PinchGestureRecognizer alloc] initWithTarget:self action:@selector(OnPinch:)];
#if PLATFORM_APPLE_VISIONOS
		onPinch.allowedTouchTypes = @[ @(UITouchTypeDirect), @(UITouchTypeIndirect), @(UITouchTypeIndirectPointer) ];
#else
		onPinch.allowedTouchTypes = @[ @(UITouchTypeIndirect), @(UITouchTypeIndirectPointer) ];
#endif
		onPinch.delegate = delegate;
		[self addGestureRecognizer:onPinch];
	}

	{
		RotationGestureRecognizer* onRotate = [[RotationGestureRecognizer alloc] initWithTarget:self action:@selector(OnRotate:)];
		onRotate.allowedTouchTypes = @[ @(UITouchTypeIndirect), @(UITouchTypeIndirectPointer) ];
		onRotate.delaysTouchesBegan = false;
		onRotate.delaysTouchesEnded = false;
		onRotate.delegate = delegate;
		onRotate.cancelsTouchesInView = false;
		[self addGestureRecognizer:onRotate];
	}
#endif

	NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
	[center addObserver:self selector:@selector(handleControllerDidConnectNotification:) name:GCControllerDidConnectNotification object:nil];
	[center addObserver:self
						 selector:@selector(handleControllerDidDisconnectNotification:)
								 name:GCControllerDidDisconnectNotification
							 object:nil];
	[center addObserver:self
						 selector:@selector(handleControllerDidBecomeCurrentNotification:)
								 name:GCControllerDidBecomeCurrentNotification
							 object:nil];
	[center addObserver:self
						 selector:@selector(handleControllerDidStopBeingCurrentNotification:)
								 name:GCControllerDidStopBeingCurrentNotification
							 object:nil];

#if PLATFORM_APPLE_MACOS
	[center addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
	[center addObserver:self selector:@selector(windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
#endif
}

- (void)handleControllerDidConnectNotification:(NSNotification* _Nonnull)notification
{
	GCController* pController = notification.object;

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	if ([self getControllerManager].IsVirtualController(pController))
	{
		Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
		window.SetDisallowedScreenOrientations(Rendering::Window::OrientationFlags::PortraitAny);
		m_pGestureDelegate.hasVirtualController = true;
	}
#endif

	[self getControllerManager].AddController(pController);

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	// Physical controller connected -> remove virtual one
	if (![self getControllerManager].IsVirtualController(pController))
	{
		[self getControllerManager].DestroyVirtualController();
	}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
	[self getControllerManager].DestroyVirtualController();
#endif
}

- (void)handleControllerDidDisconnectNotification:(NSNotification* _Nonnull)notification
{
	GCController* pController = notification.object;

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	const bool isVirtualController = [self getControllerManager].IsVirtualController(pController);
	if (isVirtualController)
	{
		m_pGestureDelegate.hasVirtualController = false;
	}
#endif

	[self getControllerManager].RemoveController(pController);

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	// Last physical controller removed -> create virtual one
	if (!isVirtualController && ![self getControllerManager].GetControllers().GetSize())
	{
		[self getControllerManager].CreateVirtualController();
	}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
	if ([self getControllerManager].GetControllers().GetSize() == 0)
	{
		[self getControllerManager].CreateVirtualController();
	}
#endif
}

- (void)handleControllerDidBecomeCurrentNotification:(NSNotification* _Nonnull)notification
{
	// GCController *controller = notification.object;
}
- (void)handleControllerDidStopBeingCurrentNotification:(NSNotification* _Nonnull)notification
{
	// GCController *controller = notification.object;
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (UIContextMenuConfiguration* _Nullable)contextMenuInteraction:(UIContextMenuInteraction* _Nonnull)interaction
																 configurationForMenuAtLocation:(CGPoint)location;
{
	return nil;
}

- (void)OnPointerHover:(UIHoverGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
		case UIGestureRecognizerStateChanged:
		{
			const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			Math::Vector2i delta;
			CGGetLastMouseDelta(&delta.x, &delta.y);

			mouseDeviceType.OnMotion(mouseIdentifier, coordinates, delta, *&window);
#else
			mouseDeviceType.OnMotion(mouseIdentifier, coordinates, *&window);
#endif
		}
		break;
		case UIGestureRecognizerStateEnded:
			break;
		case UIGestureRecognizerStateCancelled:
			break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnPointerScroll:(PanGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint delta = [sender translationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	delta.x *= scaleFactor;
	delta.y *= scaleFactor;
	[sender setTranslation:CGPointMake(0, 0) inView:self];

	CGPoint point = [sender locationInView:self];
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
		{
			mouseDeviceType.OnStartScroll(mouseIdentifier, coordinates, Math::Vector2i{(int)delta.x, (int)delta.y}, *&window);
		}
		break;
		case UIGestureRecognizerStateChanged:
			if (delta.x != 0 || delta.y != 0)
			{
				mouseDeviceType.OnScroll(mouseIdentifier, coordinates, Math::Vector2i{(int)delta.x, (int)delta.y}, *&window);
			}
			break;
		case UIGestureRecognizerStateEnded:
		{
			CGPoint velocityPoint = [sender velocityInView:self];
			velocityPoint.x *= scaleFactor;
			velocityPoint.y *= scaleFactor;
			const Math::Vector2f velocity = {(float)velocityPoint.x, (float)velocityPoint.y};
			mouseDeviceType.OnEndScroll(mouseIdentifier, coordinates, velocity, *&window);
		}
		break;
		case UIGestureRecognizerStateCancelled:
		{
			mouseDeviceType.OnCancelScroll(mouseIdentifier, coordinates, *&window);
		}
		break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnTap:(UITapGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	TapGestureRecognizer* recognizer = (TapGestureRecognizer*)sender;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
	const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
	Assert(recognizer->m_eventFingerIdentifiers.GetSize() == (uint8)[sender numberOfTouchesRequired]);

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
			break;
		/*case UIGestureRecognizerStateBegan:
		touchscreenDeviceType.OnStartTap(touchscreenIdentifier, coordinates, radius,
		&window); break;*/
		case UIGestureRecognizerStateEnded:
		{
			touchscreenDeviceType.OnStartTap(touchscreenIdentifier, coordinates, radius, recognizer->m_eventFingerIdentifiers, *&window);
			touchscreenDeviceType.OnStopTap(touchscreenIdentifier, coordinates, radius, recognizer->m_eventFingerIdentifiers, *&window);
		}
		break;
		case UIGestureRecognizerStateCancelled:
		{
			touchscreenDeviceType.OnCancelTap(touchscreenIdentifier, recognizer->m_eventFingerIdentifiers, window);
		}
		break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateChanged:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnDoubleTap:(UITapGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	TapGestureRecognizer* recognizer = (TapGestureRecognizer*)sender;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
	const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
	Assert(recognizer->m_eventFingerIdentifiers.GetSize() == (uint8)[sender numberOfTouchesRequired]);

	touchscreenDeviceType.OnDoubleTap(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, radius, *&window);
}

- (void)OnLongPress:(UILongPressGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	LongPressGestureRecognizer* recognizer = (LongPressGestureRecognizer*)sender;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
		{
			const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
			touchscreenDeviceType.OnStartLongPress(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, radius, *&window);
		}
		break;
		case UIGestureRecognizerStateEnded:
		{
			const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
			touchscreenDeviceType.OnStopLongPress(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, radius, *&window);
		}
		break;
		case UIGestureRecognizerStateCancelled:
			touchscreenDeviceType.OnCancelLongPress(touchscreenIdentifier, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateChanged:
		{
			const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
			touchscreenDeviceType.OnLongPressMotion(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, radius, *&window);
		}
		break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnPan:(UIPanGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	CGPoint velocityPoint = [sender velocityInView:self];
	velocityPoint.x *= scaleFactor;
	velocityPoint.y *= scaleFactor;

	const Math::Vector2f velocity = {(float)velocityPoint.x, (float)velocityPoint.y};

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

	PanGestureRecognizer* recognizer = (PanGestureRecognizer*)sender;
	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
		{
			const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
			Assert(recognizer->m_eventFingerIdentifiers.HasElements());
			touchscreenDeviceType.OnStartPan(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, velocity, radius, window);
		}
		break;
		case UIGestureRecognizerStateEnded:
			touchscreenDeviceType.OnStopPan(touchscreenIdentifier, coordinates, velocity, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateCancelled:
			touchscreenDeviceType.OnCancelPan(touchscreenIdentifier, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateChanged:
		{
			if (recognizer->m_eventFingerIdentifiers.HasElements())
			{
				const uint16 radius = (uint16)Math::Ceil(recognizer.majorRadius * scaleFactor);
				touchscreenDeviceType
					.OnPanMotion(touchscreenIdentifier, coordinates, velocity, radius, recognizer->m_eventFingerIdentifiers, *&window);
			}
		}
		break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnPinch:(UIPinchGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	const float scale = (float)sender.scale;

	PinchGestureRecognizer* recognizer = (PinchGestureRecognizer*)sender;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
			touchscreenDeviceType.OnStartPinch(touchscreenIdentifier, coordinates, scale, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateChanged:
			touchscreenDeviceType.OnPinchMotion(touchscreenIdentifier, coordinates, scale, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateEnded:
			touchscreenDeviceType.OnStopPinch(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateCancelled:
			touchscreenDeviceType.OnCancelPinch(touchscreenIdentifier, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)OnRotate:(UIRotationGestureRecognizer* _Nonnull)sender
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	CGPoint point = [sender locationInView:self];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	RotationGestureRecognizer* recognizer = (RotationGestureRecognizer*)sender;

	const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

	switch (sender.state)
	{
		case UIGestureRecognizerStateBegan:
			touchscreenDeviceType.OnStartRotate(touchscreenIdentifier, coordinates, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStateChanged:
		{
			const float rotation = (float)sender.rotation;
			const float velocity = (float)sender.velocity;
			touchscreenDeviceType.OnRotateMotion(
				touchscreenIdentifier,
				coordinates,
				Math::Anglef::FromRadians(rotation),
				Math::RotationalSpeedf::FromRadiansPerSecond(velocity),
				recognizer->m_eventFingerIdentifiers,
				window
			);
		}
		break;
		case UIGestureRecognizerStateEnded:
		{
			const float rotation = (float)sender.rotation;
			const float velocity = (float)sender.velocity;
			touchscreenDeviceType.OnStopRotate(
				touchscreenIdentifier,
				coordinates,
				Math::Anglef::FromRadians(rotation),
				Math::RotationalSpeedf::FromRadiansPerSecond(velocity),
				recognizer->m_eventFingerIdentifiers,
				*&window
			);
		}
		break;
		case UIGestureRecognizerStateCancelled:
			touchscreenDeviceType.OnCancelRotate(touchscreenIdentifier, recognizer->m_eventFingerIdentifiers, *&window);
			break;
		case UIGestureRecognizerStatePossible:
			break;
		case UIGestureRecognizerStateFailed:
			break;
	}

	[sender setRotation:0.f];
}
#elif PLATFORM_APPLE_MACOS
- (void)mouseDown:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	mouseDeviceType.OnPress(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Input::MouseButton::Left, *&window);
}

- (void)mouseUp:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	mouseDeviceType.OnRelease(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Input::MouseButton::Left, &window);
}

- (void)rightMouseDown:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	mouseDeviceType.OnPress(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Input::MouseButton::Right, *&window);
}

- (void)rightMouseUp:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	mouseDeviceType.OnRelease(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Input::MouseButton::Right, &window);
}

- (void)otherMouseDown:(NSEvent* _Nonnull)event
{
}

- (void)otherMouseUp:(NSEvent* _Nonnull)event
{
}

- (void)initTrackingArea
{
	NSTrackingAreaOptions options =
		(NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
	   NSTrackingEnabledDuringMouseDrag);

	NSTrackingArea* area = [[NSTrackingArea alloc] initWithRect:[self bounds] options:options owner:self userInfo:nil];

	[self addTrackingArea:area];
}

- (void)updateTrackingAreas
{
	[self initTrackingArea];
}

- (void)mouseMoved:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;
	const Math::Vector2i delta = (Math::Vector2i)(Math::Vector2f{(float)[event deltaX], (float)[event deltaY]} * scaleFactor);

	mouseDeviceType.OnMotion(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, delta, *&window);
}

- (void)mouseDragged:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;
	const Math::Vector2i delta = (Math::Vector2i)(Math::Vector2f{(float)[event deltaX], (float)[event deltaY]} * scaleFactor);

	mouseDeviceType.OnMotion(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, delta, window);
}

- (void)otherMouseDragged:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;
	const Math::Vector2i delta = (Math::Vector2i)(Math::Vector2f{(float)[event deltaX], (float)[event deltaY]} * scaleFactor);

	mouseDeviceType.OnMotion(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, delta, window);
}

- (void)rightMouseDragged:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;
	const Math::Vector2i delta = (Math::Vector2i)(Math::Vector2f{(float)[event deltaX], (float)[event deltaY]} * scaleFactor);

	mouseDeviceType.OnMotion(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, delta, window);
}

- (void)scrollWheel:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
	point.y = self.frame.size.height - point.y;
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	NSPoint scroll{[event scrollingDeltaX], [event scrollingDeltaY]};
	scroll.x *= scaleFactor;
	scroll.y *= scaleFactor;

	switch (event.momentumPhase)
	{
		case NSEventPhaseNone:
		{
			if (event.phase == NSEventPhaseNone)
			{
				mouseDeviceType
					.OnStartScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, {(int32)scroll.x, (int32)scroll.y}, window);
				mouseDeviceType
					.OnScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, {(int32)scroll.x, (int32)scroll.y}, window);

				mouseDeviceType.OnEndScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Math::Zero, window);
			}
		}
		break;
		case NSEventPhaseBegan:
		{
			mouseDeviceType
				.OnStartScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, {(int32)scroll.x, (int32)scroll.y}, window);
		}
		break;
		case NSEventPhaseChanged:
		{
			mouseDeviceType
				.OnScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, {(int32)scroll.x, (int32)scroll.y}, window);
		}
		break;
		case NSEventPhaseEnded:
		{
			mouseDeviceType.OnEndScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, Math::Zero, window);
		}
		break;
		case NSEventPhaseCancelled:
		{
			mouseDeviceType.OnCancelScroll(mouseIdentifier, (ScreenCoordinate)Math::Vector2d{point.x, point.y}, window);
		}
		break;
	}
}

/*- (void)magnifyWithEvent:(NSEvent *)event API_AVAILABLE(macos(10.5));
- (void)rotateWithEvent:(NSEvent *)event API_AVAILABLE(macos(10.5));
- (void)swipeWithEvent:(NSEvent *)event API_AVAILABLE(macos(10.5));*/

ngine::Optional<float> touchPressure;
ngine::UnorderedSet<ngine::Input::FingerIdentifier> startedTouchSet;

- (void)pressureChangeWithEvent:(NSEvent* _Nonnull)event
{
	if (event.stage != 0)
	{
		touchPressure = event.pressure;
	}
}

- (void)touchesBeganWithEvent:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	NSSet<NSTouch*>* touches = [event touchesMatchingPhase:NSTouchPhaseBegan inView:self];
	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	for (NSTouch* touch : touches)
	{
		CGPoint point;
		switch (touch.type)
		{
			case NSTouchTypeDirect:
				point = [touch locationInView:self];
				break;
			case NSTouchTypeIndirect:
				// Don't handle touchpad events, routed through macOS mouse emulation instead
				continue;
		}
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const uint16 radius = (uint16)Math::Ceil(70 * scaleFactor);

		Input::TouchDescriptor desc;
		desc.fingerIdentifier = reinterpret_cast<Input::FingerIdentifier>(touch.identity);
		desc.touchRadius = radius;
		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		desc.pressureRatio = touchPressure.IsValid() ? *touchPressure : 1.f;

		touchscreenDeviceType.OnStartTouch(Move(desc), touchscreenIdentifier, *&window);
	}
}

- (void)touchesMovedWithEvent:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	NSSet<NSTouch*>* touches = [event touchesMatchingPhase:NSTouchPhaseMoved inView:self];
	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	for (NSTouch* touch : touches)
	{
		CGPoint point;
		switch (touch.type)
		{
			case NSTouchTypeDirect:
				point = [touch locationInView:self];
				break;
			case NSTouchTypeIndirect:
				// Don't handle touchpad events, routed through macOS mouse emulation instead
				continue;
		}
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const uint16 radius = (uint16)Math::Ceil(70 * scaleFactor);

		Input::TouchDescriptor desc;
		desc.fingerIdentifier = reinterpret_cast<Input::FingerIdentifier>(touch.identity);
		desc.touchRadius = radius;
		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		desc.pressureRatio = touchPressure.IsValid() ? *touchPressure : 1.f;
		touchscreenDeviceType.OnMotion(Move(desc), touchscreenIdentifier, *&window);
	}
}

- (void)touchesEndedWithEvent:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	NSSet<NSTouch*>* touches = [event touchesMatchingPhase:NSTouchPhaseEnded inView:self];
	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	for (NSTouch* touch : touches)
	{
		CGPoint point;
		switch (touch.type)
		{
			case NSTouchTypeDirect:
				point = [touch locationInView:self];
				break;
			case NSTouchTypeIndirect:
				// Don't handle touchpad events, routed through macOS mouse emulation instead
				continue;
		}
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const uint16 radius = (uint16)Math::Ceil(70 * scaleFactor);

		Input::TouchDescriptor desc;
		desc.fingerIdentifier = reinterpret_cast<Input::FingerIdentifier>(touch.identity);
		desc.touchRadius = radius;
		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		desc.pressureRatio = touchPressure.IsValid() ? *touchPressure : 1.f;

		touchscreenDeviceType.OnStopTouch(Move(desc), touchscreenIdentifier, *&window);
		Assert(startedTouchSet.Contains(desc.fingerIdentifier));
		startedTouchSet.Remove(startedTouchSet.Find(desc.fingerIdentifier));
	}
	touchPressure = Invalid;
}

- (void)touchesCancelledWithEvent:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	NSSet<NSTouch*>* touches = [event touchesMatchingPhase:NSTouchPhaseCancelled inView:self];
	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	for (NSTouch* touch : touches)
	{
		switch (touch.type)
		{
			case NSTouchTypeDirect:
				break;
			case NSTouchTypeIndirect:
				// Don't handle touchpad events, routed through macOS mouse emulation instead
				continue;
		}
		const Input::FingerIdentifier fingerIdentifier = reinterpret_cast<Input::FingerIdentifier>(touch.identity);
		touchscreenDeviceType.OnCancelTouch(touchscreenIdentifier, fingerIdentifier, *&window);
		Assert(startedTouchSet.Contains(fingerIdentifier));
		startedTouchSet.Remove(startedTouchSet.Find(fingerIdentifier));
	}
	touchPressure = Invalid;
}

namespace ngine
{
	struct KeyMapping
	{
		uint32 source;
		Input::KeyboardInput target;
	};

	inline static constexpr Array KeyboardMappings = {
		KeyMapping{
			kVK_ANSI_A,
			Input::KeyboardInput::A,
		},
		KeyMapping{
			kVK_ANSI_B,
			Input::KeyboardInput::B,
		},
		KeyMapping{
			kVK_ANSI_C,
			Input::KeyboardInput::C,
		},
		KeyMapping{
			kVK_ANSI_D,
			Input::KeyboardInput::D,
		},
		KeyMapping{
			kVK_ANSI_E,
			Input::KeyboardInput::E,
		},
		KeyMapping{
			kVK_ANSI_F,
			Input::KeyboardInput::F,
		},
		KeyMapping{
			kVK_ANSI_G,
			Input::KeyboardInput::G,
		},
		KeyMapping{
			kVK_ANSI_H,
			Input::KeyboardInput::H,
		},
		KeyMapping{
			kVK_ANSI_I,
			Input::KeyboardInput::I,
		},
		KeyMapping{
			kVK_ANSI_J,
			Input::KeyboardInput::J,
		},
		KeyMapping{
			kVK_ANSI_K,
			Input::KeyboardInput::K,
		},
		KeyMapping{
			kVK_ANSI_L,
			Input::KeyboardInput::L,
		},
		KeyMapping{
			kVK_ANSI_M,
			Input::KeyboardInput::M,
		},
		KeyMapping{
			kVK_ANSI_N,
			Input::KeyboardInput::N,
		},
		KeyMapping{
			kVK_ANSI_O,
			Input::KeyboardInput::O,
		},
		KeyMapping{
			kVK_ANSI_P,
			Input::KeyboardInput::P,
		},
		KeyMapping{
			kVK_ANSI_Q,
			Input::KeyboardInput::Q,
		},
		KeyMapping{
			kVK_ANSI_R,
			Input::KeyboardInput::R,
		},
		KeyMapping{
			kVK_ANSI_S,
			Input::KeyboardInput::S,
		},
		KeyMapping{
			kVK_ANSI_T,
			Input::KeyboardInput::T,
		},
		KeyMapping{
			kVK_ANSI_U,
			Input::KeyboardInput::U,
		},
		KeyMapping{
			kVK_ANSI_V,
			Input::KeyboardInput::V,
		},
		KeyMapping{
			kVK_ANSI_W,
			Input::KeyboardInput::W,
		},
		KeyMapping{
			kVK_ANSI_X,
			Input::KeyboardInput::X,
		},
		KeyMapping{
			kVK_ANSI_Y,
			Input::KeyboardInput::Y,
		},
		KeyMapping{
			kVK_ANSI_Z,
			Input::KeyboardInput::Z,
		},

		KeyMapping{
			kVK_ANSI_1,
			Input::KeyboardInput::One,
		},
		KeyMapping{
			kVK_ANSI_2,
			Input::KeyboardInput::Two,
		},
		KeyMapping{
			kVK_ANSI_3,
			Input::KeyboardInput::Three,
		},
		KeyMapping{
			kVK_ANSI_4,
			Input::KeyboardInput::Four,
		},
		KeyMapping{
			kVK_ANSI_5,
			Input::KeyboardInput::Five,
		},
		KeyMapping{
			kVK_ANSI_6,
			Input::KeyboardInput::Six,
		},
		KeyMapping{
			kVK_ANSI_7,
			Input::KeyboardInput::Seven,
		},
		KeyMapping{
			kVK_ANSI_8,
			Input::KeyboardInput::Eight,
		},
		KeyMapping{
			kVK_ANSI_9,
			Input::KeyboardInput::Nine,
		},
		KeyMapping{
			kVK_ANSI_0,
			Input::KeyboardInput::Zero,
		},

		KeyMapping{
			kVK_Return,
			Input::KeyboardInput::Enter,
		},
		KeyMapping{
			kVK_Escape,
			Input::KeyboardInput::Escape,
		},
		KeyMapping{
			kVK_Delete,
			Input::KeyboardInput::Backspace,
		},
		KeyMapping{
			kVK_ForwardDelete,
			Input::KeyboardInput::Delete,
		},
		KeyMapping{
			kVK_Tab,
			Input::KeyboardInput::Tab,
		},
		KeyMapping{
			kVK_Space,
			Input::KeyboardInput::Space,
		},

		KeyMapping{
			kVK_RightArrow,
			Input::KeyboardInput::ArrowRight,
		},
		KeyMapping{
			kVK_LeftArrow,
			Input::KeyboardInput::ArrowLeft,
		},
		KeyMapping{
			kVK_DownArrow,
			Input::KeyboardInput::ArrowDown,
		},
		KeyMapping{
			kVK_UpArrow,
			Input::KeyboardInput::ArrowUp,
		},

		KeyMapping{
			kVK_Control,
			Input::KeyboardInput::LeftControl,
		},
		KeyMapping{
			kVK_Shift,
			Input::KeyboardInput::LeftShift,
		},
		KeyMapping{
			kVK_Command,
			Input::KeyboardInput::LeftCommand,
		},
		KeyMapping{
			kVK_RightControl,
			Input::KeyboardInput::RightControl,
		},
		KeyMapping{
			kVK_RightShift,
			Input::KeyboardInput::RightShift,
		},
		KeyMapping{
			kVK_RightCommand,
			Input::KeyboardInput::RightCommand,
		},
		KeyMapping{kVK_ANSI_LeftBracket, Input::KeyboardInput::OpenBracket},
		KeyMapping{kVK_ANSI_RightBracket, Input::KeyboardInput::CloseBracket}
	};

	inline static Optional<Input::KeyboardInput> GetKeyboardInputFromKeycode(const uint32 keyCode)
	{
		for (const KeyMapping mapping : KeyboardMappings)
		{
			if (mapping.source == keyCode)
			{
				return mapping.target;
			}
		}

		return Invalid;
	}
}

- (void)flagsChanged:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	const NSEventModifierFlags modifierFlags = event.modifierFlags;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const EnumFlags<Input::KeyboardModifier> activeModifiers = keyboardDeviceType.GetActiveModifiers(keyboardIdentifier);
	auto handleModifier =
		[modifierFlags,
	   &keyboardDeviceType,
	   keyboardIdentifier,
	   activeModifiers](const Input::KeyboardModifier modifier, const Input::KeyboardInput input, const NSEventModifierFlags nsFlags)
	{
		const bool isModifierActive = modifierFlags & nsFlags;
		const bool wasModifierActive = activeModifiers.IsSet(modifier);
		if (isModifierActive != wasModifierActive)
		{
			if (isModifierActive)
			{
				keyboardDeviceType.OnKeyDown(keyboardIdentifier, input);
			}
			else
			{
				keyboardDeviceType.OnKeyUp(keyboardIdentifier, input);
			}
		}
	};
	handleModifier(Input::KeyboardModifier::Capital, Input::KeyboardInput::CapsLock, NSEventModifierFlagCapsLock);
	handleModifier(Input::KeyboardModifier::LeftShift, Input::KeyboardInput::LeftShift, NX_DEVICELSHIFTKEYMASK);
	handleModifier(Input::KeyboardModifier::RightShift, Input::KeyboardInput::RightShift, NX_DEVICERSHIFTKEYMASK);
	handleModifier(Input::KeyboardModifier::LeftControl, Input::KeyboardInput::LeftControl, NX_DEVICELCTLKEYMASK);
	handleModifier(Input::KeyboardModifier::RightControl, Input::KeyboardInput::RightControl, NX_DEVICERCTLKEYMASK);
	handleModifier(Input::KeyboardModifier::LeftAlt, Input::KeyboardInput::LeftAlt, NX_DEVICELALTKEYMASK);
	handleModifier(Input::KeyboardModifier::RightAlt, Input::KeyboardInput::RightAlt, NX_DEVICERALTKEYMASK);
	handleModifier(Input::KeyboardModifier::LeftCommand, Input::KeyboardInput::LeftCommand, NX_DEVICELCMDKEYMASK);
	handleModifier(Input::KeyboardModifier::RightCommand, Input::KeyboardInput::RightCommand, NX_DEVICERCMDKEYMASK);
}

- (void)keyDown:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	if (Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(event.keyCode))
	{
		keyboardDeviceType.OnKeyDown(keyboardIdentifier, input.Get());
	}

	NSString* characters = event.characters;
	keyboardDeviceType.OnText(
		keyboardIdentifier,
		ConstUnicodeStringView{
			reinterpret_cast<const char16_t*>([characters cStringUsingEncoding:NSUTF16StringEncoding]),
			(uint32)[characters length]
		}
	);
}

- (void)keyUp:(NSEvent* _Nonnull)event
{
	if (self.engineWindow == nullptr)
	{
		return;
	}

	using namespace ngine;
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	if (Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(event.keyCode))
	{
		keyboardDeviceType.OnKeyUp(keyboardIdentifier, input.Get());
	}
}

bool startedDrag = false;
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo> _Nonnull)sender
{
	NSPasteboard* pasteboard = sender.draggingPasteboard;
	if ([pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:nil])
	{
		using namespace ngine;
		Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
		const float scaleFactor = window.GetPhysicalDevicePixelRatio();

		NSPoint point = [sender draggingLocation];
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		InlineVector<Widgets::DragAndDropData, 1> draggedItems;

		NSArray* draggedItemsArray = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:nil];
		draggedItems.Reserve((uint32)draggedItemsArray.count);
		for (id<NSObject> object : draggedItemsArray)
		{
			NSURL* url = (NSURL*)object;
			if (url.isFileURL)
			{
				[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
					if (bookmarkData != nil)
					{
						NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
						NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
						if (securityBookmarks == nil)
						{
							securityBookmarks = [[NSArray<NSData*> alloc] init];
						}
						NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
						[mutableSecurityBookmarks addObject:bookmarkData];
						[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
					}
				}
#endif

				const char* filePath = url.fileSystemRepresentation;
				draggedItems.EmplaceBack(IO::Path(filePath, (IO::Path::SizeType)strlen(filePath)));
			}
		}

		if (draggedItems.HasElements())
		{
			Assert(!startedDrag);
			startedDrag = true;
			if (window.OnStartDragItemsIntoWindow((WindowCoordinate)Math::Vector2d{point.x, point.y}, draggedItems))
			{
				return NSDragOperationCopy;
			}
		}
	}

	return NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo> _Nonnull)sender
{
	NSPasteboard* pasteboard = sender.draggingPasteboard;
	if ([pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:nil])
	{
		using namespace ngine;
		Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
		const float scaleFactor = window.GetPhysicalDevicePixelRatio();

		NSPoint point = [sender draggingLocation];
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		InlineVector<Widgets::DragAndDropData, 1> draggedItems;

		NSArray* draggedItemsArray = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:nil];
		draggedItems.Reserve((uint32)draggedItemsArray.count);
		for (id<NSObject> object : draggedItemsArray)
		{
			NSURL* url = (NSURL*)object;
			if (url.isFileURL)
			{
				[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
					if (bookmarkData != nil)
					{
						NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
						NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
						if (securityBookmarks == nil)
						{
							securityBookmarks = [[NSArray<NSData*> alloc] init];
						}
						NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
						[mutableSecurityBookmarks addObject:bookmarkData];
						[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
					}
				}
#endif

				const char* filePath = url.fileSystemRepresentation;
				draggedItems.EmplaceBack(IO::Path(filePath, (IO::Path::SizeType)strlen(filePath)));
			}
		}

		if (draggedItems.HasElements())
		{
			Assert(startedDrag);
			if (window.OnDragItemsOverWindow((WindowCoordinate)Math::Vector2d{point.x, point.y}, draggedItems))
			{
				return NSDragOperationCopy;
			}
		}
	}

	return NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo> _Nullable)sender
{
	if (startedDrag)
	{
		startedDrag = false;
		NSPasteboard* pasteboard = sender.draggingPasteboard;
		if ([pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:nil])
		{
			using namespace ngine;
			Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
			window.OnCancelDragItemsIntoWindow();
		}
	}
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo> _Nonnull)sender
{
	return true;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo> _Nonnull)sender
{
	NSPasteboard* pasteboard = sender.draggingPasteboard;
	if ([pasteboard canReadObjectForClasses:@[ [NSURL class] ] options:nil])
	{
		using namespace ngine;
		Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
		const float scaleFactor = window.GetPhysicalDevicePixelRatio();

		NSPoint point = [sender draggingLocation];
		point.y = self.frame.size.height - point.y;
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		InlineVector<Widgets::DragAndDropData, 1> draggedItems;

		NSArray* draggedItemsArray = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:nil];
		draggedItems.Reserve((uint32)draggedItemsArray.count);
		for (id<NSObject> object : draggedItemsArray)
		{
			NSURL* url = (NSURL*)object;
			if (url.isFileURL)
			{
				[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
					if (bookmarkData != nil)
					{
						NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
						NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
						if (securityBookmarks == nil)
						{
							securityBookmarks = [[NSArray<NSData*> alloc] init];
						}
						NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
						[mutableSecurityBookmarks addObject:bookmarkData];
						[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
					}
				}
#endif

				const char* filePath = url.fileSystemRepresentation;
				draggedItems.EmplaceBack(IO::Path(filePath, (IO::Path::SizeType)strlen(filePath)));
			}
		}

		if (draggedItems.HasElements())
		{
			Assert(startedDrag);
			if (window.OnDropItemsIntoWindow((WindowCoordinate)Math::Vector2d{point.x, point.y}, draggedItems))
			{
				startedDrag = false;
				return true;
			}
		}
	}
	startedDrag = false;

	return false;
}

- (void)concludeDragOperation:(id<NSDraggingInfo> _Nullable)sender
{
	Assert(!startedDrag);
}

- (void)draggingEnded:(id<NSDraggingInfo> _Nonnull)sender
{
	Assert(!startedDrag);
}

#endif

- (void)onViewportResizingStart:(CGSize)size
{
	using namespace ngine;

	const CGSize currentSize = self.metalLayer.drawableSize;
	if (size.width == currentSize.width && size.height == currentSize.height)
	{
		return;
	}

	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	window.TryOrQueueResize({(uint32)size.width, (uint32)size.height});
}

- (void)onViewportResizingFinished:(CGSize)size
{
	self.metalLayer.drawableSize = size;

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	// reconnect virtual controller
	[self getControllerManager].ResetVirtualController();
#endif
}

- (void)updateDrawableSize
{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	UIWindow* window = self.engineWindow != nil ? ((__bridge UIWindow*)((ngine::Rendering::Window*)self.engineWindow)->GetOSHandle())
	                                            : self.window;
	UIWindowScene* __strong windowScene = window.windowScene;

	CGSize size = windowScene.coordinateSpace.bounds.size;
#if PLATFORM_APPLE_VISIONOS
	const float scaleFactor = 1.5f;
#else
	const float scaleFactor = (float)[window screen].nativeScale;
#endif
#else

	NSWindow* window = self.engineWindow != nil ? ((__bridge NSWindow*)((ngine::Rendering::Window*)self.engineWindow)->GetOSHandle())
	                                            : self.window;
	CGSize size = window.frame.size;
	const float scaleFactor = (float)window.backingScaleFactor;
#endif

	size.width *= scaleFactor;
	size.height *= scaleFactor;

	self.metalLayer.drawableSize = size;
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
// Some devices don't offer force, so provide a default one (1.f)
ngine::Math::Ratiof GetPressureRatio(UITouch* _Nonnull touch)
{
	const float force = (float)[touch force];
	return ngine::Math::Ratiof(force == 0.f ? 1.f : force);
}

- (void)touchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nullable)event
{
	using namespace ngine;

	[super touchesBegan:touches withEvent:event];

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	for (UITouch* touch : touches)
	{
		CGPoint point = [touch locationInView:self];
		const uint16 radius = (uint16)Math::Ceil([touch majorRadius] * scaleFactor);

		point.x *= scaleFactor;
		point.y *= scaleFactor;

		Input::TouchDescriptor desc;
		const UITouchType touchType = [touch type];
		desc.fingerIdentifier = [touch hash];
		desc.touchRadius = radius;
		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		desc.pressureRatio = GetPressureRatio(touch);

		switch (touchType)
		{
			case UITouchTypeDirect:
			case UITouchTypeIndirect:
			case UITouchTypePencil:
			{
				touchscreenDeviceType.OnStartTouch(Move(desc), touchscreenIdentifier, *&window);
			}
			break;
			case UITouchTypeIndirectPointer:
			{
				mouseDeviceType.OnPress(mouseIdentifier, desc.screenCoordinate, Input::MouseButton::Left, *&window);
			}
			break;
		}
	}
#endif
}

- (void)touchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nullable)event
{
	using namespace ngine;

	[super touchesEnded:touches withEvent:event];

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	for (UITouch* touch : touches)
	{
		CGPoint point = [touch locationInView:self];
		const uint16 radius = (uint16)Math::Ceil([touch majorRadius] * scaleFactor);

		point.x *= scaleFactor;
		point.y *= scaleFactor;

		Input::TouchDescriptor desc;
		const UITouchType touchType = [touch type];
		desc.fingerIdentifier = [touch hash];
		desc.touchRadius = radius;

		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		desc.pressureRatio = GetPressureRatio(touch);

		switch (touchType)
		{
			case UITouchTypeDirect:
			case UITouchTypeIndirect:
			case UITouchTypePencil:
			{
				touchscreenDeviceType.OnStopTouch(Move(desc), touchscreenIdentifier, *&window);
			}
			break;
			case UITouchTypeIndirectPointer:
			{
				mouseDeviceType.OnRelease(mouseIdentifier, desc.screenCoordinate, Input::MouseButton::Left, &window);
			}
			break;
		}
	}
#endif
}

- (void)touchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nullable)event
{
	using namespace ngine;

	[super touchesEnded:touches withEvent:event];

#if !PLATFORM_APPLE_VISIONOS
	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	for ([[maybe_unused]] UITouch* touch : touches)
	{
		switch ([touch type])
		{
			case UITouchTypeDirect:
			case UITouchTypeIndirect:
			case UITouchTypePencil:
			{
				const Input::FingerIdentifier fingerIdentifier = [touch hash];
				touchscreenDeviceType.OnCancelTouch(touchscreenIdentifier, fingerIdentifier, *&window);
			}
			break;
			case UITouchTypeIndirectPointer:
			{
				mouseDeviceType.OnPressCancelled(mouseIdentifier, Input::MouseButton::Left, &window);
			}
			break;
		}
	}
#endif
}

- (void)touchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nullable)event
{
	using namespace ngine;

	[super touchesMoved:touches withEvent:event];

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::TouchscreenDeviceType& touchscreenDeviceType =
		inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
	const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
	const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	for (UITouch* touch : touches)
	{
		CGPoint point = [touch locationInView:self];
		CGPoint previousPoint = [touch previousLocationInView:self];
		const uint16 radius = (uint16)Math::Ceil([touch majorRadius] * scaleFactor);

		point.x *= scaleFactor;
		point.y *= scaleFactor;

		previousPoint.x *= scaleFactor;
		previousPoint.y *= scaleFactor;

		Input::TouchDescriptor desc;
		const UITouchType touchType = [touch type];
		desc.fingerIdentifier = [touch hash];
		desc.touchRadius = radius;

		desc.screenCoordinate = (ScreenCoordinate)Math::Vector2d{point.x, point.y};
		const Math::Vector2i deltaCoordinates = {(int)previousPoint.x - (int)point.x, (int)previousPoint.y - (int)point.y};
		desc.deltaCoordinates = {(int)previousPoint.x - (int)point.x, (int)previousPoint.y - (int)point.y};
		desc.pressureRatio = GetPressureRatio(touch);

		switch (touchType)
		{
			case UITouchTypeDirect:
			case UITouchTypeIndirect:
			case UITouchTypePencil:
			{
				touchscreenDeviceType.OnMotion(Move(desc), touchscreenIdentifier, *&window);
			}
			break;
			case UITouchTypeIndirectPointer:
			{
				mouseDeviceType.OnMotion(mouseIdentifier, desc.screenCoordinate, desc.deltaCoordinates, *&window);
			}
			break;
		}
	}
#endif
}

- (void)secondaryTouchesBegan:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event
{
	using namespace ngine;

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	for (UITouch* touch : touches)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier()
		);
		const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

		CGPoint point = [touch locationInView:self];
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

		mouseDeviceType.OnPress(mouseIdentifier, coordinates, Input::MouseButton::Right, *&window);
	}
#endif
}

- (void)secondaryTouchesEnded:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event
{
	using namespace ngine;

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	for (UITouch* touch : touches)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier()
		);
		const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

		CGPoint point = [touch locationInView:self];
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

		mouseDeviceType.OnRelease(mouseIdentifier, coordinates, Input::MouseButton::Right, &window);
	}
#endif
}

- (void)secondaryTouchesCancelled:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event
{
	[self secondaryTouchesEnded:touches withEvent:event];
}

- (void)secondaryTouchesMoved:(NSSet<UITouch*>* _Nonnull)touches withEvent:(UIEvent* _Nonnull)event
{
	using namespace ngine;

#if !PLATFORM_APPLE_VISIONOS
	if (self.engineWindow == nullptr)
	{
		return;
	}

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;

	const float scaleFactor = window.GetPhysicalDevicePixelRatio();

	for (UITouch* touch : touches)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier()
		);
		const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(0, inputManager, &window);

		CGPoint point = [touch locationInView:self];
		point.x *= scaleFactor;
		point.y *= scaleFactor;

		const ScreenCoordinate coordinates = (ScreenCoordinate)Math::Vector2d{point.x, point.y};

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
		Math::Vector2i delta;
		CGGetLastMouseDelta(&delta.x, &delta.y);
		mouseDeviceType.OnMotion(mouseIdentifier, coordinates, delta, *&window);
#else
		mouseDeviceType.OnMotion(mouseIdentifier, coordinates, *&window);
#endif
	}
#endif
}

- (void)motionBegan:(UIEventSubtype)motion withEvent:(UIEvent* _Nullable)event
{
}

- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent* _Nullable)event
{
}

- (void)motionCancelled:(UIEventSubtype)motion withEvent:(UIEvent* _Nullable)event
{
}

namespace ngine
{
	struct KeyMapping
	{
		UIKeyboardHIDUsage source;
		Input::KeyboardInput target;
	};

	inline static constexpr Array KeyboardMappings = {
		KeyMapping{
			UIKeyboardHIDUsageKeyboardA,
			Input::KeyboardInput::A,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardB,
			Input::KeyboardInput::B,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardC,
			Input::KeyboardInput::C,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardD,
			Input::KeyboardInput::D,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardE,
			Input::KeyboardInput::E,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardF,
			Input::KeyboardInput::F,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardG,
			Input::KeyboardInput::G,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardH,
			Input::KeyboardInput::H,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardI,
			Input::KeyboardInput::I,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardJ,
			Input::KeyboardInput::J,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardK,
			Input::KeyboardInput::K,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardL,
			Input::KeyboardInput::L,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardM,
			Input::KeyboardInput::M,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardN,
			Input::KeyboardInput::N,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardO,
			Input::KeyboardInput::O,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardP,
			Input::KeyboardInput::P,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardQ,
			Input::KeyboardInput::Q,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardR,
			Input::KeyboardInput::R,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardS,
			Input::KeyboardInput::S,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardT,
			Input::KeyboardInput::T,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardU,
			Input::KeyboardInput::U,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardV,
			Input::KeyboardInput::V,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardW,
			Input::KeyboardInput::W,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardX,
			Input::KeyboardInput::X,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardY,
			Input::KeyboardInput::Y,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardZ,
			Input::KeyboardInput::Z,
		},

		KeyMapping{
			UIKeyboardHIDUsageKeyboard1,
			Input::KeyboardInput::One,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard2,
			Input::KeyboardInput::Two,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard3,
			Input::KeyboardInput::Three,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard4,
			Input::KeyboardInput::Four,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard5,
			Input::KeyboardInput::Five,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard6,
			Input::KeyboardInput::Six,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard7,
			Input::KeyboardInput::Seven,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard8,
			Input::KeyboardInput::Eight,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard9,
			Input::KeyboardInput::Nine,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboard0,
			Input::KeyboardInput::One,
		},

		KeyMapping{
			UIKeyboardHIDUsageKeyboardReturnOrEnter,
			Input::KeyboardInput::Enter,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardEscape,
			Input::KeyboardInput::Escape,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardDeleteOrBackspace,
			Input::KeyboardInput::Backspace,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardDeleteForward,
			Input::KeyboardInput::Delete,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardTab,
			Input::KeyboardInput::Tab,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardSpacebar,
			Input::KeyboardInput::Space,
		},

		KeyMapping{
			UIKeyboardHIDUsageKeyboardRightArrow,
			Input::KeyboardInput::ArrowRight,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardLeftArrow,
			Input::KeyboardInput::ArrowLeft,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardDownArrow,
			Input::KeyboardInput::ArrowDown,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardUpArrow,
			Input::KeyboardInput::ArrowUp,
		},

		KeyMapping{
			UIKeyboardHIDUsageKeyboardLeftControl,
			Input::KeyboardInput::LeftControl,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardLeftShift,
			Input::KeyboardInput::LeftShift,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardLeftAlt,
			Input::KeyboardInput::LeftAlt,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardLeftGUI,
			Input::KeyboardInput::LeftCommand,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardRightControl,
			Input::KeyboardInput::RightControl,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardRightShift,
			Input::KeyboardInput::RightShift,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardRightAlt,
			Input::KeyboardInput::RightAlt,
		},
		KeyMapping{
			UIKeyboardHIDUsageKeyboardRightGUI,
			Input::KeyboardInput::RightCommand,
		},
		KeyMapping{UIKeyboardHIDUsageKeyboardOpenBracket, Input::KeyboardInput::OpenBracket},
		KeyMapping{UIKeyboardHIDUsageKeyboardCloseBracket, Input::KeyboardInput::CloseBracket},
		KeyMapping{UIKeyboardHIDUsageKeyboardRightArrow, Input::KeyboardInput::ArrowRight},
		KeyMapping{UIKeyboardHIDUsageKeyboardLeftArrow, Input::KeyboardInput::ArrowLeft},
		KeyMapping{UIKeyboardHIDUsageKeyboardDownArrow, Input::KeyboardInput::ArrowDown},
		KeyMapping{UIKeyboardHIDUsageKeyboardUpArrow, Input::KeyboardInput::ArrowUp}
	};

	inline static Optional<Input::KeyboardInput> GetKeyboardInputFromKeycode(const UIKeyboardHIDUsage keyCode)
	{
		for (const KeyMapping mapping : KeyboardMappings)
		{
			if (mapping.source == keyCode)
			{
				return mapping.target;
			}
		}

		return Invalid;
	}
}

- (void)pressesBegan:(NSSet<UIPress*>* _Nonnull)presses withEvent:(UIPressesEvent* _Nullable)event
{
	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	for (UIPress* press : presses)
	{
		if (Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(press.key.keyCode))
		{
			keyboardDeviceType.OnKeyDown(keyboardIdentifier, input.Get());
		}

		NSString* characters = press.key.characters;
		keyboardDeviceType.OnText(
			keyboardIdentifier,
			ConstUnicodeStringView{
				reinterpret_cast<const char16_t*>([characters cStringUsingEncoding:NSUTF16StringEncoding]),
				(uint32)[characters length]
			}
		);
	}
}

- (void)pressesEnded:(NSSet<UIPress*>* _Nonnull)presses withEvent:(UIPressesEvent* _Nullable)event
{
	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	for (UIPress* press : presses)
	{
		if (Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(press.key.keyCode))
		{
			keyboardDeviceType.OnKeyUp(keyboardIdentifier, input.Get());
		}
	}
}

- (void)pressesCancelled:(NSSet<UIPress*>* _Nonnull)presses withEvent:(UIPressesEvent* _Nullable)event
{
	using namespace ngine;

	Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
	Input::Manager& inputManager = System::Get<Input::Manager>();
	Input::KeyboardDeviceType& keyboardDeviceType =
		inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
	const Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager, &window);

	for (UIPress* press : presses)
	{
		if (Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(press.key.keyCode))
		{
			keyboardDeviceType.OnKeyUp(keyboardIdentifier, input.Get());
		}
	}
}
/*
- (void)pressesChanged:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent*)event
{
    AppDelegate* appDelegate = (AppDelegate*)[[UIApplication sharedApplication] delegate];
    ngine::Engine& engine = *(ngine::Engine*)[appDelegate GetEngine];
    Input::Manager& inputManager = System::Get<Input::Manager>();
    Input::KeyboardDeviceType& keyboardDeviceType =
inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier()); const
Input::DeviceIdentifier keyboardIdentifier = keyboardDeviceType.GetOrRegisterInstance(0, inputManager,
&window);

    for(UIPress* press : presses)
    {
        if(press.phase == UIPressPhaseEnded)
        {
            if(Optional<Input::KeyboardInput> input = GetKeyboardInputFromKeycode(press.key.keyCode))
            {
                keyboardDeviceType.OnKeyUp(keyboardIdentifier, input.Get());
            }
        }
    }
}*/

- (BOOL)hasText
{
	using namespace ngine;

	if (self.engineWindow != nullptr)
	{
		Rendering::Window& window = *(Rendering::Window*)self.engineWindow;
		return window.HasTextInInputFocus();
	}
	return NO;
}

- (void)insertText:(NSString* _Nonnull)text
{
	using namespace ngine;

	if (self.engineWindow != nullptr)
	{
		Rendering::Window* pWindow = (Rendering::Window*)self.engineWindow;
		pWindow->InsertTextIntoInputFocus(
			ConstUnicodeStringView{reinterpret_cast<const char16_t*>([text cStringUsingEncoding:NSUTF16StringEncoding]), (uint32)[text length]}
		);
	}
}

- (void)deleteBackward
{
	using namespace ngine;

	if (self.engineWindow != nullptr)
	{
		Rendering::Window* pWindow = (Rendering::Window*)self.engineWindow;
		pWindow->DeleteTextInInputFocusBackwards();
	}
}

- (BOOL)canBecomeFirstResponder
{
	return YES;
}

- (void)showOnScreenKeyboard:(unsigned int)keyboardTypeFlagsInt
{
	using namespace ngine;

	EnumFlags<Rendering::KeyboardTypeFlags> keyboardTypeFlags{static_cast<Rendering::KeyboardTypeFlags>(keyboardTypeFlagsInt)};
	UIKeyboardType keyboardType;
	if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::URL))
	{
		keyboardType = UIKeyboardTypeURL;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Email))
	{
		keyboardType = UIKeyboardTypeEmailAddress;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::WebSearch))
	{
		keyboardType = UIKeyboardTypeWebSearch;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Number))
	{
		if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::ASCII))
		{
			keyboardType = UIKeyboardTypeASCIICapableNumberPad;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Signed))
		{
			keyboardType = UIKeyboardTypeNumbersAndPunctuation;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Decimal))
		{
			keyboardType = UIKeyboardTypeDecimalPad;
		}
		else
		{
			keyboardType = UIKeyboardTypeNumberPad;
		}
	}
	else if (keyboardTypeFlags.AreAllSet(Rendering::KeyboardTypeFlags::PhoneNumber | Rendering::KeyboardTypeFlags::Name))
	{
		keyboardType = UIKeyboardTypeNamePhonePad;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::PhoneNumber))
	{
		keyboardType = UIKeyboardTypePhonePad;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Name))
	{
		keyboardType = UIKeyboardTypeNamePhonePad;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::PIN))
	{
		keyboardType = UIKeyboardTypeNumberPad;
	}
	else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::ASCII))
	{
		keyboardType = UIKeyboardTypeASCIICapable;
	}
	else
	{
		keyboardType = UIKeyboardTypeDefault;
	}
	self.keyboardType = keyboardType;

	self.enablesReturnKeyAutomatically = YES;
	[self becomeFirstResponder];
}

- (void)hideOnScreenKeyboard
{
	[self resignFirstResponder];
}
#endif

namespace ngine
{

#if USE_APPLE_GAME_CONTROLLER
	inline static Array GamepadMappings = {
		Input::GamepadMapping{(__bridge void*)GCInputButtonA, Input::GamepadInput::Button::A},
		Input::GamepadMapping{(__bridge void*)GCInputButtonB, Input::GamepadInput::Button::B},
		Input::GamepadMapping{(__bridge void*)GCInputButtonX, Input::GamepadInput::Button::X},
		Input::GamepadMapping{(__bridge void*)GCInputButtonY, Input::GamepadInput::Button::Y},
		Input::GamepadMapping{(__bridge void*)GCInputButtonHome, Input::GamepadInput::Button::Home},
		Input::GamepadMapping{(__bridge void*)GCInputButtonMenu, Input::GamepadInput::Button::Menu},
		Input::GamepadMapping{(__bridge void*)GCInputButtonOptions, Input::GamepadInput::Button::Options},
		Input::GamepadMapping{(__bridge void*)GCInputButtonShare, Input::GamepadInput::Button::Share},
		Input::GamepadMapping{(__bridge void*)GCInputLeftShoulder, Input::GamepadInput::Button::LeftShoulder},
		Input::GamepadMapping{(__bridge void*)GCInputRightShoulder, Input::GamepadInput::Button::RightShoulder},
		Input::GamepadMapping{(__bridge void*)GCInputLeftTrigger, Input::GamepadInput::Analog::LeftTrigger},
		Input::GamepadMapping{(__bridge void*)GCInputRightTrigger, Input::GamepadInput::Analog::RightTrigger},
		Input::GamepadMapping{(__bridge void*)GCInputLeftThumbstick, Input::GamepadInput::Axis::LeftThumbstick},
		Input::GamepadMapping{(__bridge void*)GCInputRightThumbstick, Input::GamepadInput::Axis::RightThumbstick},
		Input::GamepadMapping{(__bridge void*)GCInputLeftThumbstickButton, Input::GamepadInput::Button::LeftThumbstick},
		Input::GamepadMapping{(__bridge void*)GCInputRightThumbstickButton, Input::GamepadInput::Button::RightThumbstick},
		Input::GamepadMapping{(__bridge void*)GCInputDirectionalDpad, Input::GamepadMapping::Type::DirectionalPad},
		Input::GamepadMapping{(__bridge void*)GCInputDualShockTouchpadButton, Input::GamepadInput::Button::Touchpad},
		Input::GamepadMapping{(__bridge void*)GCInputXboxPaddleOne, Input::GamepadInput::Button::PaddleOne},
		Input::GamepadMapping{(__bridge void*)GCInputXboxPaddleTwo, Input::GamepadInput::Button::PaddleTwo},
		Input::GamepadMapping{(__bridge void*)GCInputXboxPaddleThree, Input::GamepadInput::Button::PaddleThree},
		Input::GamepadMapping{(__bridge void*)GCInputXboxPaddleFour, Input::GamepadInput::Button::PaddleFour}
	};

	inline static Optional<Input::GamepadMapping> GetGamepadMappingFromSource(Input::GamepadMapping::SourceInputIdentifier _Nonnull source)
	{
		for (const Input::GamepadMapping mapping : GamepadMappings)
		{
			if (mapping.m_source == source)
			{
				return mapping;
			}
		}

		return Invalid;
	}
#endif

	void ControllerManager::Initialize(Engine& engine, Rendering::Window* _Nullable pWindow)
	{
		m_pEngine = engine;
		m_pWindow = pWindow;

		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

#if USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
		if (!GetControllers().GetSize())
		{
			CreateVirtualController();
		}
#endif

		gamepadDeviceType.OnInputEnabledEvent.Add(
			*this,
			[](ControllerManager& manager, const Input::ActionMonitor& actionMonitor)
			{
				manager.AssignMonitor(actionMonitor);

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
				if (!manager.GetControllers().GetSize())
				{
					manager.CreateVirtualController();
				}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
				manager.m_pGestureDelegate.hasVirtualController = true;
#endif
			}
		);
		gamepadDeviceType.OnMonitorAssigned.Add(
			*this,
			[](ControllerManager& manager, const Input::ActionMonitor& actionMonitor)
			{
				manager.AssignMonitor(actionMonitor);
#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
				if (!manager.GetControllers().GetSize())
				{
					manager.DestroyVirtualController();
					manager.CreateVirtualController();
				}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
				manager.m_pGestureDelegate.hasVirtualController = true;
#endif
			}
		);

#if USE_VIRTUAL_GAME_CONTROLLER
		gamepadDeviceType.OnInputDisabledEvent.Add(
			*this,
			[](ControllerManager& manager, const Input::ActionMonitor&)
			{
#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
				manager.DestroyVirtualController();
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
				manager.m_pGestureDelegate.hasVirtualController = false;
#endif
			}
		);
#endif
	}

#if USE_VIRTUAL_GAME_CONTROLLER
	void ControllerManager::CreateVirtualController()
	{
#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
		if (m_pVirtualController || m_pEngine.IsInvalid() || m_pWindow.IsInvalid() || m_pWindow == nullptr || m_pActionMonitor.IsInvalid())
		{
			return;
		}

		ngine::Engine& engine = *m_pEngine;
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		NSSet* elements = [NSSet set];
		Input::ActionMonitor::BoundInputActions::ConstRestrictedView boundInputActions = m_pActionMonitor->GetBoundActions();
		auto isActionBound = [boundInputActions](Input::InputIdentifier input)
		{
			return boundInputActions[input].GetSize() > 0;
		};

		for (const Input::GamepadMapping& mapping : Input::GamepadMappings)
		{
			switch (mapping.m_type)
			{
				case Input::GamepadMapping::Type::Button:
				{
					if (Optional<const Input::GamepadInput::Button*> pButtonInput = mapping.m_physicalTarget.Get<Input::GamepadInput::Button>())
					{
						if (isActionBound(gamepadDeviceType.GetInputIdentifier(*pButtonInput)))
						{
							elements = [elements setByAddingObject:mapping.m_source];
						}
					}
				}
				break;
				case Input::GamepadMapping::Type::Axis:
					[[fallthrough]];
				case Input::GamepadMapping::Type::DirectionalPad:
				{
					if (Optional<const Input::GamepadInput::Axis*> pAxisInput = mapping.m_physicalTarget.Get<Input::GamepadInput::Axis>())
					{
						if (isActionBound(gamepadDeviceType.GetInputIdentifier(*pAxisInput)))
						{
							elements = [elements setByAddingObject:mapping.m_source];
						}
					}
				}
				break;
				case Input::GamepadMapping::Type::AnalogInput:
				{
					if (Optional<const Input::GamepadInput::Analog*> pAnalogInput = mapping.m_physicalTarget.Get<Input::GamepadInput::Analog>())
					{
						if (isActionBound(gamepadDeviceType.GetInputIdentifier(*pAnalogInput)))
						{
							elements = [elements setByAddingObject:mapping.m_source];
						}
					}
				}
				break;
			}
		}

		GCVirtualControllerConfiguration* config = [[GCVirtualControllerConfiguration alloc] init];
		config.elements = elements;

		m_pVirtualController = [[GCVirtualController alloc] initWithConfiguration:config];
		[m_pVirtualController connectWithReplyHandler:^(NSError* _Nullable error) {
			if (error == nil)
			{
				m_pVirtualControllerHandler = m_pVirtualController.controller;
			}
		}];
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.EnableVirtualGamepad();
		m_pGestureDelegate.hasVirtualController = true;
#endif
	}

	void ControllerManager::DestroyVirtualController()
	{
#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
		if (m_pVirtualController)
		{
			[m_pVirtualController disconnect];
			m_pVirtualController = nullptr;
			RemoveController(m_pVirtualControllerHandler);
		}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.DisableVirtualGamepad();
		m_pGestureDelegate.hasVirtualController = false;
#endif
	}
#endif

#if USE_APPLE_VIRTUAL_GAME_CONTROLLER
	void ControllerManager::ResetVirtualController()
	{
		if (m_pVirtualController)
		{
			[m_pVirtualController disconnect];
			[m_pVirtualController connectWithReplyHandler:^(NSError* _Nullable error) {
				if (error == nil)
				{
					m_pVirtualControllerHandler = m_pVirtualController.controller;
				}
			}];
		}
	}

	bool ControllerManager::IsVirtualController(GCController* pController) const
	{
		return (pController == m_pVirtualControllerHandler);
	}
#elif USE_CUSTOM_VIRTUAL_GAME_CONTROLLER
	void ControllerManager::SetGestureDelegate(GestureDelegate* _Nullable pGestureDelegate)
	{
		m_pGestureDelegate = pGestureDelegate;
	}
#endif

	void ControllerManager::AddController(GCController* _Nonnull pController)
	{
		m_controllers.EmplaceBack(pController);

		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		[[maybe_unused]] const Input::DeviceIdentifier gamepadIdentifier =
			gamepadDeviceType.GetOrRegisterInstance(uintptr(pController), inputManager, m_pWindow);
	}

	ArrayView<GCController*> ControllerManager::GetControllers()
	{
		return m_controllers;
	}

	void ControllerManager::RemoveController(GCController* _Nonnull pController)
	{
		for (uint32 index = 0; index < m_controllers.GetSize(); ++index)
		{
			if (m_controllers[index] == pController)
			{
				m_controllers.RemoveAt(index);
				break;
			}
		}

		// TODO: Remove device instance?
	}

	void ControllerManager::AssignMonitor(const ngine::Input::ActionMonitor& actionMonitor)
	{
		m_pActionMonitor = actionMonitor;
	}
}

- (void)windowDidBecomeKey:(NSNotification* _Nonnull)notification
{
#if PLATFORM_APPLE_MACOS
	using namespace ngine;
	if (Rendering::Window* pWindow = (Rendering::Window*)self.engineWindow)
	{
		pWindow->OnSwitchToForeground();
		pWindow->OnReceivedKeyboardFocus();
	}
#endif
}

- (void)windowDidResignKey:(NSNotification* _Nonnull)notification
{
#if PLATFORM_APPLE_MACOS
	using namespace ngine;
	if (Rendering::Window* pWindow = (Rendering::Window*)self.engineWindow)
	{
		pWindow->OnLostKeyboardFocus();
		pWindow->OnSwitchToBackground();
		Rendering::LogicalDevice& logicalDevice = pWindow->GetLogicalDevice();
		logicalDevice.WaitUntilIdle();
		Assert(!pWindow->IsInForeground());
	}
#endif
}

@end
#endif
