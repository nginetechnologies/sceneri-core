// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "Plugin.h"
#if ENABLE_JOLT_DEBUG_RENDERER

#include <TestFramework/TestFramework.h>
#include <TestFramework/Utils/Log.h>
#include <cstdarg>

namespace JPH::DebugRendering
{
	// Trace to TTY
	void TraceImpl(const char* inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		strcat_s(buffer, "\n");

		// Log to the output window
		OutputDebugStringA(buffer);
	}

	void FatalError [[noreturn]] (const char* inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);

		Trace("Fatal Error: %s", buffer);

		MessageBoxA(nullptr, buffer, "Fatal Error", MB_OK);
		exit(1);
	}
}

#endif
