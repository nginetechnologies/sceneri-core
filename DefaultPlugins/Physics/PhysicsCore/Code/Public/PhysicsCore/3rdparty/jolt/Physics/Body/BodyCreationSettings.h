// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Collision/Shape/Shape.h>
#include <Physics/Collision/ObjectLayer.h>
#include <Physics/Collision/CollisionGroup.h>
#include <Physics/Body/MotionType.h>
#include <Physics/Body/MotionQuality.h>
#include <ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

/// Enum used in BodyCreationSettings to indicate how mass and inertia should be calculated
enum class EOverrideMassProperties : uint8
{
	CalculateMassAndInertia, ///< Tells the system to calculate the mass and inertia based on density
	CalculateInertia, ///< Tells the system to take the mass from mMassPropertiesOverride and to calculate the inertia based on density of the
	                  ///< shapes and to scale it to the provided mass
	MassAndInertiaProvided ///< Tells the system to take the mass and inertia from mMassPropertiesOverride
};

/// Settings for constructing a rigid body
class BodyCreationSettings
{
public:
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(BodyCreationSettings)

	/// Constructor
	BodyCreationSettings() = default;
	BodyCreationSettings(
		const ShapeSettings* inShape, Vec3Arg inPosition, QuatArg inRotation, EMotionType inMotionType, ObjectLayer inObjectLayer
	)
		: mPosition(inPosition)
		, mRotation(inRotation)
		, mObjectLayer(inObjectLayer)
		, mMotionType(inMotionType)
		, mShape(inShape)
	{
	}
	BodyCreationSettings(const Shape* inShape, Vec3Arg inPosition, QuatArg inRotation, EMotionType inMotionType, ObjectLayer inObjectLayer)
		: mPosition(inPosition)
		, mRotation(inRotation)
		, mObjectLayer(inObjectLayer)
		, mMotionType(inMotionType)
		, mShapePtr(inShape)
	{
	}

	/// Access to the shape settings object. This contains serializable (non-runtime optimized) information about the Shape.
	const ShapeSettings* GetShapeSettings() const
	{
		return mShape;
	}
	void SetShapeSettings(const ShapeSettings* inShape)
	{
		mShape = inShape;
		mShapePtr = nullptr;
	}

	/// Convert ShapeSettings object into a Shape object. This will free the ShapeSettings object and make the object ready for runtime.
	/// Serialization is no longer possible after this.
	Shape::ShapeResult ConvertShapeSettings();

	/// Access to the run-time shape object. Will convert from ShapeSettings object if needed.
	const Shape* GetShape() const;
	void SetShape(const Shape* inShape)
	{
		mShapePtr = inShape;
		mShape = nullptr;
	}

	/// Check if the mass properties of this body will be calculated (only relevant for kinematic or dynamic objects that need a
	/// MotionProperties object)
	bool HasMassProperties() const
	{
		return mAllowDynamicOrKinematic || mMotionType != EMotionType::Static;
	}

	/// Calculate (or return when overridden) the mass and inertia for this body
	MassProperties GetMassProperties() const;

	/// Saves the state of this object in binary form to inStream. Doesn't store the shape nor the group filter.
	void SaveBinaryState(StreamOut& inStream) const;

	/// Restore the state of this object from inStream. Doesn't restore the shape nor the group filter.
	void RestoreBinaryState(StreamIn& inStream);

	using GroupFilterToIDMap = UnorderedMap<const GroupFilter*, uint32>;
	using IDToGroupFilterMap = Array<RefConst<GroupFilter>>;
	using ShapeToIDMap = Shape::ShapeToIDMap;
	using IDToShapeMap = Shape::IDToShapeMap;
	using MaterialToIDMap = Shape::MaterialToIDMap;
	using IDToMaterialMap = Shape::IDToMaterialMap;

	/// Save this body creation settings, its shape and gropu filter. Pass in an empty map in ioShapeMap / ioMaterialMap / ioGroupFilterMap or
	/// reuse the same map while saving multiple shapes to the same stream in order to avoid writing duplicates. Pass nullptr to ioShapeMap
	/// and ioMaterial map to skip saving shapes Pass nullptr to ioGroupFilterMap to skip saving group filters
	void SaveWithChildren(
		StreamOut& inStream, ShapeToIDMap* ioShapeMap, MaterialToIDMap* ioMaterialMap, GroupFilterToIDMap* ioGroupFilterMap
	) const;

	using BCSResult = Result<BodyCreationSettings>;

	/// Restore a shape, all its children and materials. Pass in an empty map in ioShapeMap / ioMaterialMap / ioGroupFilterMap or reuse the
	/// same map while reading multiple shapes from the same stream in order to restore duplicates.
	static BCSResult
	sRestoreWithChildren(StreamIn& inStream, IDToShapeMap& ioShapeMap, IDToMaterialMap& ioMaterialMap, IDToGroupFilterMap& ioGroupFilterMap);

	Vec3 mPosition = Vec3::sZero();        ///< Position of the body (not of the center of mass)
	Quat mRotation = Quat::sIdentity();    ///< Rotation of the body
	Vec3 mLinearVelocity = Vec3::sZero();  ///< World space linear velocity of the center of mass (m/s)
	Vec3 mAngularVelocity = Vec3::sZero(); ///< World space angular velocity (rad/s)

	/// User data value (can be used by application)
	uint64 mUserData = 0;

	///@name Collision settings
	ObjectLayer mObjectLayer = 0;   ///< The collision layer this body belongs to (determines if two objects can collide)
	CollisionGroup mCollisionGroup; ///< The collision group this body belongs to (determines if two objects can collide)

	///@name Simulation properties
	EMotionType mMotionType = EMotionType::Dynamic; ///< Motion type, determines if the object is static, dynamic or kinematic
	bool mAllowDynamicOrKinematic = false;          ///< When this body is created as static, this setting tells the system to create a
	                                                ///< MotionProperties object so that the object can be switched to kinematic or dynamic
	bool mIsSensor = false; ///< If this body is a sensor. A sensor will receive collision callbacks, but will not cause any collision
	                        ///< responses and can be used as a trigger volume. See description at Body::SetIsSensor.
	EMotionQuality mMotionQuality =
		EMotionQuality::Discrete;        ///< Motion quality, or how well it detects collisions when it has a high velocity
	bool mAllowSleeping = true;        ///< If this body can go to sleep or not
	float mLinearDamping = 0.05f;      ///< Linear damping: dv/dt = -c * v. c must be between 0 and 1 but is usually close to 0.
	float mAngularDamping = 0.05f;     ///< Angular damping: dw/dt = -c * w. c must be between 0 and 1 but is usually close to 0.
	float mMaxLinearVelocity = 500.0f; ///< Maximum linear velocity that this body can reach (m/s)
	float mMaxAngularVelocity = 0.25f * JPH_PI * 60.0f; ///< Maximum angular velocity that this body can reach (rad/s)
	float mGravityFactor = 1.0f;                        ///< Value to multiply gravity with for this body

	///@name Mass properties of the body (by default calculated by the shape)
	EOverrideMassProperties mOverrideMassProperties =
		EOverrideMassProperties::CalculateMassAndInertia; ///< Determines how mMassPropertiesOverride will be used
	float mInertiaMultiplier =
		1.0f; ///< When calculating the inertia (not when it is provided) the calculated inertia will be multiplied by this value
	MassProperties mMassPropertiesOverride; ///< Contains replacement mass settings which override the automatically calculated values
private:
	/// Collision volume for the body
	RefConst<ShapeSettings> mShape; ///< Shape settings, can be serialized. Mutually exclusive with mShapePtr
	RefConst<Shape> mShapePtr;      ///< Actual shape, cannot be serialized. Mutually exclusive with mShape
};

JPH_NAMESPACE_END
