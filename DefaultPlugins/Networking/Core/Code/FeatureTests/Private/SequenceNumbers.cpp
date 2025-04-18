#include <Common/Memory/New.h>

#include <Engine/Tests/FeatureTest.h>

#include <NetworkingCore/LocalPeer.h>

namespace ngine::Tests::Network
{
	using namespace ngine::Network;
	FEATURE_TEST(Networking, SequenceSendWindow)
	{
		using SendWindow = LocalPeer::SendWindow;

		EXPECT_FALSE(SendWindow().GetFirstSentSequenceNumber().IsValid());
		EXPECT_FALSE(SendWindow().GetLastSentSequenceNumber().IsValid());
		EXPECT_FALSE(SendWindow().GetLastAcknowledgedSequenceNumber().IsValid());

		// Test simple usage
		{
			SendWindow sendWindow;
			const Optional<SequenceNumber> newSequenceNumber = sendWindow.GetNewSequenceNumber();
			EXPECT_TRUE(newSequenceNumber.IsValid());
			EXPECT_EQ(newSequenceNumber.Get(), 0);
			EXPECT_EQ(sendWindow.GetSentCount(), 0);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize);

			EXPECT_FALSE(sendWindow.GetFirstSentSequenceNumber().IsValid());
			EXPECT_FALSE(sendWindow.GetLastSentSequenceNumber().IsValid());
			EXPECT_FALSE(sendWindow.GetLastAcknowledgedSequenceNumber().IsValid());
			EXPECT_EQ(*sendWindow.GetNewSequenceNumber(), *newSequenceNumber);
			EXPECT_EQ(sendWindow.GetSentCount(), 0);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize);

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(0), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize - 1), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize * 2), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(1), SendWindow::AcknowledgmentResult::Rejected);

			sendWindow.OnSequenceSent(*newSequenceNumber);
			EXPECT_EQ(sendWindow.GetSentCount(), 1);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize - 1);

			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), *newSequenceNumber);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), *newSequenceNumber);

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(1), SendWindow::AcknowledgmentResult::Rejected);

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(*newSequenceNumber), SendWindow::AcknowledgmentResult::AcceptedLastSentSequence);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(*newSequenceNumber), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.GetSentCount(), 0);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize);

			EXPECT_FALSE(sendWindow.GetFirstSentSequenceNumber().IsValid());
			EXPECT_FALSE(sendWindow.GetLastSentSequenceNumber().IsValid());
			EXPECT_FALSE(sendWindow.GetLastAcknowledgedSequenceNumber().IsValid());
			EXPECT_EQ(*sendWindow.GetNewSequenceNumber(), 1);
		}

		// Test case where we run out of usable / sendablewindow
		{
			SendWindow sendWindow{0, SendWindow::WindowSize};
			EXPECT_EQ(sendWindow.GetSentCount(), SendWindow::WindowSize);
			EXPECT_EQ(sendWindow.GetSendableCount(), 0);
			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), 1);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), SendWindow::WindowSize);
			EXPECT_FALSE(sendWindow.GetNewSequenceNumber().IsValid());

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(0), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize + 1), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize - 1), SendWindow::AcknowledgmentResult::Accepted);
			EXPECT_EQ(sendWindow.GetSentCount(), 1);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize - 1);
			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), SendWindow::WindowSize);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), SendWindow::WindowSize);

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::WindowSize), SendWindow::AcknowledgmentResult::AcceptedLastSentSequence);
			EXPECT_EQ(sendWindow.GetSentCount(), 0);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize);
			EXPECT_FALSE(sendWindow.GetFirstSentSequenceNumber().IsValid());
			EXPECT_FALSE(sendWindow.GetLastSentSequenceNumber().IsValid());
		}

		// Test case where we have sendables in the overlap area
		{
			SendWindow sendWindow{SendWindow::MaximumSequenceNumber - 2, 2};
			EXPECT_EQ(sendWindow.GetSentCount(), 5);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize - 5);
			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), SendWindow::MaximumSequenceNumber - 1);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), 2);
			EXPECT_TRUE(sendWindow.GetNewSequenceNumber().IsValid());

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber - 2), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(3), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber - 1), SendWindow::AcknowledgmentResult::Accepted);
			EXPECT_EQ(sendWindow.GetSentCount(), 4);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize - 4);
			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), SendWindow::MaximumSequenceNumber);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), 2);

			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber - 2), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(SendWindow::MaximumSequenceNumber - 1), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(3), SendWindow::AcknowledgmentResult::Rejected);
			EXPECT_EQ(sendWindow.OnSequenceAcknowledged(1), SendWindow::AcknowledgmentResult::Accepted);
			EXPECT_EQ(sendWindow.GetSentCount(), 1);
			EXPECT_EQ(sendWindow.GetSendableCount(), SendWindow::WindowSize - 1);
			EXPECT_EQ(*sendWindow.GetFirstSentSequenceNumber(), 2);
			EXPECT_EQ(*sendWindow.GetLastSentSequenceNumber(), 2);
		}
	}
}
