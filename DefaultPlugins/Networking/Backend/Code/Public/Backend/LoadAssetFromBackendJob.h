#pragma once

#include <Common/Threading/Jobs/Job.h>

#include <Common/Asset/Guid.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>
#include <Common/IO/AsyncLoadCallback.h>
#include <Common/Threading/Jobs/JobPriority.h>
#include <Common/AtomicEnumFlags.h>

#include <Engine/Asset/AssetManager.h>

namespace ngine::Networking::Backend
{
	struct Game;
	struct AssetEntry;

	struct LoadAssetJob final : public Threading::Job
	{
		LoadAssetJob(
			Game& gameAPI,
			const Asset::Guid assetGuid,
			//! The target path within a project
			IO::Path&& targetPath,
			const Priority priority,
			IO::AsyncLoadCallback&& callback,
			const ByteView target = {},
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		);
		~LoadAssetJob();

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final;
		virtual Result OnExecute(Threading::JobRunnerThread& thread) override final;
#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Async Load From Backend";
		}
#endif

		enum class StateFlags : uint16
		{
			AwaitingInitialExecution = 1 << 0,
			AwaitingLoadFromNetwork = 1 << 1,
			AwaitingBackendResponse = 1 << 2,
			AwaitingImportedAssetsBackendResponse = 1 << 3,
			AwaitingBackendFileToken = 1 << 4,
			AwaitingLoadFromDisk = 1 << 5,
			AwaitingFlags = AwaitingInitialExecution | AwaitingLoadFromNetwork | AwaitingBackendResponse | AwaitingImportedAssetsBackendResponse |
			                AwaitingBackendFileToken | AwaitingLoadFromDisk,
			ReceivedLoadFromNetwork = 1 << 6,
			ReceivedBackendResponse = 1 << 7,
			ReceivedLoadFromDisk = 1 << 8,
			ReceivedFlags = ReceivedLoadFromNetwork | ReceivedBackendResponse | ReceivedLoadFromDisk,
		};
	protected:
		void RequestBackendAssetInfo();
		void RequestFileFromNetwork(const AssetEntry& backendAssetEntry, Threading::JobRunnerThread& thread);
		void RequestFileFromDisk(const AssetEntry& backendAssetEntry, Threading::JobRunnerThread& thread);
	protected:
		Game& m_gameAPI;
		AtomicEnumFlags<StateFlags> m_stateFlags{StateFlags::AwaitingInitialExecution};

		Asset::Guid m_assetGuid;
		//! Path to this file where the requester expects it
		IO::Path m_targetPath;
		IO::AsyncLoadCallback m_callback;
		const ByteView m_target;
		const Math::Range<size> m_dataRange;
	};

	ENUM_FLAG_OPERATORS(LoadAssetJob::StateFlags);
}
