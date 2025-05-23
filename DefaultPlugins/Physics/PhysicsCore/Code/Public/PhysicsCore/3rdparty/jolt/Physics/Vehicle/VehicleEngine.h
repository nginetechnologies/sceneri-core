// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <ObjectStream/SerializableObject.h>
#include <Core/LinearCurve.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#include <Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

#ifdef JPH_DEBUG_RENDERER
	class DebugRenderer;
#endif // JPH_DEBUG_RENDERER

/// Generic properties for a vehicle engine
class VehicleEngineSettings
{
public:
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(VehicleEngineSettings)

	/// Constructor
							VehicleEngineSettings();

	/// Saves the contents in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores the contents in binary form to inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	float					mMaxTorque = 500.0f;						///< Max amount of torque (Nm) that the engine can deliver
	float					mMinRPM = 1000.0f;							///< Min amount of revolutions per minute (rpm) the engine can produce without stalling
	float					mMaxRPM = 6000.0f;							///< Max amount of revolutions per minute (rpm) the engine can generate
	LinearCurve				mNormalizedTorque;							///< Curve that describes a ratio of the max torque the engine can produce vs the fraction of the max RPM of the engine
	float					mInertia = 2.0f;							///< Moment of inertia (kg m^2) of the engine
	float					mAngularDamping = 0.2f;						///< Angular damping factor of the wheel: dw/dt = -c * w
};

/// Runtime data for engine
class VehicleEngine : public VehicleEngineSettings
{
public:
	/// Multiply an angular velocity (rad/s) with this value to get rounds per minute (RPM)
	static constexpr float	cAngularVelocityToRPM = 60.0f / (2.0f * JPH_PI);

	/// Current rotation speed of engine in rounds per minute
	float					GetCurrentRPM() const						{ return mCurrentRPM; }

	/// Update rotation speed of engine in rounds per minute
	void					SetCurrentRPM(float inRPM)					{ mCurrentRPM = inRPM; }

	/// Get the amount of torque (N m) that the engine can supply
	/// @param inAcceleration How much the gas pedal is pressed [0, 1]
	float					GetTorque(float inAcceleration) const		{ return inAcceleration * mMaxTorque * mNormalizedTorque.GetValue(mCurrentRPM / mMaxRPM); }

	/// Update the engine RPM assuming the engine is not connected to the wheels
	/// @param inDeltaTime Delta time in seconds
	/// @param inAcceleration How much the gas pedal is pressed [0, 1]
	void					UpdateRPM(float inDeltaTime, float inAcceleration);

#ifdef JPH_DEBUG_RENDERER
	/// Debug draw a RPM meter
	void					DrawRPM(DebugRenderer *inRenderer, Vec3Arg inPosition, Vec3Arg inForward, Vec3Arg inUp, float inSize, float inShiftDownRPM, float inShiftUpRPM) const;
#endif // JPH_DEBUG_RENDERER

	/// Saving state for replay
	void					SaveState(StateRecorder &inStream) const;
	void					RestoreState(StateRecorder &inStream);

private:
	float					mCurrentRPM = 1000.0f;						///< Current rotation speed of engine in rounds per minute
};

JPH_NAMESPACE_END
