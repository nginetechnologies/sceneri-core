#pragma once

#include <Http/FileCacheState.h>

#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Asset/Guid.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>
#include <Common/IO/AsyncLoadCallback.h>
#include <Common/Function/Function.h>

namespace ngine::Networking::HTTP
{
	struct Plugin;

	// Ensure that entity tags are all stored uniformly without strings
	[[nodiscard]] inline ConstStringView FilterEntityTag(ConstStringView tag)
	{
		if (tag.StartsWith("W/"))
		{
			tag += 2;
		}
		if (tag.GetSize() > 2)
		{
			tag += (tag[0] == '\"') | (tag[0] == '\'');
			tag -= (tag.GetLastElement() == '\"') | (tag.GetLastElement() == '\'');
		}
		return tag;
	}

	struct LoadAssetFromNetworkJob final : public Threading::Job
	{
		enum class Flags : uint8
		{
			SkipExistingFilesValidation = 1 << 0
		};

		LoadAssetFromNetworkJob(
			const Asset::Guid assetGuid,
			//! The target path within a project
			IO::Path&& localPath,
			//! The URI pointing to the resource on the network
			IO::URI&& remoteURI,
			const Priority priority,
			IO::AsyncLoadCallback&& callback,
			const EnumFlags<Flags> flags = {},
			const ByteView target = {},
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		);

		LoadAssetFromNetworkJob(
			const Asset::Guid assetGuid,
			//! The target path within a project
			const IO::PathView localPath,
			const IO::PathView rootLocalPath,
			//! The URI pointing to the resource on the network
			const IO::URIView rootRemoteURI,
			const IO::URIView uriQueryString,
			const Priority priority,
			IO::AsyncLoadCallback&& callback,
			const EnumFlags<Flags> flags = {},
			const ByteView target = {},
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		);

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final;
		virtual Result OnExecute(Threading::JobRunnerThread& thread) override final;
#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Async Load From Network";
		}
#endif

		struct FileInfo
		{
			IO::Path path;
			Math::Range<size> range;
		};

		using OngoingValidityRequestCallback = Function<void(const FileCacheState), 24>;

		void CheckFileCacheValidityAsync(const IO::URI& uri, Networking::HTTP::Plugin& httpPlugin, OngoingValidityRequestCallback&& callback);
	protected:
		void QueueLoadLocalFile();
		[[nodiscard]] bool QueueLoadCachedFile();
		void QueueDownloadRemoteFile();

		void OnDataReceived(const ConstByteView fileData);
	protected:
		enum class State : uint8
		{
			AwaitingInitialExecution,
			AwaitingFileCacheValidityCheck,
			AwaitingLocalFileStreamFinish,
			AwaitingPartialCachedFileStreamFinish,
			AwaitingCompleteCachedFileStreamFinish,
			AwaitingRemoteFileDownloadFinish,
		};
		State m_state = State::AwaitingInitialExecution;

		Asset::Guid m_assetGuid;
		//! Path to this file where the requester expects it
		IO::Path m_localPath;
		IO::URI m_remoteURI;
		IO::AsyncLoadCallback m_callback;
		EnumFlags<Flags> m_flags;
		const ByteView m_target;
		const Math::Range<size> m_dataRange;
	};

	ENUM_FLAG_OPERATORS(LoadAssetFromNetworkJob::Flags);
}
