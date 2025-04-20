#pragma once

#include <Common/IO/Path.h>
#include <Common/Platform/StartProcess.h>
#include <Common/Platform/Type.h>
#include <Common/Platform/GetName.h>

#include <Common/IO/Format/ZeroTerminatedPathView.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Format/Path.h>

namespace ngine::AssetSystem
{
	[[nodiscard]] inline bool CompileFromMetaAsset(
		const Platform::Type platform, const IO::ConstZeroTerminatedPathView assetCompilerPath, const IO::PathView metaAssetPath
	)
	{
		const NativeString platformName(Platform::GetName(platform));

		NativeString commandLine;
		commandLine.Format("{} -platform {} +compile_from_meta_asset {}", assetCompilerPath, platformName, metaAssetPath);
		return Platform::StartProcessAndWaitForFinish(assetCompilerPath, commandLine);
	}
}
