#pragma once

#include <Common/Memory/Containers/Vector.h>
#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>
#include <Common/Function/Function.h>

#include <Common/Time/FrameTime.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Storage/IdentifierMask.h>

namespace ngine::Input
{
	template<typename VectorType, bool IsContinuous = false>
	struct SmoothedVectorAction : public Action
	{
		using Action::Action;

		void BindInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier) = delete;

		void
		BindAxisInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier, const VectorType direction)
		{
			m_binaryInputs.EmplaceBack(Input{inputIdentifier, direction});

			Action::BindInput(monitor, deviceType, inputIdentifier);
		}

		void UnbindInput(ActionMonitor& monitor, const InputIdentifier inputIdentifier)
		{
			m_binaryInputs.RemoveFirstOccurrencePredicate(
				[inputIdentifier](const Input& input) -> ErasePredicateResult
				{
					if (input.m_identifier == inputIdentifier)
					{
						return ErasePredicateResult::Remove;
					}

					return ErasePredicateResult::Continue;
				}
			);

			Action::UnbindInput(monitor, inputIdentifier);
		}

		[[nodiscard]] VectorType GetValue() const
		{
			return m_value;
		}

		[[nodiscard]] VectorType GetTargetValue() const
		{
			return m_targetValue;
		}

		void Update(const FrameTime deltaTime, const float smoothingTime)
		{
			const float effectiveSmoothingTime = 1.f / Math::Max(smoothingTime, 0.001f);
			m_value = Math::LinearInterpolate(m_value, m_targetValue, Math::Min(deltaTime * effectiveSmoothingTime, 1.f));
		}

		Function<void(DeviceIdentifier, VectorType, VectorType), 24> OnChanged{[](DeviceIdentifier, VectorType, VectorType)
		                                                                       {
																																					 }};
	protected:
		virtual void
		OnKeyboardInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			OnBinaryInputChanged(deviceIdentifier, inputIdentifier, true);
		}

		virtual void
		OnKeyboardInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			OnBinaryInputChanged(deviceIdentifier, inputIdentifier, false);
		}

		virtual void
		OnKeyboardInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			OnBinaryInputChanged(deviceIdentifier, inputIdentifier, false);
		}

		virtual void
		OnKeyboardInputRestored(const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>& inputs, const EnumFlags<KeyboardModifier>)
			override
		{
			for (const typename InputIdentifier::IndexType inputIdentifierIndex : inputs.GetSetBitsIterator())
			{
				const InputIdentifier inputIdentifier = InputIdentifier::MakeFromValidIndex(inputIdentifierIndex);
				OnBinaryInputChanged(deviceIdentifier, inputIdentifier, true);
			}
		}

		virtual void
		OnKeyboardInputRepeat(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			if constexpr (IsContinuous)
			{
				OnChanged(deviceIdentifier, m_value, m_targetValue);
			}
		}

		virtual void OnBinaryInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier) override
		{
			OnBinaryInputChanged(deviceIdentifier, inputIdentifier, true);
		}

		virtual void OnBinaryInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier) override
		{
			OnBinaryInputChanged(deviceIdentifier, inputIdentifier, false);
		}

		inline void OnBinaryInputChanged(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const bool value)
		{
			for (Input& input : m_binaryInputs)
			{
				if (input.m_identifier == inputIdentifier)
				{
					m_targetValue += input.m_direction * (-1.f + ((float)value * 2.f));
					break;
				}
			}

			OnChanged(deviceIdentifier, m_value, m_targetValue);
		}
	protected:
		VectorType m_value = Math::Zero;
		VectorType m_targetValue = Math::Zero;

		struct Input
		{
			InputIdentifier m_identifier;
			VectorType m_direction;
		};

		Vector<Input> m_binaryInputs;
	};
}
