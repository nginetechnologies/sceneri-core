// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Vehicle/VehicleConstraint.h>
#include <ObjectStream/SerializableObject.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

class VehicleController;

/// Basic settings object for interface that controls acceleration / decelleration of the vehicle
class VehicleControllerSettings : public SerializableObject, public RefTarget<VehicleControllerSettings>
{
public:
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(VehicleControllerSettings)

	/// Saves the contents of the controller settings in binary form to inStream.
	virtual void				SaveBinaryState(StreamOut &inStream) const = 0;

	/// Restore the contents of the controller settings in binary form from inStream.
	virtual void				RestoreBinaryState(StreamIn &inStream) = 0;

	/// Create an instance of the vehicle controller class
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const = 0;
};

/// Runtime data for interface that controls acceleration / decelleration of the vehicle
class VehicleController : public RefTarget<VehicleController>
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor / destructor
	explicit					VehicleController(VehicleConstraint &inConstraint) : mConstraint(inConstraint) { }
	virtual						~VehicleController() = default;

	// Create a new instance of wheel
	virtual Wheel *				ConstructWheel(WheelSettings &inWheel) const = 0;

protected:
	// The functions below are only for the VehicleConstraint
	friend class VehicleConstraint;

	// Called before the wheel probes have been done
	virtual void				PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) = 0;

	// Called after the wheel probes have been done
	virtual void				PostCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) = 0;

	// Solve longitudinal and lateral constraint parts for all of the wheels
	virtual bool				SolveLongitudinalAndLateralConstraints(float inDeltaTime) = 0;

	// Saving state for replay
	virtual void				SaveState(StateRecorder &inStream) const = 0;
	virtual void				RestoreState(StateRecorder &inStream) = 0;

#ifdef JPH_DEBUG_RENDERER
	// Drawing interface
	virtual void				Draw(DebugRenderer *inRenderer) const = 0;
#endif // JPH_DEBUG_RENDERER

	VehicleConstraint &			mConstraint;								///< The vehicle constraint we belong to
};

JPH_NAMESPACE_END
