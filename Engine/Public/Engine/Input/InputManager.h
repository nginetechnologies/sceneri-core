#pragma once

#include "DeviceIdentifier.h"
#include "DeviceTypeIdentifier.h"
#include "InputIdentifier.h"
#include "Feedback/Feedback.h"
#include "DeviceType.h"

#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/SaltedIdentifierStorage.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/UnorderedMap.h>

#include <Common/Function/Function.h>

#include <Common/System/SystemType.h>
#include <Common/Storage/PersistentIdentifierStorage.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/AtomicBool.h>

namespace ngine
{
	struct Engine;

	namespace Rendering
	{
		struct Window;
	}

	namespace Threading
	{
		struct Job;
	}
}

namespace ngine::Input
{
	struct DeviceType;
	struct DeviceInstance;
	struct Action;
	struct Monitor;

	struct Manager final
	{
		inline static constexpr System::Type SystemType = System::Type::InputManager;

		Manager();
		~Manager();

		template<typename Type, typename... ConstructArgs>
		[[nodiscard]] DeviceTypeIdentifier RegisterDeviceType(ConstructArgs&&... args)
		{
			const Guid deviceTypeGuid = Type::DeviceTypeGuid;
			const DeviceTypeIdentifier identifier = m_deviceTypeIdentifierLookup.FindOrRegister(deviceTypeGuid);
			if (identifier.IsValid())
			{
				m_deviceTypes[identifier] = UniquePtr<Type>::Make(identifier, Forward<ConstructArgs>(args)...);
			}
			return identifier;
		}

		[[nodiscard]] Feedback& GetFeedback()
		{
			return m_feedbackEngine;
		}

		template<typename Type>
		[[nodiscard]] Type& GetDeviceType(const DeviceTypeIdentifier identifier) const
		{
			return static_cast<Type&>(*m_deviceTypes[identifier]);
		}

		[[nodiscard]] DeviceType& GetDeviceType(const DeviceTypeIdentifier identifier) const
		{
			return *m_deviceTypes[identifier];
		}

		template<typename Type>
		[[nodiscard]] Optional<Type*> FindDeviceType(const DeviceTypeIdentifier identifier) const
		{
			return static_cast<Type*>(m_deviceTypes[identifier].Get().Get());
		}

		[[nodiscard]] Optional<DeviceType*> FindDeviceType(const DeviceTypeIdentifier identifier) const
		{
			return m_deviceTypes[identifier].Get().Get();
		}

		[[nodiscard]] InputIdentifier RegisterInput()
		{
			return m_inputIdentifiers.AcquireIdentifier();
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, InputIdentifier>
		GetValidInputElementView(IdentifierArrayView<ElementType, InputIdentifier> view) const
		{
			return m_inputIdentifiers.GetValidElementView(view);
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, InputIdentifier>
		GetValidInputElementView(FixedIdentifierArrayView<ElementType, InputIdentifier> view) const
		{
			return m_inputIdentifiers.GetValidElementView(view);
		}

		[[nodiscard]] DeviceTypeIdentifier GetMouseDeviceTypeIdentifier() const
		{
			return m_mouseDeviceType;
		}

		[[nodiscard]] DeviceTypeIdentifier GetKeyboardDeviceTypeIdentifier() const
		{
			return m_keyboardDeviceType;
		}

		[[nodiscard]] DeviceTypeIdentifier GetTouchscreenDeviceTypeIdentifier() const
		{
			return m_touchscreenDeviceType;
		}

		[[nodiscard]] DeviceTypeIdentifier GetGamepadDeviceTypeIdentifier() const
		{
			return m_gamepadDeviceType;
		}

		template<typename InstanceType, typename... ConstructArgs>
		[[nodiscard]] DeviceIdentifier RegisterDeviceInstance(const DeviceTypeIdentifier typeIdentifier, ConstructArgs&&... args)
		{
			const DeviceIdentifier identifier = m_deviceIdentifiers.AcquireIdentifier();
			Assert(identifier.IsValid());
			if (LIKELY(identifier.IsValid()))
			{
				m_deviceInstances[identifier] = UniquePtr<InstanceType>::Make(identifier, typeIdentifier, Forward<ConstructArgs>(args)...);
			}
			return identifier;
		}

		[[nodiscard]] DeviceIdentifier
		RegisterDeviceInstance(const DeviceTypeIdentifier typeIdentifier, Optional<Rendering::Window*> pSourceWindow);

		[[nodiscard]] DeviceInstance& GetDeviceInstance(const DeviceIdentifier identifier) const
		{
			return *m_deviceInstances[identifier];
		}

		template<typename Callback>
		inline void IterateDeviceInstances(Callback&& callback) const
		{
			const decltype(m_deviceInstances
			)::ConstDynamicView instancesView = m_deviceIdentifiers.GetValidElementView(m_deviceInstances.GetView());
			for (Optional<DeviceInstance*> pInstance : instancesView)
			{
				if (pInstance != nullptr)
				{
					if (callback(*pInstance) == Memory::CallbackResult::Break)
					{
						break;
					}
				}
			}
		}

		[[nodiscard]] DeviceTypeIdentifier GetDeviceTypeIdentifier(Guid deviceTypeGuid) const;

		void OnMonitorDestroyed(Monitor& monitor);

#if PLATFORM_WINDOWS
		void HandleWindowsRawInput(const long long);
#endif

		[[nodiscard]] Threading::Job& GetPollForInputStage()
		{
			return m_pollForInputStage;
		}
		[[nodiscard]] Threading::Job& GetPolledForInputStage()
		{
			return m_polledForInputStage;
		}

		using QueuedInputFunction = Function<void(), 48>;
		template<typename Callback>
		void QueueInput(Callback&& function)
		{
			if (CanProcessInput())
			{
				function();
				Assert(m_canProcessInput, "Input was queued and executed outside of input polling logic");
			}
			else
			{
				const uint8 inputQueueIndex{m_activeInputQueueIndex};
				Threading::UniqueLock lock(m_inputQueueMutexes[inputQueueIndex]);
				m_inputQueues[inputQueueIndex].EmplaceBack(Forward<Callback>(function));
			}
		}
		//! Checks whether input events can be sent / handled on this thread
		[[nodiscard]] bool CanProcessInput() const;
	protected:
		friend Rendering::Window;

		void ProcessInputQueue();
	protected:
		Threading::Job& m_pollForInputStage;
		Threading::Job& m_processInputQueueStage;
		Threading::Job& m_polledForInputStage;

		PersistentIdentifierStorage<DeviceTypeIdentifier> m_deviceTypeIdentifierLookup;
		TIdentifierArray<UniquePtr<DeviceType>, DeviceTypeIdentifier> m_deviceTypes{Memory::Zeroed};

		TSaltedIdentifierStorage<DeviceIdentifier> m_deviceIdentifiers;
		TIdentifierArray<UniquePtr<DeviceInstance>, DeviceIdentifier> m_deviceInstances{Memory::Zeroed};

		TSaltedIdentifierStorage<InputIdentifier> m_inputIdentifiers;

		DeviceTypeIdentifier m_mouseDeviceType;
		DeviceTypeIdentifier m_keyboardDeviceType;
		DeviceTypeIdentifier m_touchscreenDeviceType;
		DeviceTypeIdentifier m_gamepadDeviceType;

		Array<Threading::Mutex, 2> m_inputQueueMutexes;
		using InputQueue = Vector<QueuedInputFunction>;
		Array<InputQueue, 2> m_inputQueues;
		uint8 m_activeInputQueueIndex{0};
		Threading::Atomic<bool> m_canProcessInput{false};

		Feedback m_feedbackEngine;
#if PLATFORM_WINDOWS
		InlineVector<unsigned char, 64, uint16> m_rawInputDataBuffer;
#endif
	};
}
