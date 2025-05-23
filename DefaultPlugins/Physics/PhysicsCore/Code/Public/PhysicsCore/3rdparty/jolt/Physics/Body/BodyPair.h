// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Body/BodyID.h>
#include <Core/HashCombine.h>

JPH_NAMESPACE_BEGIN

/// Structure that holds a body pair
struct alignas(uint64) BodyPair
{
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							BodyPair() = default;
							BodyPair(BodyID inA, BodyID inB)							: mBodyA(inA), mBodyB(inB) { }

	/// Equals operator
	bool					operator == (const BodyPair &inRHS) const					{ static_assert(sizeof(*this) == sizeof(uint64), "Mismatch in class size"); return *reinterpret_cast<const uint64 *>(this) == *reinterpret_cast<const uint64 *>(&inRHS); }

	/// Smaller than operator, used for consistently ordering body pairs
	bool					operator < (const BodyPair &inRHS) const					{ static_assert(sizeof(*this) == sizeof(uint64), "Mismatch in class size"); return *reinterpret_cast<const uint64 *>(this) < *reinterpret_cast<const uint64 *>(&inRHS); }

	uint64					GetHash() const												{ return HashBytes(this, sizeof(BodyPair)); }

	BodyID					mBodyA;
	BodyID					mBodyB;
};

JPH_NAMESPACE_END
