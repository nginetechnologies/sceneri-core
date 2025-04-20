#include <Common/Memory/New.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Threading/Jobs/JobManager.h>

#include "gtest/gtest.h"

#include <AssetCompiler/AssetCompilers/Textures/GenericTextureCompiler.h>

namespace ngine::Tests
{
	TEST(AssetCompiler, AssetCompilerLoad)
	{
		ngine::Plugin::EntryPoint pluginEntryPoint = ngine::Plugin::FindEntryPoint(AssetCompiler::Plugin::Guid);
		EXPECT_TRUE(pluginEntryPoint != nullptr);
		UniquePtr<Plugin> pPlugin = UniquePtr<Plugin>(pluginEntryPoint(nullptr));
		EXPECT_TRUE(pPlugin.IsValid());
	}

	/*TEST(AssetCompiler_SVG, AssetCompilerLoad)
	{
	  Threading::JobManager jobManager;

	  ngine::Plugin::EntryPoint pluginEntryPoint = ngine::Plugin::FindEntryPoint(AssetCompiler::Plugin::Guid);
	  UniquePtr<Plugin> pPlugin = UniquePtr<Plugin>(pluginEntryPoint(nullptr));
	  AssetCompiler::Plugin& assetCompiler = static_cast<AssetCompiler::Plugin&>(*pPlugin);

	  ConstUnicodeStringView svgJson = {};
	  Serialization::Data assetData;
	  Asset::Asset asset;

	  auto compileCallback = [](const EnumFlags<AssetCompiler::CompileFlags> compileFlags, ArrayView<Asset::Asset> assets, ArrayView<const
	Serialization::Data> assetsData)
	  {
	    EXPECT_TRUE(compileFlags.IsSet(AssetCompiler::CompileFlags::Compiled));
	    EXPECT_TRUE(assets.GetSize() == 1);
	    EXPECT_TRUE(assetsData.GetSize() == 1);
	  };
	  Threading::Job* pJob = AssetCompiler::Compilers::GenericTextureCompiler::CompileSVG(
	    {},
	    compileCallback,
	    *jobManager.GetCurrentJobThread(),
	    assetCompiler,
	    Platform::Current,

	  );
	}*/
}
