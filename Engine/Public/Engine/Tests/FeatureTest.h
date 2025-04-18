#pragma once

#include <Engine/EngineSystems.h>

#include <Common/Tests/UnitTest.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Tests
{
	struct FeatureTest : public UnitTest
	{
		static void SetUpTestSuite()
		{
			UnitTest::SetUpTestSuite();

			if (!GetEngineSystems().IsValid())
			{
				GetEngineSystems() = UniquePtr<EngineSystems>::Make(CommandLine::InitializationParameters{});
				Threading::JobBatch loadDefaultResourcesBatch = GetEngineSystems()->m_engine.LoadDefaultResources();

				Threading::Atomic<bool> finished { false };
				loadDefaultResourcesBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[&finished](Threading::JobRunnerThread&)
					{
						finished = true;
					},
					Threading::JobPriority::LoadPlugin
				));
				GetEngineSystems()->m_startupJobBatch.QueueAfterStartStage(loadDefaultResourcesBatch);
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
				thread.Queue(GetEngineSystems()->m_startupJobBatch);
				while (!finished)
				{
					thread.DoRunNextJob();
				}
			}
		}

		static void TearDownTestSuite()
		{
			UnitTest::TearDownTestSuite();

			GetEngine().Quit();
			GetEngineSystems().DestroyElement();
		}

		// Per-test setup
		void SetUp() override
		{
			UnitTest::SetUp();
		}

		// Per-test cleanup
		void TearDown() override
		{
			UnitTest::TearDown();
		}

		[[nodiscard]] static Engine& GetEngine()
		{
			return GetEngineSystems()->m_engine;
		}

		template<typename ConditionCallback>
		void RunMainThreadJobRunner(ConditionCallback&& callback)
		{
			while (callback())
			{
				GetEngine().DoTick();
			}
		}
	protected:
		[[nodiscard]] static UniquePtr<EngineSystems>& GetEngineSystems()
		{
			static UniquePtr<EngineSystems> pEngineSystems;
			return pEngineSystems;
		}
	};

#define FEATURE_TEST(category, name) \
	GTEST_TEST_(category, name, ngine::Tests::FeatureTest, ::testing::internal::GetTypeId<ngine::Tests::FeatureTest>())
}
