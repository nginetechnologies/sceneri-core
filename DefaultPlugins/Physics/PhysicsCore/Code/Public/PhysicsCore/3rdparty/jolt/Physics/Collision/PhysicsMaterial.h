// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>
#include <Core/Color.h>
#include <Core/Result.h>
#include <ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

/// This structure describes the surface of (part of) a shape. You should inherit from it to define additional
/// information that is interesting for the simulation. The 2 materials involved in a contact could be used
/// to decide which sound or particle effects to play.
///
/// If you inherit from this material, don't forget to create a suitable default material in sDefault
class PhysicsMaterial : public SerializableObject, public RefTarget<PhysicsMaterial>
{
public:
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(PhysicsMaterial)

	PhysicsMaterial(const float friction = 0.2f, const float restitution = 0.0f, const float density = 1000.f)
		: m_friction(friction)
		, m_restitution(restitution)
		, m_density(density)
	{
	}

	PhysicsMaterial(const PhysicsMaterial& other)
		: m_friction(other.m_friction)
		, m_restitution(other.m_restitution)
		, m_density(other.m_density)
	{
	}
	PhysicsMaterial(PhysicsMaterial&& other)
		: m_friction(other.m_friction)
		, m_restitution(other.m_restitution)
		, m_density(other.m_density)
	{
	}
	PhysicsMaterial& operator=(const PhysicsMaterial& other)
	{
		m_friction = other.m_friction;
		m_restitution = other.m_restitution;
		m_density = other.m_density;
		return *this;
	}
	PhysicsMaterial& operator=(PhysicsMaterial&& other)
	{
		m_friction = other.m_friction;
		m_restitution = other.m_restitution;
		m_density = other.m_density;
		return *this;
	}

	/// Virtual destructor
	virtual ~PhysicsMaterial() override = default;

	/// Default material that is used when a shape has no materials defined
	static RefConst<PhysicsMaterial> sDefault;

	// Properties
	virtual const char* GetDebugName() const
	{
		return "Unknown";
	}
	virtual ColorArg GetDebugColor() const
	{
		return Color::sGrey;
	}

	/// Saves the contents of the material in binary form to inStream.
	virtual void SaveBinaryState(StreamOut& inStream) const;

	using PhysicsMaterialResult = Result<Ref<PhysicsMaterial>>;

	/// Creates a PhysicsMaterial of the correct type and restores its contents from the binary stream inStream.
	static PhysicsMaterialResult sRestoreFromBinaryState(StreamIn& inStream);

	[[nodiscard]] float GetFriction() const
	{
		return m_friction;
	}
	[[nodiscard]] float GetRestitution() const
	{
		return m_restitution;
	}

	/// Get density of the shape (kg / m^3)
	[[nodiscard]] float GetDensity() const
	{
		return m_density;
	}
protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void RestoreBinaryState(StreamIn& inStream);
protected:
	float m_friction = 0.2f;
	float m_restitution = 0.0f;
	float m_density = 1000.f;
};

using PhysicsMaterialList = Array<RefConst<PhysicsMaterial>>;

JPH_NAMESPACE_END
