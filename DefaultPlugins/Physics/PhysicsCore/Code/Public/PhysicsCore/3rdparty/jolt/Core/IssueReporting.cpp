// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <fstream>
#include <iostream>
#include <stdarg.h>
JPH_SUPPRESS_WARNINGS_STD_END

#include "IssueReporting.h"

JPH_NAMESPACE_BEGIN

JPH_SUPPRESS_WARNING_PUSH
JPH_CLANG_SUPPRESS_WARNING("-Wformat-nonliteral")

// Callback for traces, connect this to your own trace function if you have one
void Trace(const char* inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);

	// Print to the TTY
	std::cout << buffer << std::endl;
}

JPH_SUPPRESS_WARNING_POP

JPH_NAMESPACE_END
