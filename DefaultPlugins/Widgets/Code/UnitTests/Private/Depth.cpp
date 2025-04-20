#include <Common/Memory/New.h>

#include <Common/Tests/UnitTest.h>

#include <Widgets/DepthValue.h>
#include <Widgets/DepthInfo.h>

namespace ngine::Widgets
{
	inline static constexpr double TestMinimumDepth = 0.0;
	inline static constexpr double TestMaximumDepth = 1000000.0;

	[[nodiscard]] bool operator<(const DepthValue left, const DepthValue right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) < right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator<=(const DepthValue left, const DepthValue right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) <= right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator>(const DepthValue left, const DepthValue right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) > right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator>=(const DepthValue left, const DepthValue right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) >= right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}

	[[nodiscard]] bool operator<(const DepthInfo left, const DepthInfo right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) < right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator<=(const DepthInfo left, const DepthInfo right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) <= right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator>(const DepthInfo left, const DepthInfo right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) > right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
	[[nodiscard]] bool operator>=(const DepthInfo left, const DepthInfo right)
	{
		return left.GetDepthRatio(TestMinimumDepth, TestMaximumDepth) >= right.GetDepthRatio(TestMinimumDepth, TestMaximumDepth);
	}
}

namespace ngine::Widgets::Tests
{
	UNIT_TEST(Widgets, DepthValueHierarchyIndex)
	{
		// Simple case, one element in front of the other
		{
			DepthValue back;
			back.m_value = 0;
			back.m_computedDepthIndex = 0;
			DepthValue front;
			front.m_value = 0;
			front.m_computedDepthIndex = back.m_computedDepthIndex + 1;
			EXPECT_GT(front, back);
		}
		// Case with three elements
		{
			DepthValue back;
			back.m_value = 0;
			back.m_computedDepthIndex = 0;

			DepthValue middle;
			middle.m_value = 0;
			middle.m_computedDepthIndex = back.m_computedDepthIndex + 1;

			DepthValue front;
			front.m_value = 0;
			front.m_computedDepthIndex = middle.m_computedDepthIndex + 1;
			EXPECT_GT(middle, back);
			EXPECT_GT(front, middle);
			EXPECT_GT(front, back);
		}
	}

	/*
	UNIT_TEST(Widgets, DepthScopeIndexAndHierarchy)
	{
	  DepthValue root;
	  root.m_value = 0;
	  root.m_scopeIndex = 0;
	  root.m_computedDepthIndex = 0;
	  root.m_siblingIndex = 0;

	  DepthValue div1;
	  div1.m_value = 0;
	  div1.m_scopeIndex = 0;
	  div1.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div1.m_siblingIndex = 0;
	  EXPECT_GT(div1.m_value, root.m_value);

	  DepthValue div3;
	  div3.m_value = 0;
	  div3.m_scopeIndex = 0;
	  div3.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div3.m_siblingIndex = div1.m_siblingIndex + 1;
	  EXPECT_GT(div3.m_value, root.m_value);
	  EXPECT_GT(div3.m_value, div1.m_value);

	  DepthValue div4;
	  div4.m_value = 0;
	  div4.m_scopeIndex = div3.m_scopeIndex + 1;
	  div4.m_siblingIndex = 0;
	  div4.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  EXPECT_GT(div4.m_value, div1.m_value);
	}

	UNIT_TEST(Widgets, DepthScopeIndexAndHierarchyWithZIndex)
	{
	  DepthValue root;
	  root.m_value = 0;
	  root.m_scopeIndex = 0;
	  root.m_computedDepthIndex = 0;
	  root.m_siblingIndex = 0;
	  root.m_zeroZIndexValue = 1;

	  DepthValue div1;
	  div1.m_value = 0;
	  div1.m_scopeIndex = 0;
	  div1.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div1.m_siblingIndex = 0;
	  div1.m_positiveZIndexValue = 1;

	  DepthValue div3;
	  div3.m_value = 0;
	  div3.m_scopeIndex = 0;
	  div3.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div3.m_siblingIndex = div1.m_siblingIndex + 1;
	  div3.m_zeroZIndexValue = 1;
	  EXPECT_GT(div1.m_value, div3.m_value);

	  DepthValue div4;
	  div4.m_value = 0;
	  div4.m_scopeIndex = div1.m_scopeIndex + 1;
	  div4.m_siblingIndex = 0;
	  div4.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div4.m_zeroZIndexValue = 1;
	  EXPECT_GT(div4.m_value, div3.m_value);
	  EXPECT_GT(div4.m_value, root.m_value);
	  EXPECT_GT(div1.m_value, div4.m_value);
	}

	UNIT_TEST(Widgets, DepthScopeIndexHierarchyZIndex)
	{
	  DepthValue root;
	  root.m_value = 0;
	  root.m_scopeIndex = 0;
	  root.m_computedDepthIndex = 0;
	  root.m_siblingIndex = 0;
	  root.m_zeroZIndexValue = 1;

	  DepthValue div3;
	  div3.m_value = 0;
	  div3.m_scopeIndex = 0;
	  div3.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div3.m_siblingIndex = 0;
	  div3.m_positiveZIndexValue = 1;

	  DepthValue div4;
	  div4.m_value = 0;
	  div4.m_scopeIndex = div3.m_scopeIndex + 1;
	  div4.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div4.m_siblingIndex = 0;
	  div4.m_zeroZIndexValue = 1;

	  DepthValue div1;
	  div1.m_value = 0;
	  div1.m_scopeIndex = 0;
	  div1.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div1.m_siblingIndex = div3.m_siblingIndex + 1;
	  div1.m_positiveZIndexValue = 1;

	  // Ensure that root is behind all
	  EXPECT_LT(root.m_value, div3.m_value);
	  EXPECT_LT(root.m_value, div4.m_value);
	  EXPECT_LT(root.m_value, div1.m_value);

	  // Check divs
	  EXPECT_GT(div3.m_value, root.m_value);
	  EXPECT_LT(div3.m_value, div1.m_value);
	  EXPECT_LT(div3.m_value, div4.m_value);

	  EXPECT_GT(div4.m_value, root.m_value);
	  EXPECT_GT(div4.m_value, div3.m_value);
	  EXPECT_LT(div4.m_value, div1.m_value);

	  EXPECT_GT(div1.m_value, root.m_value);
	  EXPECT_GT(div1.m_value, div3.m_value);
	  EXPECT_GT(div1.m_value, div4.m_value);
	}

	UNIT_TEST(Widgets, DepthComplexWithoutZIndex)
	{
	  DepthValue root;
	  root.m_value = 0;
	  root.m_scopeIndex = 0;
	  root.m_computedDepthIndex = 0;
	  root.m_siblingIndex = 0;
	  root.m_zeroZIndexValue = 1;

	  DepthValue div1;
	  div1.m_value = 0;
	  div1.m_scopeIndex = 0;
	  div1.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div1.m_siblingIndex = 0;
	  div1.m_zeroZIndexValue = 1;

	  DepthValue div2;
	  div2.m_value = 0;
	  div2.m_scopeIndex = 0;
	  div2.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div2.m_siblingIndex = div1.m_siblingIndex + 1;
	  div2.m_zeroZIndexValue = 1;

	  DepthValue div3;
	  div3.m_value = 0;
	  div3.m_scopeIndex = 0;
	  div3.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div3.m_siblingIndex = div2.m_siblingIndex + 1;
	  div3.m_zeroZIndexValue = 1;

	  DepthValue div4;
	  div4.m_value = 0;
	  div4.m_scopeIndex = div3.m_scopeIndex + 1;
	  div4.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div4.m_siblingIndex = 0;
	  div4.m_zeroZIndexValue = 1;

	  DepthValue div5;
	  div5.m_value = 0;
	  div5.m_scopeIndex = div3.m_scopeIndex + 1;
	  div5.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div5.m_siblingIndex = div4.m_siblingIndex + 1;
	  div5.m_zeroZIndexValue = 1;

	  DepthValue div6;
	  div6.m_value = 0;
	  div6.m_scopeIndex = div3.m_scopeIndex + 1;
	  div6.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div6.m_siblingIndex = div5.m_siblingIndex + 1;
	  div6.m_zeroZIndexValue = 1;

	  // Ensure that root is behind all
	  EXPECT_LT(root.m_value, div1.m_value);
	  EXPECT_LT(root.m_value, div2.m_value);
	  EXPECT_LT(root.m_value, div3.m_value);
	  EXPECT_LT(root.m_value, div4.m_value);
	  EXPECT_LT(root.m_value, div5.m_value);
	  EXPECT_LT(root.m_value, div6.m_value);

	  // Check divs
	  EXPECT_GT(div1.m_value, root.m_value);
	  EXPECT_LT(div1.m_value, div2.m_value);
	  EXPECT_LT(div1.m_value, div3.m_value);
	  EXPECT_LT(div1.m_value, div4.m_value);
	  EXPECT_LT(div1.m_value, div5.m_value);
	  EXPECT_LT(div1.m_value, div6.m_value);

	  EXPECT_GT(div2.m_value, root.m_value);
	  EXPECT_GT(div2.m_value, div1.m_value);
	  EXPECT_LT(div2.m_value, div3.m_value);
	  EXPECT_LT(div2.m_value, div4.m_value);
	  EXPECT_LT(div2.m_value, div5.m_value);
	  EXPECT_LT(div2.m_value, div6.m_value);

	  EXPECT_GT(div3.m_value, root.m_value);
	  EXPECT_GT(div3.m_value, div1.m_value);
	  EXPECT_GT(div3.m_value, div2.m_value);
	  EXPECT_LT(div3.m_value, div4.m_value);
	  EXPECT_LT(div3.m_value, div5.m_value);
	  EXPECT_LT(div3.m_value, div6.m_value);

	  EXPECT_GT(div4.m_value, root.m_value);
	  EXPECT_GT(div4.m_value, div1.m_value);
	  EXPECT_GT(div4.m_value, div2.m_value);
	  EXPECT_GT(div4.m_value, div3.m_value);
	  EXPECT_LT(div4.m_value, div5.m_value);
	  EXPECT_LT(div4.m_value, div6.m_value);

	  EXPECT_GT(div5.m_value, root.m_value);
	  EXPECT_GT(div5.m_value, div1.m_value);
	  EXPECT_GT(div5.m_value, div2.m_value);
	  EXPECT_GT(div5.m_value, div3.m_value);
	  EXPECT_GT(div5.m_value, div4.m_value);
	  EXPECT_LT(div5.m_value, div6.m_value);

	  EXPECT_GT(div6.m_value, root.m_value);
	  EXPECT_GT(div6.m_value, div1.m_value);
	  EXPECT_GT(div6.m_value, div2.m_value);
	  EXPECT_GT(div6.m_value, div3.m_value);
	  EXPECT_GT(div6.m_value, div4.m_value);
	  EXPECT_GT(div6.m_value, div5.m_value);
	}*/

	UNIT_TEST(Widgets, DepthInfoHierarchyIndex)
	{
		// Simple case, one element in front of the other
		{
			const DepthInfo root;
			const DepthInfo back(root, root, 0, false, false);
			const DepthInfo front(root, back, 0, false, false);
			EXPECT_GT(front, back);
		}
		// Case with three elements where the front is a child of the middle
		{
			const DepthInfo root;
			const DepthInfo back(root, root, 0, false, false);
			const DepthInfo middle(root, back, 0, false, false);
			const DepthInfo front(middle, middle, 0, false, false);
			EXPECT_GT(middle, back);
			EXPECT_GT(front, middle);
			EXPECT_GT(front, back);
		}
		// Case with three elements where the middle is a child of the first (non-root)
		{
			const DepthInfo root;
			const DepthInfo back(root, root, 0, false, false);
			const DepthInfo middle(back, back, 0, false, false);
			const DepthInfo front(root, middle, 0, false, false);
			EXPECT_GT(middle, back);
			EXPECT_GT(front, middle);
			EXPECT_GT(front, back);
		}
	}

	UNIT_TEST(Widgets, DepthInfoInheritZIndex)
	{
		const DepthInfo root;
		const DepthInfo parent(root, root, 6, false, true);
		EXPECT_GT(parent, root);
		EXPECT_GT(parent.GetComputedDepthIndex(), root.GetComputedDepthIndex());
		EXPECT_EQ(parent.GetCustomDepth(), 6);

		const DepthInfo child(parent, parent, 0, false, true);
		EXPECT_GT(child, parent);
	}

	UNIT_TEST(Widgets, DepthInfoHierarchy)
	{
		const DepthInfo root;
		{
			const DepthInfo back(root, root, 0, false, false);
			EXPECT_EQ(back.GetComputedDepthIndex(), 1);
			EXPECT_EQ(back.GetScopeIndex(), 0);
			EXPECT_EQ(back.GetCustomDepth(), 0);

			const DepthInfo front(root, back, 0, false, false);
			EXPECT_GT(front.GetComputedDepthIndex(), back.GetComputedDepthIndex());
			EXPECT_EQ(front.GetScopeIndex(), 0);
			EXPECT_EQ(front.GetCustomDepth(), 0);

			EXPECT_GT(front, back);
		}

		{
			const DepthInfo back(root, root, 0, false, false);
			EXPECT_EQ(back.GetComputedDepthIndex(), 1);
			EXPECT_EQ(back.GetScopeIndex(), 0);
			EXPECT_EQ(back.GetCustomDepth(), 0);

			const DepthInfo middle(root, back, 0, false, false);
			EXPECT_GT(middle.GetComputedDepthIndex(), back.GetComputedDepthIndex());
			EXPECT_EQ(middle.GetScopeIndex(), 0);
			EXPECT_EQ(middle.GetCustomDepth(), 0);

			const DepthInfo front(middle, middle, 0, false, false);
			EXPECT_GT(front.GetComputedDepthIndex(), middle.GetComputedDepthIndex());
			EXPECT_EQ(front.GetScopeIndex(), 0);
			EXPECT_EQ(front.GetCustomDepth(), 0);

			EXPECT_GT(middle, back);
			EXPECT_GT(front, middle);
			EXPECT_GT(front, back);
		}

		{
			const DepthInfo back(root, root, 0, false, false);
			EXPECT_EQ(back.GetComputedDepthIndex(), 1);
			EXPECT_EQ(back.GetScopeIndex(), 0);
			EXPECT_EQ(back.GetCustomDepth(), 0);

			const DepthInfo middle(back, back, 0, false, false);
			EXPECT_GT(middle.GetComputedDepthIndex(), back.GetComputedDepthIndex());
			EXPECT_EQ(middle.GetScopeIndex(), 0);
			EXPECT_EQ(middle.GetCustomDepth(), 0);

			const DepthInfo front(root, middle, 0, false, false);
			EXPECT_GT(front.GetComputedDepthIndex(), middle.GetComputedDepthIndex());
			EXPECT_EQ(front.GetScopeIndex(), 0);
			EXPECT_EQ(front.GetCustomDepth(), 0);

			EXPECT_GT(middle, back);
			EXPECT_GT(front, middle);
			EXPECT_GT(front, back);
		}
	}

	UNIT_TEST(Widgets, DepthInfoComplexUnscopedWithZIndex)
	{
		const DepthInfo root;

		const DepthInfo div1(root, root, 5, false, false);
		const DepthInfo div2(root, div1, 2, false, false);
		const DepthInfo div3(root, div2, 0, false, false);
		const DepthInfo div4(div3, div3, 6, false, false);
		const DepthInfo div5(div3, div4, 1, false, false);
		const DepthInfo div6(div3, div5, 3, false, false);

		// Ensure that root is behind all
		EXPECT_LT(root, div1);
		EXPECT_LT(root, div2);
		EXPECT_LT(root, div3);
		EXPECT_LT(root, div4);
		EXPECT_LT(root, div5);
		EXPECT_LT(root, div6);

		// Check divs
		EXPECT_GT(div3, root);
		EXPECT_LT(div3, div1);
		EXPECT_LT(div3, div2);
		EXPECT_LT(div3, div4);
		EXPECT_LT(div3, div5);
		EXPECT_LT(div3, div6);

		EXPECT_GT(div5, root);
		EXPECT_GT(div5, div3);
		EXPECT_LT(div5, div2);
		EXPECT_LT(div5, div1);
		EXPECT_LT(div5, div4);
		EXPECT_LT(div5, div6);

		EXPECT_GT(div6, root);
		EXPECT_GT(div6, div3);
		EXPECT_GT(div6, div5);
		EXPECT_LT(div2, div1);
		EXPECT_LT(div2, div4);
		EXPECT_LT(div2, div6);

		EXPECT_GT(div6, root);
		EXPECT_GT(div6, div2);
		EXPECT_GT(div6, div3);
		EXPECT_GT(div6, div5);
		EXPECT_LT(div6, div1);
		EXPECT_LT(div6, div4);

		EXPECT_GT(div1, root);
		EXPECT_GT(div1, div2);
		EXPECT_GT(div1, div3);
		EXPECT_GT(div1, div5);
		EXPECT_GT(div1, div6);
		EXPECT_LT(div1, div4);

		EXPECT_GT(div4, root);
		EXPECT_GT(div4, div1);
		EXPECT_GT(div4, div2);
		EXPECT_GT(div4, div3);
		EXPECT_GT(div4, div5);
		EXPECT_GT(div4, div6);
	}
	/*
	UNIT_TEST(Widgets, DepthComplexWithZIndex)
	{
	  DepthValue root;
	  root.m_value = 0;
	  root.m_scopeIndex = 0;
	  root.m_computedDepthIndex = 0;
	  root.m_siblingIndex = 0;
	  root.m_zeroZIndexValue = 1;

	  DepthValue div1;
	  div1.m_value = 0;
	  div1.m_scopeIndex = 0;
	  div1.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div1.m_siblingIndex = 0;
	  div1.m_positiveZIndexValue = 5;

	  DepthValue div2;
	  div2.m_value = 0;
	  div2.m_scopeIndex = 0;
	  div2.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div2.m_siblingIndex = div1.m_siblingIndex + 1;
	  div2.m_positiveZIndexValue = 2;

	  DepthValue div3;
	  div3.m_value = 0;
	  div3.m_scopeIndex = 0;
	  div3.m_computedDepthIndex = root.m_computedDepthIndex + 1;
	  div3.m_siblingIndex = div2.m_siblingIndex + 1;
	  div3.m_positiveZIndexValue = 4;

	  DepthValue div4;
	  div4.m_value = 0;
	  div4.m_scopeIndex = div3.m_scopeIndex + 1;
	  div4.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div4.m_siblingIndex = 0;
	  div4.m_positiveZIndexValue = 6;

	  DepthValue div5;
	  div5.m_value = 0;
	  div5.m_scopeIndex = div3.m_scopeIndex + 1;
	  div5.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div5.m_siblingIndex = div4.m_siblingIndex + 1;
	  div5.m_positiveZIndexValue = 1;

	  DepthValue div6;
	  div6.m_value = 0;
	  div6.m_scopeIndex = div3.m_scopeIndex + 1;
	  div6.m_computedDepthIndex = div3.m_computedDepthIndex + 1;
	  div6.m_siblingIndex = div5.m_siblingIndex + 1;
	  div6.m_positiveZIndexValue = 3;

	  // Ensure that root is behind all
	  EXPECT_LT(root, div1);
	  EXPECT_LT(root, div2);
	  EXPECT_LT(root, div3);
	  EXPECT_LT(root, div4);
	  EXPECT_LT(root, div5);
	  EXPECT_LT(root, div6);

	  // Ensure that div2 is behind all but root
	  EXPECT_GT(div2, root);
	  EXPECT_LT(div2, div1);
	  EXPECT_LT(div2, div3);
	  EXPECT_LT(div2, div4);
	  EXPECT_LT(div2, div5);
	  EXPECT_LT(div2, div6);
	}*/
}
