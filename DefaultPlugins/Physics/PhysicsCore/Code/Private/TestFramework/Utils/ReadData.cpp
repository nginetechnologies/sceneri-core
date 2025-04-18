// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "Plugin.h"
#if ENABLE_JOLT_DEBUG_RENDERER

#include <TestFramework/TestFramework.h>
#include <fstream>
#include <TestFramework/Utils/ReadData.h>
#include <TestFramework/Utils/Log.h>

#include <Common/IO/Path.h>
#include <Common/IO/File.h>
#include <Common/EnumFlags.h>
#include <Common/Assert/Assert.h>

using namespace ngine;

namespace JPH::DebugRendering
{
	// Read file contents
	Array<uint8> ReadData(const IO::ConstZeroTerminatedPathView path)
	{
		Array<uint8> data;
		IO::File file(path, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
		if (!file.IsValid())
		{
			Assert(false, "Unable to open file");
			return {};
		}

		data.resize(file.GetSize());
		if (!file.ReadIntoView(ArrayView<uint8>{data.data(), (uint32)data.size()}))
		{
			Assert(false, "Unable to read file");
			return {};
		}
		return data;
	}
}

#endif
