// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <ObjectStream/SerializableObject.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#include <Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// How gears are shifted
enum class ETransmissionMode : uint8
{
	Auto,																///< Automatically shift gear up and down
	Manual,																///< Manual gear shift (call SetTransmissionInput)
};

/// Configuration for the transmission of a vehicle (gear box)
class VehicleTransmissionSettings
{
public:
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(VehicleTransmissionSettings)

	/// Saves the contents in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores the contents in binary form to inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	ETransmissionMode		mMode = ETransmissionMode::Auto;			///< How to switch gears
	Array<float>			mGearRatios { 2.66f, 1.78f, 1.3f, 1.0f, 0.74f }; ///< Ratio in rotation rate between engine and gear box, first element is 1st gear, 2nd element 2nd gear etc.
	Array<float>			mReverseGearRatios { -2.90f };				///< Ratio in rotation rate between engine and gear box when driving in reverse
	float					mSwitchTime = 0.5f;							///< How long it takes to switch gears (s), only used in auto mode
	float					mClutchReleaseTime = 0.3f;					///< How long it takes to release the clutch (go to full friction)
	float					mShiftUpRPM = 4000.0f;						///< If RPM of engine is bigger then this we will shift a gear up, only used in auto mode
	float					mShiftDownRPM = 2000.0f;					///< If RPM of engine is smaller then this we will shift a gear down, only used in auto mode
};

/// Runtime data for transmission
class VehicleTransmission : public VehicleTransmissionSettings
{
public:
	/// Set input from driver regarding the transmission (only relevant when transmission is set to manual mode)
	/// @param inCurrentGear Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	/// @param inClutchFriction Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	void					Set(int inCurrentGear, float inClutchFriction) { mCurrentGear = inCurrentGear; mClutchFriction = inClutchFriction; }

	/// Update the current gear and clutch friction if the transmission is in aut mode
	/// @param inDeltaTime Time step delta time in s
	/// @param inCurrentRPM Current RPM for engine
	/// @param inForwardInput Hint if the user wants to drive forward (> 0) or backwards (< 0)
	/// @param inEngineCanApplyTorque Indicates if the engine is connected ultimately to the ground, if not it makes no sense to shift up
	void					Update(float inDeltaTime, float inCurrentRPM, float inForwardInput, bool inEngineCanApplyTorque);

	/// Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	int						GetCurrentGear() const						{ return mCurrentGear; }

	/// Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	float					GetClutchFriction() const					{ return mClutchFriction; }

	/// If the auto box is currently switching gears
	bool					IsSwitchingGear() const						{ return mGearSwitchTimeLeft > 0.0f; }

	/// Return the transmission ratio based on the current gear (ratio between engine and differential)
	float					GetCurrentRatio() const;

	/// Saving state for replay
	void					SaveState(StateRecorder &inStream) const;
	void					RestoreState(StateRecorder &inStream);

private:
	int						mCurrentGear = 0;							///< Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	float					mClutchFriction = 1.0f;						///< Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	float					mGearSwitchTimeLeft = 0.0f;					///< When switching gears this will be > 0 and will cause the engine to not provide any torque to the wheels for a short time (used for automatic gear switching only)
	float					mClutchReleaseTimeLeft = 0.0f;				///< After switching gears this will be > 0 and will cause the clutch friction to go from 0 to 1 (used for automatic gear switching only)
};

JPH_NAMESPACE_END
