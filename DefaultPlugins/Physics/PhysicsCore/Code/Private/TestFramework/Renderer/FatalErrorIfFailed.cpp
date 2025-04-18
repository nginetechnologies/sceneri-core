// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "Plugin.h"
#if ENABLE_JOLT_DEBUG_RENDERER

#include <TestFramework/TestFramework.h>

#include <TestFramework/Renderer/FatalErrorIfFailed.h>
#include <Core/StringTools.h>
#include <TestFramework/Utils/Log.h>

namespace JPH::DebugRendering
{
	void FatalErrorIfFailed(HRESULT inHResult)
	{
		if (FAILED(inHResult))
			FatalError("DirectX exception thrown: %s", ConvertToString(inHResult).c_str());
	}
}

#endif
