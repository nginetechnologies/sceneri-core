// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Collision/Shape/ConvexShape.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

/// Class that constructs a TaperedCapsuleShape
class TaperedCapsuleShapeSettings final : public ConvexShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(TaperedCapsuleShapeSettings)

	/// Default constructor for deserialization
							TaperedCapsuleShapeSettings() = default;

	/// Create a tapered capsule centered around the origin with one sphere cap at (0, -inHalfHeightOfTaperedCylinder, 0) with radius inBottomRadius and the other at (0, inHalfHeightOfTaperedCylinder, 0) with radius inTopRadius
							TaperedCapsuleShapeSettings(float inHalfHeightOfTaperedCylinder, float inTopRadius, float inBottomRadius, const PhysicsMaterial *inMaterial = nullptr);

	/// Check if the settings are valid
	bool					IsValid() const															{ return mTopRadius > 0.0f && mBottomRadius > 0.0f && mHalfHeightOfTaperedCylinder >= 0.0f; }

	/// Checks if the settings of this tapered capsule make this shape a sphere
	bool					IsSphere() const;

	// See: ShapeSettings
	virtual ShapeResult		Create() const override;

	float					mHalfHeightOfTaperedCylinder = 0.0f;
	float					mTopRadius = 0.0f;
	float					mBottomRadius = 0.0f;
};

/// A capsule with different top and bottom radii
class TaperedCapsuleShape final : public ConvexShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							TaperedCapsuleShape() : ConvexShape(EShapeSubType::TaperedCapsule) { }
							TaperedCapsuleShape(const TaperedCapsuleShapeSettings &inSettings, ShapeResult &outResult);

	// See Shape::GetCenterOfMass
	virtual Vec3			GetCenterOfMass() const override										{ return mCenterOfMass; }

	// See Shape::GetLocalBounds
	virtual AABox			GetLocalBounds() const override;

	// See Shape::GetWorldSpaceBounds
	virtual AABox			GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const override;

	// See Shape::GetInnerRadius
	virtual float			GetInnerRadius() const override											{ return min(mTopRadius, mBottomRadius); }

	// See Shape::GetMassProperties
	virtual MassProperties	GetMassProperties(const float density) const override;
	virtual MassProperties	GetMassProperties() const override;

	// See Shape::GetSurfaceNormal
	virtual Vec3			GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See ConvexShape::GetSupportFunction
	virtual const Support *	GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const override;

	// See ConvexShape::GetSupportingFace
	virtual void			GetSupportingFace(Vec3Arg inDirection, Vec3Arg inScale, SupportingFace &outVertices) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void			Draw(DebugRenderer *inRenderer, Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape::TransformShape
	virtual void			TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const override;

	// See Shape
	virtual void			SaveBinaryState(StreamOut &inStream) const override;

	// See Shape::GetStats
	virtual Stats			GetStats() const override												{ return Stats(sizeof(*this), 0); } 

	// See Shape::GetVolume
	virtual float			GetVolume() const override												{ return GetLocalBounds().GetVolume(); } // Volume is approximate!

	// See Shape::IsValidScale
	virtual bool			IsValidScale(Vec3Arg inScale) const override;

	// Register shape functions with the registry
	static void				sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void			RestoreBinaryState(StreamIn &inStream) override;

private:
	// Class for GetSupportFunction
	class					TaperedCapsule;

	/// Returns box that approximates the inertia
	AABox					GetInertiaApproximation() const;

	Vec3					mCenterOfMass = Vec3::sZero();
	float					mTopRadius = 0.0f;
	float					mBottomRadius = 0.0f;
	float					mTopCenter = 0.0f;
	float					mBottomCenter = 0.0f;
	float					mConvexRadius = 0.0f;
	float					mSinAlpha = 0.0f;
	float					mTanAlpha = 0.0f;

#ifdef JPH_DEBUG_RENDERER
	mutable DebugRenderer::GeometryRef mGeometry;
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END
