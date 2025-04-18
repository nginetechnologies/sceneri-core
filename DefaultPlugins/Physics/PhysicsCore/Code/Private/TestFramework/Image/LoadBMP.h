// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>

namespace JPH::DebugRendering
{
	class Surface;

	/// Load a windows BMP file
	Ref<Surface> LoadBMP(istream& inStream);

	/// Write a windows BMP file
	bool SaveBMP(RefConst<Surface> inSurface, ostream& inStream);
}
