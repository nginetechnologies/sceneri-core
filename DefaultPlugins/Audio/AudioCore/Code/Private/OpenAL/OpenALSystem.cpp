#include "OpenAL/OpenALSystem.h"

#include <string.h>

#define AL_ALEXT_PROTOTYPES 1

#if PLATFORM_WEB
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <3rdparty/openal-soft/include/AL/al.h>
#include <3rdparty/openal-soft/include/AL/alext.h>
#endif

#include <Common/Assert/Assert.h>

#if PLATFORM_APPLE_IOS
#import <AudioToolbox/AudioToolbox.h>
#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>
#endif

namespace ngine::Audio
{
	InitializationResult OpenALSystem::Initialize()
	{
#if PLATFORM_APPLE_IOS
		{
			[[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient
																			 withOptions:AVAudioSessionCategoryOptionMixWithOthers
																						 error:nil];
			[[AVAudioSession sharedInstance] setActive:YES error:nil];
		}
#endif

		ALCdevice* pDevice;
		ALCcontext* pContext;

		// TODO: Support selecting device (for now takes default)
		pDevice = alcOpenDevice(nullptr);
		if (!pDevice)
		{
			return InitializationResult::Failed;
		}

		m_pDevice = pDevice;

#if PLATFORM_APPLE_IOS
		[[NSNotificationCenter defaultCenter]
			addObserverForName:AVAudioSessionInterruptionNotification
									object:nil
									 queue:nil
							usingBlock:^(NSNotification* notification) {
								NSDictionary* userInfo = [notification userInfo];
								NSNumber* key = [userInfo objectForKey:AVAudioSessionInterruptionTypeKey];
								AVAudioSessionInterruptionType interruptionType = (AVAudioSessionInterruptionType)[key unsignedIntValue];
								if (AVAudioSessionInterruptionTypeBegan == interruptionType)
								{
									[[AVAudioSession sharedInstance] setActive:NO error:nil];
									alcMakeContextCurrent(nullptr);
									if (m_pContext != nullptr)
									{
										alcSuspendContext(m_pContext);
									}
									alcDevicePauseSOFT(m_pDevice);
								}
								else if (AVAudioSessionInterruptionTypeEnded == interruptionType)
								{
									[[AVAudioSession sharedInstance] setActive:YES error:nil];
									alcMakeContextCurrent(m_pContext);
									if (m_pContext != nullptr)
									{
										alcProcessContext(m_pContext);
									}
									alcDeviceResumeSOFT(m_pDevice);
								}
							}];
#endif

		pContext = alcCreateContext(pDevice, nullptr);
		if (pContext == nullptr || alcMakeContextCurrent(pContext) == ALC_FALSE)
		{
			if (pContext != NULL)
				alcDestroyContext(pContext);

			alcCloseDevice(pDevice);

			return InitializationResult::Failed;
		}

		m_pContext = pContext;

		return InitializationResult::Success;
	}

	void OpenALSystem::Shutdown()
	{
		ALCdevice* pDevice = nullptr;
		ALCcontext* pContext = nullptr;
		pContext = alcGetCurrentContext();
		Assert(pContext);
		if (pContext == nullptr)
		{
			return;
		}

		pDevice = alcGetContextsDevice(pContext);

		alcMakeContextCurrent(nullptr);
		alcDestroyContext(pContext);
		alcCloseDevice(pDevice);
	}

	ConstStringView OpenALSystem::GetDeviceName() const
	{
		ALCdevice* pDevice = GetDevice();
		Assert(pDevice);
		if (pDevice)
		{
			return ConstStringView();
		}

		const ALCchar* name = nullptr;
		if (alcIsExtensionPresent(pDevice, "ALC_ENUMERATE_ALL_EXT"))
		{
			name = alcGetString(pDevice, ALC_ALL_DEVICES_SPECIFIER);
		}

		if (!name || alcGetError(pDevice) != AL_NO_ERROR)
		{
			name = alcGetString(pDevice, ALC_DEVICE_SPECIFIER);
		}

		return ConstStringView(name, (ConstStringView::SizeType)strlen(name));
	}

	ALCdevice* OpenALSystem::GetDevice() const
	{
		ALCcontext* pContext = nullptr;
		pContext = alcGetCurrentContext();
		Assert(pContext);
		if (pContext == nullptr)
		{
			return nullptr;
		}

		return alcGetContextsDevice(pContext);
	}
}
