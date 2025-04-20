#pragma once

#include "DesiredSize.h"

#include <Renderer/Window/ScreenProperties.h>

#include <Common/Math/Floor.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Reflection
{
	struct Registry;
}

namespace ngine::Widgets::Style
{
	struct SizeAxis
	{
		constexpr SizeAxis()
			: m_value(Math::Ratiof(0.f))
		{
		}
		constexpr SizeAxis(const InvalidType)
			: m_value()
		{
		}
		constexpr SizeAxis(const Math::ZeroType)
			: m_value(0_px)
		{
		}
		constexpr SizeAxis(const AutoType)
			: m_value(Auto)
		{
		}
		constexpr SizeAxis(const NoMaximumType)
			: m_value(NoMaximum)
		{
		}
		constexpr SizeAxis(const ReferencePixel value)
			: m_value(value)
		{
		}
		constexpr SizeAxis(const DesiredSize size)
			: m_value(size)
		{
		}
		constexpr SizeAxis(const Math::Ratiof ratio)
			: m_value(ratio)
		{
		}
		constexpr SizeAxis(const Math::Ratiod ratio)
			: m_value((Math::Ratiof)ratio)
		{
		}
		constexpr SizeAxis(const Font::Point point)
			: m_value(point)
		{
		}
		constexpr SizeAxis(const ViewportWidthRatio ratio)
			: m_value(ratio)
		{
		}
		constexpr SizeAxis(const ViewportHeightRatio ratio)
			: m_value(ratio)
		{
		}
		constexpr SizeAxis(const ViewportMinimumRatio ratio)
			: m_value(ratio)
		{
		}
		constexpr SizeAxis(const ViewportMaximumRatio ratio)
			: m_value(ratio)
		{
		}
		constexpr SizeAxis(const PixelValue pixelValue)
			: m_value(pixelValue)
		{
		}
		constexpr SizeAxis(const float pixelValue)
			: m_value(pixelValue)
		{
		}
		SizeAxis(const SizeAxis&) = default;
		SizeAxis& operator=(const SizeAxis&) = default;
		SizeAxis(SizeAxis&&) = default;
		SizeAxis& operator=(SizeAxis&&) = default;

		[[nodiscard]] float GetPoint(
			const float availableAreaSize,
			const float smallestChildSize,
			const float largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return m_value.Visit(
				[screenProperties](const ReferencePixel value) -> float
				{
					return value.GetPoint(screenProperties.m_devicePixelRatio);
				},
				[screenProperties](const PhysicalSizePixel value) -> float
				{
					return value.GetPhysicalPoint(screenProperties.m_dotsPerInch);
				},
				[availableAreaSize](const Math::Ratiof value) -> float
				{
					return value * availableAreaSize;
				},
				[](const PixelValue value) -> float
				{
					return (float)value.m_value;
				},
				[&screenProperties](const ViewportWidthRatio value) -> float
				{
					return (Math::Ratiof)value * (float)screenProperties.m_dimensions.x;
				},
				[&screenProperties](const ViewportHeightRatio value) -> float
				{
					return (Math::Ratiof)value * (float)screenProperties.m_dimensions.y;
				},
				[&screenProperties](const ViewportMinimumRatio value) -> float
				{
					return (Math::Ratiof)value * (float)Math::Min(screenProperties.m_dimensions.x, screenProperties.m_dimensions.y);
				},
				[&screenProperties](const ViewportMaximumRatio value) -> float
				{
					return (Math::Ratiof)value * (float)Math::Max(screenProperties.m_dimensions.x, screenProperties.m_dimensions.y);
				},
				[](const float value) -> float
				{
					return value;
				},
				[&screenProperties](const Font::Point value) -> float
				{
					const ReferencePixel pixel = ReferencePixel::FromValue(value.GetPixels());
					return pixel.GetPoint(screenProperties.m_devicePixelRatio);
				},
				[availableAreaSize](const AutoType) -> float
				{
					// Start with the maximum size, and have it corrected elsewhere after
					return availableAreaSize;
				},
				[](const NoMaximumType) -> float
				{
					return 100000.f;
				},
				[smallestChildSize](const MinContentSizeType) -> float
				{
					return (float)smallestChildSize;
				},
				[largestChildSize](const MaxContentSizeType) -> float
				{
					return (float)largestChildSize;
				},
				[availableAreaSize, largestChildSize](const FitContentSizeType) -> float
				{
					return Math::Min(availableAreaSize, (float)largestChildSize);
				},
				[&screenProperties](const SafeAreaInsetLeftType) -> float
				{
					return screenProperties.m_safeArea.m_left;
				},
				[&screenProperties](const SafeAreaInsetRightType) -> float
				{
					return screenProperties.m_safeArea.m_right;
				},
				[&screenProperties](const SafeAreaInsetTopType) -> float
				{
					return screenProperties.m_safeArea.m_top;
				},
				[&screenProperties](const SafeAreaInsetBottomType) -> float
				{
					return screenProperties.m_safeArea.m_bottom;
				},
				[](const ngine::DataSource::PropertyIdentifier) -> float
				{
					return 0.f;
				},
				[]() -> float
				{
					return 0.f;
				}
			);
		}

		[[nodiscard]] float GetPoint(const float availableAreaSize, const Rendering::ScreenProperties screenProperties) const
		{
			return GetPoint(
				availableAreaSize,
				availableAreaSize, /* smallestChildSize: Start with the maximum size, and have it corrected elsewhere after */
				availableAreaSize, /* largestChildSize: Start with the maximum size, and have it corrected elsewhere after */
				screenProperties
			);
		}

		[[nodiscard]] int32 Get(const int32 availableAreaSize, const Rendering::ScreenProperties screenProperties) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, screenProperties));
		}

		[[nodiscard]] int32 Get(
			const int32 availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, (float)smallestChildSize, (float)largestChildSize, screenProperties));
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_value.HasValue();
		}

		[[nodiscard]] bool IsConstantValue() const
		{
			return m_value.Visit(
				[](const ReferencePixel) -> bool
				{
					return true;
				},
				[](const PhysicalSizePixel) -> bool
				{
					return true;
				},
				[](const Math::Ratiof value) -> bool
				{
					return value == 0_percent;
				},
				[](const PixelValue) -> bool
				{
					return true;
				},
				[](const ViewportWidthRatio) -> bool
				{
					return true;
				},
				[](const ViewportHeightRatio) -> bool
				{
					return true;
				},
				[](const ViewportMinimumRatio) -> bool
				{
					return true;
				},
				[](const ViewportMaximumRatio) -> bool
				{
					return true;
				},
				[](const float) -> bool
				{
					return true;
				},
				[](const Font::Point) -> bool
				{
					return true;
				},
				[](const AutoType) -> bool
				{
					return false;
				},
				[](const NoMaximumType) -> bool
				{
					return true;
				},
				[](const MinContentSizeType) -> bool
				{
					return false;
				},
				[](const MaxContentSizeType) -> bool
				{
					return false;
				},
				[](const FitContentSizeType) -> bool
				{
					return false;
				},
				[](const SafeAreaInsetLeftType) -> bool
				{
					return true;
				},
				[](const SafeAreaInsetRightType) -> bool
				{
					return true;
				},
				[](const SafeAreaInsetTopType) -> bool
				{
					return true;
				},
				[](const SafeAreaInsetBottomType) -> bool
				{
					return true;
				},
				[](const ngine::DataSource::PropertyIdentifier) -> bool
				{
					return false;
				},
				[]() -> bool
				{
					return true;
				}
			);
		}

		[[nodiscard]] bool DependsOnParentDimensions() const
		{
			return (m_value.Is<Math::Ratiof>() && m_value.GetExpected<Math::Ratiof>() != 0_percent);
		}

		[[nodiscard]] bool DependsOnContentDimensions() const
		{
			return m_value.Is<MinContentSizeType>() | m_value.Is<MaxContentSizeType>() | m_value.Is<FitContentSizeType>();
		}

		[[nodiscard]] SizeAxis operator-() const
		{
			return m_value.Visit(
				[](const ReferencePixel value) -> SizeAxis
				{
					return -value;
				},
				[](const PhysicalSizePixel value) -> SizeAxis
				{
					return -value;
				},
				[](const Math::Ratiof value) -> SizeAxis
				{
					return -value;
				},
				[](const PixelValue value) -> SizeAxis
				{
					return -value;
				},
				[](const ViewportWidthRatio value) -> SizeAxis
				{
					return -value;
				},
				[](const ViewportHeightRatio value) -> SizeAxis
				{
					return -value;
				},
				[](const ViewportMinimumRatio value) -> SizeAxis
				{
					return -value;
				},
				[](const ViewportMaximumRatio value) -> SizeAxis
				{
					return -value;
				},
				[](const float value) -> SizeAxis
				{
					return -value;
				},
				[](const Font::Point value) -> SizeAxis
				{
					return -value;
				},
				[](const AutoType) -> SizeAxis
				{
					return Auto;
				},
				[](const NoMaximumType) -> SizeAxis
				{
					return NoMaximum;
				},
				[](const MinContentSizeType) -> SizeAxis
				{
					return DesiredSize{MinContentSizeType::MinContent};
				},
				[](const MaxContentSizeType) -> SizeAxis
				{
					return DesiredSize{MaxContentSizeType::MaxContent};
				},
				[](const FitContentSizeType) -> SizeAxis
				{
					return DesiredSize{FitContentSizeType::FitContent};
				},
				[](const SafeAreaInsetLeftType) -> SizeAxis
				{
					Assert(false, "Not implemented");
					return DesiredSize{SafeAreaInsetLeftType::SafeAreaInsetLeft};
				},
				[](const SafeAreaInsetRightType) -> SizeAxis
				{
					Assert(false, "Not implemented");
					return DesiredSize{SafeAreaInsetRightType::SafeAreaInsetRight};
				},
				[](const SafeAreaInsetTopType) -> SizeAxis
				{
					Assert(false, "Not implemented");
					return DesiredSize{SafeAreaInsetTopType::SafeAreaInsetTop};
				},
				[](const SafeAreaInsetBottomType) -> SizeAxis
				{
					Assert(false, "Not implemented");
					return DesiredSize{SafeAreaInsetBottomType::SafeAreaInsetBottom};
				},
				[](const ngine::DataSource::PropertyIdentifier propertyIdentifier) -> SizeAxis
				{
					return SizeAxis{propertyIdentifier};
				},
				[]() -> SizeAxis
				{
					return SizeAxis{Invalid};
				}
			);
		}

		[[nodiscard]] inline bool operator==(const SizeAxis& other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] inline bool operator!=(const SizeAxis& other) const
		{
			return m_value != other.m_value;
		}

		[[nodiscard]] String ToString() const;
		[[nodiscard]] static Optional<SizeAxis> Parse(const ConstStringView string);

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;

		DesiredSize m_value;
	};

	enum class OperationType : uint8
	{
		Addition,
		Subtraction,
		Multiplication,
		Division,
		Min,
		Max
	};

	using SizeAxisOperation = Variant<SizeAxis, OperationType>;

	struct SizeAxisExpressionView : public ArrayView<const SizeAxisOperation>
	{
		using BaseType = ArrayView<const SizeAxisOperation>;

		SizeAxisExpressionView() = default;
		SizeAxisExpressionView(const BaseType& value)
			: BaseType(value)
		{
		}

		using Operation = SizeAxisOperation;

		[[nodiscard]] bool IsValid() const
		{
			return BaseType::HasElements();
		}

		template<typename Type>
		[[nodiscard]] bool Is() const
		{
			return BaseType::GetSize() == 1 && (*this)[0].GetExpected<SizeAxis>().m_value.Is<Type>();
		}

		[[nodiscard]] bool IsConstantValue() const
		{
			for (const SizeAxisOperation& operation : *this)
			{
				if (const Optional<const SizeAxis*> sizeAxis = operation.Get<SizeAxis>())
				{
					if (!sizeAxis->IsConstantValue())
					{
						return false;
					}
				}
			}
			return true;
		}

		[[nodiscard]] bool DependsOnParentDimensions() const
		{
			for (const SizeAxisOperation& operation : *this)
			{
				if (const Optional<const SizeAxis*> sizeAxis = operation.Get<SizeAxis>())
				{
					if (sizeAxis->DependsOnParentDimensions())
					{
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] bool DependsOnContentDimensions() const
		{
			for (const SizeAxisOperation& operation : *this)
			{
				if (const Optional<const SizeAxis*> sizeAxis = operation.Get<SizeAxis>())
				{
					if (sizeAxis->DependsOnContentDimensions())
					{
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] Optional<SizeAxis> Get() const
		{
			if (GetSize() == 1)
			{
				if (const Optional<const SizeAxis*> sizeAxis = (*this)[0].Get<SizeAxis>())
				{
					return *sizeAxis;
				}
			}
			return {};
		}

		[[nodiscard]] float GetPoint(
			const float availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			Rendering::ScreenProperties screenProperties
		) const
		{
			ArrayView<const Operation> operations = *this;
			return SizeAxisExpressionView::GetPoint(operations, availableAreaSize, smallestChildSize, largestChildSize, screenProperties);
		}
		[[nodiscard]] float GetPoint(const float availableAreaSize, Rendering::ScreenProperties screenProperties) const
		{
			ArrayView<const Operation> operations = *this;
			return SizeAxisExpressionView::GetPoint(operations, availableAreaSize, screenProperties);
		}

		[[nodiscard]] int32 Get(const int32 availableAreaSize, const Rendering::ScreenProperties screenProperties) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, screenProperties));
		}

		[[nodiscard]] int32 Get(
			const int32 availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, smallestChildSize, largestChildSize, screenProperties));
		}

		[[nodiscard]] static float GetPoint(
			ArrayView<const Operation>& operationValues,
			const float availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			Rendering::ScreenProperties screenProperties
		)
		{
			if (operationValues.HasElements())
			{
				const Operation& firstOperation = operationValues[0];
				operationValues++;
				if (const Optional<const SizeAxis*> firstOperationAxis = firstOperation.Get<SizeAxis>())
				{
					return firstOperationAxis->GetPoint(availableAreaSize, (float)smallestChildSize, (float)largestChildSize, screenProperties);
				}

				const OperationType operationType = *firstOperation.Get<OperationType>();
				const Array<float, 2> values{
					GetPoint(operationValues, availableAreaSize, smallestChildSize, largestChildSize, screenProperties),
					GetPoint(operationValues, availableAreaSize, smallestChildSize, largestChildSize, screenProperties)
				};

				switch (operationType)
				{
					case OperationType::Addition:
						return values[0] + values[1];
					case OperationType::Subtraction:
						return values[0] - values[1];
					case OperationType::Multiplication:
						return values[0] * values[1];
					case OperationType::Division:
						return values[0] / values[1];
					case OperationType::Min:
						return Math::Min(values[0], values[1]);
					case OperationType::Max:
						return Math::Max(values[0], values[1]);
				}
				ExpectUnreachable();
			}
			else
			{
				return 0.f;
			}
		}
		[[nodiscard]] static float
		GetPoint(ArrayView<const Operation>& operationValues, const float availableAreaSize, const Rendering::ScreenProperties screenProperties)
		{
			if (operationValues.HasElements())
			{
				const Operation& firstOperation = operationValues[0];
				operationValues++;
				if (const Optional<const SizeAxis*> firstOperationAxis = firstOperation.Get<SizeAxis>())
				{
					return firstOperationAxis->GetPoint(availableAreaSize, screenProperties);
				}

				const OperationType operationType = firstOperation.GetExpected<OperationType>();
				const Array<float, 2> values{
					GetPoint(operationValues, availableAreaSize, screenProperties),
					GetPoint(operationValues, availableAreaSize, screenProperties)
				};

				switch (operationType)
				{
					case OperationType::Addition:
						return values[0] + values[1];
					case OperationType::Subtraction:
						return values[0] - values[1];
					case OperationType::Multiplication:
						return values[0] * values[1];
					case OperationType::Division:
						return values[0] / values[1];
					case OperationType::Min:
						return Math::Min(values[0], values[1]);
					case OperationType::Max:
						return Math::Max(values[0], values[1]);
				}
				ExpectUnreachable();
			}
			else
			{
				return 0.f;
			}
		}

		[[nodiscard]] String ToString() const;
	};
	static_assert(TypeTraits::IsTriviallyDestructible<SizeAxis>);

	struct SizeAxisExpression
	{
		inline static constexpr Guid TypeGuid = "eb22fb27-fe0a-438a-9f2e-6557c3eca062"_guid;

		SizeAxisExpression()
			: m_operations(SizeAxis())
		{
		}
		SizeAxisExpression(const InvalidType)
			: m_operations(SizeAxis(Invalid))
		{
		}
		SizeAxisExpression(const Math::ZeroType)
			: m_operations(SizeAxis(Math::Zero))
		{
		}
		SizeAxisExpression(const AutoType)
			: m_operations(SizeAxis(Auto))
		{
		}
		SizeAxisExpression(const ReferencePixel value)
			: m_operations(SizeAxis(value))
		{
		}
		SizeAxisExpression(const DesiredSize size)
			: m_operations(SizeAxis(size))
		{
		}
		SizeAxisExpression(const Math::Ratiof ratio)
			: m_operations(SizeAxis(ratio))
		{
		}
		SizeAxisExpression(const Math::Ratiod ratio)
			: m_operations(SizeAxis(ratio))
		{
		}
		SizeAxisExpression(const PixelValue pixelValue)
			: m_operations(SizeAxis(pixelValue))
		{
		}
		SizeAxisExpression(SizeAxis&& value)
			: m_operations(Forward<SizeAxis>(value))
		{
		}
		SizeAxisExpression(const SizeAxis& value)
			: m_operations(value)
		{
		}
		SizeAxisExpression(const SizeAxisExpressionView view)
			: m_operations(OperationsContainer{view})
		{
		}

		[[nodiscard]] operator SizeAxisExpressionView() const LIFETIME_BOUND
		{
			return SizeAxisExpressionView{m_operations.GetView()};
		}

		using Operation = SizeAxisOperation;

		[[nodiscard]] bool IsValid() const
		{
			if (m_operations.HasElements())
			{
				const Operation& firstOperation = m_operations[0];
				Assert(firstOperation.HasValue());
				if (const Optional<const SizeAxis*> pSizeAxis = firstOperation.Get<SizeAxis>())
				{
					return pSizeAxis->IsValid();
				}
				else
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] float GetPoint(
			const float availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			Rendering::ScreenProperties screenProperties
		) const
		{
			ArrayView<const Operation> operations = m_operations.GetView();
			return SizeAxisExpressionView::GetPoint(operations, availableAreaSize, smallestChildSize, largestChildSize, screenProperties);
		}
		[[nodiscard]] float GetPoint(const float availableAreaSize, Rendering::ScreenProperties screenProperties) const
		{
			ArrayView<const Operation> operations = m_operations.GetView();
			return SizeAxisExpressionView::GetPoint(operations, availableAreaSize, screenProperties);
		}

		[[nodiscard]] int32 Get(const int32 availableAreaSize, const Rendering::ScreenProperties screenProperties) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, screenProperties));
		}

		[[nodiscard]] int32 Get(
			const int32 availableAreaSize,
			const int32 smallestChildSize,
			const int32 largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return (int32)Math::Floor(GetPoint((float)availableAreaSize, smallestChildSize, largestChildSize, screenProperties));
		}

		template<typename Type>
		[[nodiscard]] bool Is() const
		{
			SizeAxisExpressionView expression{m_operations.GetView()};
			return expression.Is<Type>();
		}

		[[nodiscard]] bool IsConstantValue() const
		{
			SizeAxisExpressionView expression{m_operations.GetView()};
			return expression.IsConstantValue();
		}
		[[nodiscard]] bool DependsOnParentDimensions() const
		{
			SizeAxisExpressionView expression{m_operations.GetView()};
			return expression.DependsOnParentDimensions();
		}
		[[nodiscard]] bool DependsOnContentDimensions() const
		{
			SizeAxisExpressionView expression{m_operations.GetView()};
			return expression.DependsOnContentDimensions();
		}

		[[nodiscard]] Optional<SizeAxis> Get() const
		{
			SizeAxisExpressionView expression{m_operations.GetView()};
			return expression.Get();
		}

		[[nodiscard]] inline bool operator==(const SizeAxisExpression& other) const
		{
			return m_operations.GetView() == other.m_operations.GetView();
		}
		[[nodiscard]] inline bool operator!=(const SizeAxisExpression& other) const
		{
			return m_operations.GetView() != other.m_operations.GetView();
		}

		[[nodiscard]] String ToString() const;
		[[nodiscard]] static Optional<SizeAxisExpression> Parse(const ConstStringView string);

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;

		using OperationsContainer = InlineVector<Operation, 1>;
		OperationsContainer m_operations;
	};
	static_assert(TypeTraits::IsTriviallyDestructible<SizeAxisOperation>);
}
