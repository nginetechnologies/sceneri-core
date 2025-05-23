// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Vehicle/VehicleConstraint.h>
#include <Physics/Vehicle/VehicleController.h>
#include <Physics/Vehicle/VehicleEngine.h>
#include <Physics/Vehicle/VehicleTransmission.h>
#include <Physics/Vehicle/VehicleDifferential.h>
#include <Core/LinearCurve.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;

/// WheelSettings object specifically for WheeledVehicleController
class WheelSettingsWV : public WheelSettings
{
public:
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(WheelSettingsWV)

	/// Constructor
								WheelSettingsWV();

	// See: WheelSettings
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	float						mInertia = 0.9f;							///< Moment of inertia (kg m^2), for a cylinder this would be 0.5 * M * R^2 which is 0.9 for a wheel with a mass of 20 kg and radius 0.3 m
	float						mAngularDamping = 0.2f;						///< Angular damping factor of the wheel: dw/dt = -c * w
	float						mMaxSteerAngle = DegreesToRadians(70.0f);	///< How much this wheel can steer (radians)
	LinearCurve					mLongitudinalFriction;						///< Friction in forward direction of tire as a function of the slip ratio (fraction): (omega_wheel * r_wheel - v_longitudinal) / |v_longitudinal|
	LinearCurve					mLateralFriction;							///< Friction in sideway direction of tire as a function of the slip angle (degrees): angle between relative contact velocity and vehicle direction
	float						mMaxBrakeTorque = 1500.0f;					///< How much torque (Nm) the brakes can apply to this wheel
	float						mMaxHandBrakeTorque = 4000.0f;				///< How much torque (Nm) the hand brake can apply to this wheel (usually only applied to the rear wheels)
};

/// Wheel object specifically for WheeledVehicleController
class WheelWV : public Wheel
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit					WheelWV(WheelSettingsWV &inWheel);

	/// Override GetSettings and cast to the correct class
	const WheelSettingsWV *		GetSettings() const							{ return static_cast<const WheelSettingsWV *>(mSettings.GetPtr()); }

	/// Apply a torque (N m) to the wheel for a particular delta time
	void						ApplyTorque(float inTorque, float inDeltaTime)
	{
		mAngularVelocity += inTorque * inDeltaTime / GetSettings()->mInertia;
	}

	/// Update the wheel rotation based on the current angular velocity
	void						Update(float inDeltaTime, const VehicleConstraint &inConstraint);

	float						mCombinedLongitudinalFriction = 0.0f;		///< Combined friction coefficient in longitudinal direction (combines terrain and tires)
	float						mCombinedLateralFriction = 0.0f;			///< Combined friction coefficient in lateral direction (combines terrain and tires)
	float 						mLongitudinalSlip = 0.f;
	float 						mLateralSlip = 0.f;
	float						mBrakeImpulse = 0.0f;						///< Amount of impulse that the brakes can apply to the floor (excluding friction)
	float 						mMaxLongitudinalImpulse = 0.f;
};

/// Settings of a vehicle with regular wheels
/// 
/// The properties in this controller are largely based on "Car Physics for Games" by Marco Monster.
/// See: https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html
class WheeledVehicleControllerSettings : public VehicleControllerSettings
{
public:
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(WheeledVehicleControllerSettings)

	// See: VehicleControllerSettings
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const override;
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	VehicleEngineSettings		mEngine;									///< The properties of the engine
	VehicleTransmissionSettings	mTransmission;								///< The properties of the transmission (aka gear box)
	Array<VehicleDifferentialSettings> mDifferentials;						///< List of differentials and their properties
};

/// Runtime controller class
class WheeledVehicleController : public VehicleController
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								WheeledVehicleController(const WheeledVehicleControllerSettings &inSettings, VehicleConstraint &inConstraint);

	/// Typedefs
	using Differentials = Array<VehicleDifferentialSettings>;

	/// Set input from driver
	/// @param inForward Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	/// @param inRight Value between -1 and 1 indicating desired steering angle (1 = right)
	/// @param inBrake Value between 0 and 1 indicating how strong the brake pedal is pressed
	/// @param inHandBrake Value between 0 and 1 indicating how strong the hand brake is pulled
	void						SetDriverInput(float inForward, float inRight, float inBrake, float inHandBrake) { mForwardInput = inForward; mRightInput = inRight; mBrakeInput = inBrake; mHandBrakeInput = inHandBrake; }

	/// Get current engine state
	const VehicleEngine &		GetEngine() const							{ return mEngine; }

	/// Get current engine state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleEngine &				GetEngine()									{ return mEngine; }

	/// Get current transmission state
	const VehicleTransmission &	GetTransmission() const						{ return mTransmission; }

	/// Get current transmission state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleTransmission &		GetTransmission()							{ return mTransmission; }

	/// Get the differentials this vehicle has
	const Differentials &		GetDifferentials() const					{ return mDifferentials; }

	/// Get the differentials this vehicle has (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	Differentials &				GetDifferentials()							{ return mDifferentials; }

#ifdef JPH_DEBUG_RENDERER
	/// Debug drawing of RPM meter
	void						SetRPMMeter(Vec3Arg inPosition, float inSize) { mRPMMeterPosition = inPosition; mRPMMeterSize = inSize; }
#endif // JPH_DEBUG_RENDERER

	// See: VehicleController
	virtual Wheel *				ConstructWheel(WheelSettings &inWheel) const override { JPH_ASSERT(IsKindOf(&inWheel, JPH_RTTI(WheelSettingsWV))); return new WheelWV(static_cast<WheelSettingsWV &>(inWheel)); }
	
protected:
	virtual void				PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) override;
	virtual void				PostCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) override;
	virtual bool				SolveLongitudinalAndLateralConstraints(float inDeltaTime) override;
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				Draw(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER

	// Control information
	float						mForwardInput = 0.0f;						///< Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	float						mRightInput = 0.0f;							///< Value between -1 and 1 indicating desired steering angle
	float						mBrakeInput = 0.0f;							///< Value between 0 and 1 indicating how strong the brake pedal is pressed
	float						mHandBrakeInput = 0.0f;						///< Value between 0 and 1 indicating how strong the hand brake is pulled

	// Simluation information
	VehicleEngine				mEngine;									///< Engine state of the vehicle
	VehicleTransmission			mTransmission;								///< Transmission state of the vehicle
	Differentials				mDifferentials;								///< Differential states of the vehicle

#ifdef JPH_DEBUG_RENDERER
	// Debug settings
	Vec3						mRPMMeterPosition { 0, 1, 0 };				///< Position (in local space of the body) of the RPM meter when drawing the constraint
	float						mRPMMeterSize = 0.5f;						///< Size of the RPM meter when drawing the constraint
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END
