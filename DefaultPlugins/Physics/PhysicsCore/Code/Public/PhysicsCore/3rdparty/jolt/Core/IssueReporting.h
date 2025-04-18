// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Common/Assert/Assert.h>

JPH_NAMESPACE_BEGIN
	
extern void Trace(const char* inFMT, ...);
	
// Always turn on asserts in Debug mode
#if ENABLE_ASSERTS
	#define JPH_ENABLE_ASSERTS
#endif

#ifdef JPH_ENABLE_ASSERTS
	#define JPH_ASSERT(inExpression, ...) Assert(inExpression, ##__VA_ARGS__)

	#define JPH_IF_ENABLE_ASSERTS(...)		__VA_ARGS__
#else
    #define JPH_ASSERT(...)					((void)0)

	#define JPH_IF_ENABLE_ASSERTS(...)	
#endif // JPH_ENABLE_ASSERTS

JPH_NAMESPACE_END
