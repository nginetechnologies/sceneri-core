// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Collision/Shape/CylinderShape.h>
#include <Physics/Collision/Shape/ScaleHelpers.h>
#include <Physics/Collision/Shape/GetTrianglesContext.h>
#include <Physics/Collision/RayCast.h>
#include <Physics/Collision/CastResult.h>
#include <Physics/Collision/CollidePointResult.h>
#include <Physics/Collision/TransformedShape.h>
#include <Geometry/RayCylinder.h>
#include <ObjectStream/TypeDeclarations.h>
#include <Core/StreamIn.h>
#include <Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(CylinderShapeSettings)
{
	JPH_ADD_BASE_CLASS(CylinderShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(CylinderShapeSettings, mHalfHeight)
	JPH_ADD_ATTRIBUTE(CylinderShapeSettings, mRadius)
	JPH_ADD_ATTRIBUTE(CylinderShapeSettings, mConvexRadius)
}

// Approximation of top face with 8 vertices
static const float cSin45 = 0.70710678118654752440084436210485f;
static const Vec3 cTopFace[] = {
	Vec3(0.0f, 1.0f, 1.0f),
	Vec3(cSin45, 1.0f, cSin45),
	Vec3(1.0f, 1.0f, 0.0f),
	Vec3(cSin45, 1.0f, -cSin45),
	Vec3(-0.0f, 1.0f, -1.0f),
	Vec3(-cSin45, 1.0f, -cSin45),
	Vec3(-1.0f, 1.0f, 0.0f),
	Vec3(-cSin45, 1.0f, cSin45)};

static const vector<Vec3> sUnitCylinderTriangles = []()
{
	vector<Vec3> verts;

	const Vec3 bottom_offset(0.0f, -2.0f, 0.0f);

	int num_verts = sizeof(cTopFace) / sizeof(Vec3);
	for (int i = 0; i < num_verts; ++i)
	{
		Vec3 t1 = cTopFace[i];
		Vec3 t2 = cTopFace[(i + 1) % num_verts];
		Vec3 b1 = cTopFace[i] + bottom_offset;
		Vec3 b2 = cTopFace[(i + 1) % num_verts] + bottom_offset;

		// Top
		verts.push_back(Vec3(0.0f, 1.0f, 0.0f));
		verts.push_back(t1);
		verts.push_back(t2);

		// Bottom
		verts.push_back(Vec3(0.0f, -1.0f, 0.0f));
		verts.push_back(b2);
		verts.push_back(b1);

		// Side
		verts.push_back(t1);
		verts.push_back(b1);
		verts.push_back(t2);

		verts.push_back(t2);
		verts.push_back(b1);
		verts.push_back(b2);
	}

	return verts;
}();

ShapeSettings::ShapeResult CylinderShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new CylinderShape(*this, mCachedResult);
	return mCachedResult;
}

CylinderShape::CylinderShape(const CylinderShapeSettings& inSettings, ShapeResult& outResult)
	: ConvexShape(EShapeSubType::Cylinder, inSettings, outResult)
	, mHalfHeight(inSettings.mHalfHeight)
	, mRadius(inSettings.mRadius)
	, mConvexRadius(inSettings.mConvexRadius)
{
	if (inSettings.mHalfHeight < inSettings.mConvexRadius)
	{
		outResult.SetError("Invalid height");
		return;
	}

	if (inSettings.mRadius < inSettings.mConvexRadius)
	{
		outResult.SetError("Invalid radius");
		return;
	}

	if (inSettings.mConvexRadius < 0.0f)
	{
		outResult.SetError("Invalid convex radius");
		return;
	}

	outResult.Set(this);
}

CylinderShape::CylinderShape(float inHalfHeight, float inRadius, float inConvexRadius, const PhysicsMaterial* inMaterial)
	: ConvexShape(EShapeSubType::Cylinder, inMaterial)
	, mHalfHeight(inHalfHeight)
	, mRadius(inRadius)
	, mConvexRadius(inConvexRadius)
{
	JPH_ASSERT(inHalfHeight >= inConvexRadius);
	JPH_ASSERT(inRadius >= inConvexRadius);
	JPH_ASSERT(inConvexRadius >= 0.0f);
}

class CylinderShape::Cylinder final : public Support
{
public:
	Cylinder(float inHalfHeight, float inRadius, float inConvexRadius)
		: mHalfHeight(inHalfHeight)
		, mRadius(inRadius)
		, mConvexRadius(inConvexRadius)
	{
		static_assert(sizeof(Cylinder) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(Cylinder)));
	}

	virtual Vec3 GetSupport(Vec3Arg inDirection) const override
	{
		// Support mapping, taken from:
		// A Fast and Robust GJK Implementation for Collision Detection of Convex Objects - Gino van den Bergen
		// page 8
		float x = inDirection.GetX(), y = inDirection.GetY(), z = inDirection.GetZ();
		float o = sqrt(Square(x) + Square(z));
		if (o > 0.0f)
			return Vec3((mRadius * x) / o, Sign(y) * mHalfHeight, (mRadius * z) / o);
		else
			return Vec3(0, Sign(y) * mHalfHeight, 0);
	}

	virtual float GetConvexRadius() const override
	{
		return mConvexRadius;
	}
private:
	float mHalfHeight;
	float mRadius;
	float mConvexRadius;
};

const ConvexShape::Support* CylinderShape::GetSupportFunction(ESupportMode inMode, SupportBuffer& inBuffer, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	// Get scaled cylinder
	Vec3 abs_scale = inScale.Abs();
	float scale_xz = abs_scale.GetX();
	float scale_y = abs_scale.GetY();
	float scaled_half_height = scale_y * mHalfHeight;
	float scaled_radius = scale_xz * mRadius;
	float scaled_convex_radius = ScaleHelpers::ScaleConvexRadius(mConvexRadius, inScale);

	switch (inMode)
	{
		case ESupportMode::IncludeConvexRadius:
			return new (&inBuffer) Cylinder(scaled_half_height, scaled_radius, 0.0f);

		case ESupportMode::ExcludeConvexRadius:
			return new (&inBuffer)
				Cylinder(scaled_half_height - scaled_convex_radius, scaled_radius - scaled_convex_radius, scaled_convex_radius);
	}

	JPH_ASSERT(false);
	return nullptr;
}

void CylinderShape::GetSupportingFace(Vec3Arg inDirection, Vec3Arg inScale, SupportingFace& outVertices) const
{
	JPH_ASSERT(IsValidScale(inScale));

	// Get scaled cylinder
	Vec3 abs_scale = inScale.Abs();
	float scale_xz = abs_scale.GetX();
	float scale_y = abs_scale.GetY();
	float scaled_half_height = scale_y * mHalfHeight;
	float scaled_radius = scale_xz * mRadius;

	float x = inDirection.GetX(), y = inDirection.GetY(), z = inDirection.GetZ();
	float o = sqrt(Square(x) + Square(z));

	// If o / |y| > scaled_radius / scaled_half_height, we're hitting the side
	if (o * scaled_half_height > scaled_radius * abs(y))
	{
		// Hitting side
		float f = -scaled_radius / o;
		float vx = x * f;
		float vz = z * f;
		outVertices.push_back(Vec3(vx, scaled_half_height, vz));
		outVertices.push_back(Vec3(vx, -scaled_half_height, vz));
	}
	else
	{
		// Hitting top or bottom
		Vec3 multiplier = y < 0.0f ? Vec3(scaled_radius, scaled_half_height, scaled_radius)
		                           : Vec3(-scaled_radius, -scaled_half_height, scaled_radius);
		for (const Vec3& v : cTopFace)
			outVertices.push_back(multiplier * v);
	}
}

MassProperties CylinderShape::GetMassProperties(const float density) const
{
	MassProperties p;

	// Mass is surface of circle * height
	float radius_sq = Square(mRadius);
	float height = 2.0f * mHalfHeight;
	p.mMass = JPH_PI * radius_sq * height * density;

	// Inertia according to https://en.wikipedia.org/wiki/List_of_moments_of_inertia:
	float inertia_y = radius_sq * p.mMass * 0.5f;
	float inertia_x = inertia_y * 0.5f + p.mMass * height * height / 12.0f;
	float inertia_z = inertia_x;

	// Set inertia
	p.mInertia = Mat44::sScale(Vec3(inertia_x, inertia_y, inertia_z));

	return p;
}

MassProperties CylinderShape::GetMassProperties() const
{
    return GetMassProperties(GetMaterial()->GetDensity());
}

Vec3 CylinderShape::GetSurfaceNormal(const SubShapeID& inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	// Calculate distance to infinite cylinder surface
	Vec3 local_surface_position_xz(inLocalSurfacePosition.GetX(), 0, inLocalSurfacePosition.GetZ());
	float local_surface_position_xz_len = local_surface_position_xz.Length();
	float distance_to_curved_surface = abs(local_surface_position_xz_len - mRadius);

	// Calculate distance to top or bottom plane
	float distance_to_top_or_bottom = abs(abs(inLocalSurfacePosition.GetY()) - mHalfHeight);

	// Return normal according to closest surface
	if (distance_to_curved_surface < distance_to_top_or_bottom)
		return local_surface_position_xz / local_surface_position_xz_len;
	else
		return inLocalSurfacePosition.GetY() > 0.0f ? Vec3::sAxisY() : -Vec3::sAxisY();
}

AABox CylinderShape::GetLocalBounds() const
{
	Vec3 extent = Vec3(mRadius, mHalfHeight, mRadius);
	return AABox(-extent, extent);
}

#ifdef JPH_DEBUG_RENDERER
void CylinderShape::Draw(
	DebugRenderer* inRenderer,
	Mat44Arg inCenterOfMassTransform,
	Vec3Arg inScale,
	ColorArg inColor,
	bool inUseMaterialColors,
	bool inDrawWireframe
) const
{
	DebugRenderer::EDrawMode draw_mode = inDrawWireframe ? DebugRenderer::EDrawMode::Wireframe : DebugRenderer::EDrawMode::Solid;
	inRenderer->DrawCylinder(
		inCenterOfMassTransform * Mat44::sScale(inScale.Abs()),
		mHalfHeight,
		mRadius,
		inUseMaterialColors ? GetMaterial()->GetDebugColor() : inColor,
		DebugRenderer::ECastShadow::On,
		draw_mode
	);
}
#endif // JPH_DEBUG_RENDERER

bool CylinderShape::CastRay(const RayCast& inRay, const SubShapeIDCreator& inSubShapeIDCreator, RayCastResult& ioHit) const
{
	// Test ray against capsule
	float fraction = RayCylinder(inRay.mOrigin, inRay.mDirection, mHalfHeight, mRadius);
	if (fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void CylinderShape::CollidePoint(
	Vec3Arg inPoint, const SubShapeIDCreator& inSubShapeIDCreator, CollidePointCollector& ioCollector, const ShapeFilter& inShapeFilter
) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(inSubShapeIDCreator.GetID()))
		return;

	// Check if the point is in the cylinder
	if (abs(inPoint.GetY()) <= mHalfHeight											// Within the height
		&& Square(inPoint.GetX()) + Square(inPoint.GetZ()) <= Square(mRadius))		// Within the radius
		ioCollector.AddHit({TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID()});
}

void CylinderShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector& ioCollector) const
{
	Vec3 scale;
	Mat44 transform = inCenterOfMassTransform.Decompose(scale);
	TransformedShape ts(transform.GetTranslation(), transform.GetRotation().GetQuaternion(), this, BodyID(), SubShapeIDCreator());
	Vec3 abs_scale = scale.Abs();
	float xz = 0.5f * (abs_scale.GetX() + abs_scale.GetZ());
	ts.SetShapeScale(Vec3(xz, abs_scale.GetY(), xz));
	ioCollector.AddHit(ts);
}

void CylinderShape::GetTrianglesStart(
	GetTrianglesContext& ioContext, const AABox& inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale
) const
{
	Mat44 unit_cylinder_transform(Vec4(mRadius, 0, 0, 0), Vec4(0, mHalfHeight, 0, 0), Vec4(0, 0, mRadius, 0), Vec4(0, 0, 0, 1));
	new (&ioContext) GetTrianglesContextVertexList(
		inPositionCOM,
		inRotation,
		inScale,
		unit_cylinder_transform,
		sUnitCylinderTriangles.data(),
		sUnitCylinderTriangles.size(),
		GetMaterial()
	);
}

int CylinderShape::GetTrianglesNext(
	GetTrianglesContext& ioContext, int inMaxTrianglesRequested, Float3* outTriangleVertices, const PhysicsMaterial** outMaterials
) const
{
	return ((GetTrianglesContextVertexList&)ioContext).GetTrianglesNext(inMaxTrianglesRequested, outTriangleVertices, outMaterials);
}

void CylinderShape::SaveBinaryState(StreamOut& inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mHalfHeight);
	inStream.Write(mRadius);
	inStream.Write(mConvexRadius);
}

void CylinderShape::RestoreBinaryState(StreamIn& inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mHalfHeight);
	inStream.Read(mRadius);
	inStream.Read(mConvexRadius);
}

bool CylinderShape::IsValidScale(Vec3Arg inScale) const
{
	// X and Z need same scale
	Vec3 abs_scale = inScale.Abs();
	return ConvexShape::IsValidScale(inScale) &&
	       abs_scale.Swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>().IsClose(abs_scale, ScaleHelpers::cScaleToleranceSq);
}

void CylinderShape::sRegister()
{
	ShapeFunctions& f = ShapeFunctions::sGet(EShapeSubType::Cylinder);
	f.mConstruct = []() -> Shape*
	{
		return new CylinderShape;
	};
	f.mColor = Color::sGreen;
}

JPH_NAMESPACE_END
