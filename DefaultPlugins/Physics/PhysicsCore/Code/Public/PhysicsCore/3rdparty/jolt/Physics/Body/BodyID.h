// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <PhysicsCore/3rdparty/jolt/Core/Core.h>

JPH_NAMESPACE_BEGIN

using BodyIdentifier = ngine::TIdentifier<uint32_t, 24, 65535>;
using BodyMask = ngine::IdentifierMask<BodyIdentifier>;

/// ID of a body. This is a way of reasoning about bodies in a multithreaded simulation while avoiding race conditions.
class BodyID : public BodyIdentifier
{
public:
	static constexpr uint32	cBroadPhaseBit = 0x00800000;	///< This bit is used by the broadphase
	static constexpr uint8	cMaxSequenceNumber = 0xff;		///< Maximum value for the sequence number
	static constexpr uint32	cMaxBodyIndex = 0x7fffff;		///< Maximum value for body index (also the maximum amount of bodies supported - 1)

	using BodyIdentifier::BodyIdentifier;
	using BodyIdentifier::operator=;
	BodyID(const BodyIdentifier identifier)
		: BodyIdentifier(identifier)
	{
	}
	BodyID(const BodyID& identifier) = default;
	BodyID& operator=(const BodyID&) = default;
	explicit BodyID(const typename BodyIdentifier::InternalType value)
		: BodyIdentifier(BodyIdentifier::MakeFromValue(value))
	{
	}

	/// Get index in body array
	inline uint32			GetIndex() const
	{
		return BodyIdentifier::GetFirstValidIndex() & cMaxBodyIndex;
	}

	/// Get sequence number of body.
	/// The sequence number can be used to check if a body ID with the same body index has been reused by another body.
	/// It is mainly used in multi threaded situations where a body is removed and its body index is immediately reused by a body created from another thread.
	/// Functions querying the broadphase can (after aquiring a body lock) detect that the body has been removed (we assume that this won't happen more than 128 times in a row).
	inline typename BodyIdentifier::IndexReuseType	GetSequenceNumber() const
	{
		return BodyIdentifier::GetIndexUseCount();
	}

	/// Returns the index and sequence number combined in an uint32
	inline typename BodyIdentifier::InternalType	GetIndexAndSequenceNumber() const
	{
		return BodyIdentifier::GetValue();
	}
};

JPH_NAMESPACE_END
