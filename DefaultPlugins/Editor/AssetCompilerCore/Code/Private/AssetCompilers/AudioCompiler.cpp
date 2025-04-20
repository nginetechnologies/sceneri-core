#include "AssetCompilerCore/AssetCompilers/AudioCompiler.h"

#include <AudioCore/AudioAssetType.h>
#include <AudioCore/AudioLoader.h>
#include <AudioCore/Components/SoundSpotComponent.h>

#include <Common/Asset/Asset.h>
#include <Common/Asset/Context.h>
#include <Common/Memory/Containers/String.h>
#include <Common/IO/Path.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/File.h>
#include <Common/IO/Library.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/EnumFlags.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Time/Timestamp.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Guid.h>

namespace ngine::AssetCompiler::Compilers
{
	bool AudioCompiler::IsUpToDate(
		const Platform::Type,
		const Serialization::Data&,
		const Asset::Asset& asset,
		const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context
	)
	{
		const Time::Timestamp sourceFileTimestamp = sourceFilePath.GetLastModifiedTime();

		const IO::PathView targetFileExtension = Audio::AssetFormat.binaryFileExtension;
		const IO::Path targetFilePath = IO::Path::Combine(
			asset.GetDirectory(),
			IO::Path::Merge(asset.GetMetaDataFilePath().GetFileNameWithoutExtensions(), targetFileExtension)
		);
		const Time::Timestamp targetFileTimestamp = targetFilePath.GetLastModifiedTime();

		return targetFileTimestamp >= sourceFileTimestamp && sourceFileTimestamp.IsValid();
	}

	[[nodiscard]] bool CreateAudioFile(IO::Path path, const Audio::Loader& sourceAudio)
	{
		const Audio::Loader::AudioData& audioData = sourceAudio.GetAudioData();
		Audio::FileHeader header;
		header.metaData = sourceAudio.GetMetaData();
		header.dataChunkSize = audioData.GetDataSize();

		bool wasSuccessful = true;
		const IO::File binaryFile(path, IO::AccessModeFlags::WriteBinary);
		wasSuccessful &= binaryFile.Write(header) > 0;
		wasSuccessful &= binaryFile.Write(audioData.GetView()) > 0;

		return wasSuccessful;
	}

	Threading::Job* AudioCompiler::CompileAudio(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& path,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return &Threading::CreateCallback(
			[callback = Forward<CompileCallback>(callback),
		   flags,
		   assetData = Forward<Serialization::Data>(assetData),
		   asset = Forward<Asset::Asset>(asset)](Threading::JobRunnerThread&) mutable
			{
				const IO::Path fullSourcePath = *asset.ComputeAbsoluteSourceFilePath();

				Audio::Loader loader(fullSourcePath);
				if (!loader.IsValid())
				{
					LogError(
						"Couldn't load audio asset {0} from source asset {1}!",
						asset.GetGuid().ToString(),
						fullSourcePath.GetView().GetStringView()
					);
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				if (!loader.TryConvertToSupported())
				{
					// TODO: Print out meta data
					LogError("Source audio asset {0} is in an unsupported format!", fullSourcePath.GetView().GetStringView());
					LogError(
						"Format: {0}, Sample Rate: {1}, Channel Count: {2}",
						GetPCMFormatName(loader.GetMetaData().sourceFormat),
						loader.GetMetaData().sampleRate,
						loader.GetMetaData().channelCount
					);
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				IO::PathView sourceWithoutExtensions = fullSourcePath.GetWithoutExtensions();
				IO::Path newAudioSourcePath = sourceWithoutExtensions + Audio::AssetFormat.binaryFileExtension;
				bool wasSuccessful = CreateAudioFile(newAudioSourcePath, loader);
				if (!wasSuccessful)
				{
					LogError("Failed to create audio binary file at {0}!", newAudioSourcePath.GetView().GetStringView());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				asset.SetComponentTypeGuid(Audio::SoundSpotComponent::TypeGuid);
				asset.SetTypeGuid(Audio::AssetFormat.assetTypeGuid);

				Serialization::Writer writer(assetData);
				asset.Serialize(writer);
				writer.SerializeObjectWithCallback(
					Reflection::GetTypeGuid<Audio::SoundSpotComponent>().ToString().GetView(),
					[&asset](Serialization::Writer serializer) -> bool
					{
						return serializer.Serialize("audio_asset", asset.GetGuid());
					}
				);

				callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			},
			Threading::JobPriority::AssetCompilation
		);
	}
}
