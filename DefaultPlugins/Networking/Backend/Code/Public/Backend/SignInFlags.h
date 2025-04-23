#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/Log2.h>

namespace ngine::Networking::Backend
{
	enum class SignInFlags : uint16
	{
		None,
		//! Signed in with Apple
		Apple = 1 << 0,
		//! Signed in with Google
		Google = 1 << 1,
		//! Signed in with Email
		Email = 1 << 2,
		//! Signed in with Steam
		Steam = 1 << 3,
		//! Signed in with Steam
		Guest = 1 << 4,
		ProviderCount = Math::Log2((uint32)Guest) + 1,
		AllProviders = Apple | Google | Steam | Email | Guest,

		//! Indicates that the sign in succeed, or is supported
		Success = 1 << 5,
		Supported = Success,
		SignedIn = Success,
		//! Indicates that we are offline, operating on cached data
		Offline = 1 << 6,
		//! Indicates that the sign in was cached from a prior session
		Cached = 1 << 7,

		// Internal flags below
		RequestedSignIn = 1 << 8,
		HasSessionToken = 1 << 9,
		IsFirstSession = 1 << 10,
		HasFileToken = 1 << 11,
		HasLocalPlayerProfile = 1 << 12,
		FinishedSessionRegistration = 1 << 13,
		CompletedCoreRequests = HasSessionToken | HasFileToken | HasLocalPlayerProfile | FinishedSessionRegistration,
		InternalFlags = RequestedSignIn | HasSessionToken | IsFirstSession | HasFileToken | HasLocalPlayerProfile | FinishedSessionRegistration,
	};

	[[nodiscard]] ConstStringView GetSignInFlagString(const SignInFlags signInProvider);
	[[nodiscard]] SignInFlags GetSignInFlag(const ConstStringView signInProviderString);

	ENUM_FLAG_OPERATORS(SignInFlags);
}
