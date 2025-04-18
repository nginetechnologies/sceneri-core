// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

namespace JPH::DebugRendering
{
	/// Convert DirectX error codes to exceptions
	void FatalErrorIfFailed(HRESULT inHResult);
}
