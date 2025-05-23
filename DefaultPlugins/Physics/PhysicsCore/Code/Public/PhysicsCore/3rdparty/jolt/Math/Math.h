// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Common/Memory/CountBits.h>

JPH_NAMESPACE_BEGIN

/// The constant \f$\pi\f$
static constexpr float JPH_PI = 3.14159265358979323846f;

/// Convert a value from degrees to radians
constexpr float DegreesToRadians(float inV)
{
	return inV * (JPH_PI / 180.0f);
}

/// Convert a value from radians to degrees
constexpr float RadiansToDegrees(float inV)
{
	return inV * (180.0f / JPH_PI);
}

/// Convert angle in radians to the range \f$[-\pi, \pi]\f$
inline float CenterAngleAroundZero(float inV)
{
	if (inV < -JPH_PI)
	{
		do
			inV += 2.0f * JPH_PI;
		while (inV < -JPH_PI);
	}
	else if (inV > JPH_PI)
	{
		do
			inV -= 2.0f * JPH_PI;
		while (inV > JPH_PI);
	}
	JPH_ASSERT(inV >= -JPH_PI && inV <= JPH_PI);
	return inV;
}

/// Clamp a value between two values
template <typename T>
constexpr T Clamp(T inV, T inMin, T inMax)
{
	return min(max(inV, inMin), inMax);
}

/// Square a value
template <typename T>
constexpr T Square(T inV)
{
	return inV * inV;
}

/// Returns \f$inV^3\f$.
template <typename T>
constexpr T Cubed(T inV)
{
	return inV * inV * inV;
}

/// Get the sign of a value
template <typename T>
constexpr T Sign(T inV)
{
	return inV < 0? T(-1) : T(1);
}

/// Check if inV is a power of 2
template <typename T>
constexpr bool IsPowerOf2(T inV)
{
	return (inV & (inV - 1)) == 0;
}

/// Align inV up to the next inAlignment bytes
template <typename T>
inline T AlignUp(T inV, uint64 inAlignment)
{
	JPH_ASSERT(IsPowerOf2(inAlignment));
	return T((uint64(inV) + inAlignment - 1) & ~(inAlignment - 1));
}

/// Check if inV is inAlignment aligned
template <typename T>
inline bool IsAligned(T inV, uint64 inAlignment)
{
	JPH_ASSERT(IsPowerOf2(inAlignment));
	return (uint64(inV) & (inAlignment - 1)) == 0;
}

/// Compute number of trailing zero bits (how many low bits are zero)
inline uint CountTrailingZeros(uint32 inValue)
{
	return ngine::Memory::GetNumberOfTrailingZeros(inValue);
}

/// Compute the number of leading zero bits (how many high bits are zero)
inline uint CountLeadingZeros(uint32 inValue)
{
	return ngine::Memory::GetNumberOfLeadingZeros(inValue);
}

/// Count the number of 1 bits in a value
inline uint CountBits(uint32 inValue)
{
	return ngine::Memory::GetNumberOfSetBits(inValue);
}

/// Get the next higher power of 2 of a value, or the value itself if the value is already a power of 2
inline uint32 GetNextPowerOf2(uint32 inValue)
{
	return inValue <= 1? uint32(1) : uint32(1) << (32 - CountLeadingZeros(inValue - 1));
}

JPH_NAMESPACE_END
