// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Common/IO/ForwardDeclarations/ZeroTerminatedPathView.h>

namespace JPH::DebugRendering
{
	/// Read file contents into byte vector
	Array<uint8> ReadData(const ngine::IO::ConstZeroTerminatedPathView filePath);
}
