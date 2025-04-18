#pragma once

#include <Common/Math/Color.h>

namespace ngine::Rendering
{
	struct DepthValue
	{
		float m_value;
	};
	struct StencilValue
	{
		uint32 m_value;
	};
	struct DepthStencilValue
	{
		DepthValue m_depth;
		StencilValue m_stencil;
	};

	struct ClearValue
	{
		ClearValue() = default;
		constexpr ClearValue(const Math::Color color)
			: m_floatValues{color.r, color.g, color.b, color.a}
		{
		}
		constexpr ClearValue(const DepthValue depthValue)
			: m_depthStencilValue{depthValue}
		{
		}
		constexpr ClearValue(const StencilValue stencilValue)
			: m_depthStencilValue{DepthValue{}, stencilValue}
		{
		}
		constexpr ClearValue(const DepthStencilValue depthStencilValue)
			: m_depthStencilValue{depthStencilValue}
		{
		}

		constexpr ClearValue& operator=(const Math::Color color)
		{
			m_floatValues = {color.r, color.g, color.b, color.a};
			return *this;
		}
		constexpr ClearValue& operator=(const DepthValue depthValue)
		{
			m_depthStencilValue.m_depth = depthValue;
			return *this;
		}
		constexpr ClearValue& operator=(const StencilValue stencilValue)
		{
			m_depthStencilValue.m_stencil = stencilValue;
			return *this;
		}
		constexpr ClearValue& operator=(const DepthStencilValue depthStencilValue)
		{
			m_depthStencilValue = depthStencilValue;
			return *this;
		}

		[[nodiscard]] operator Math::Color() const
		{
			return {m_floatValues[0], m_floatValues[1], m_floatValues[2], m_floatValues[3]};
		}
		[[nodiscard]] operator DepthStencilValue() const
		{
			return m_depthStencilValue;
		}
	protected:
		union
		{
			Array<float, 4> m_floatValues;
			Array<int32, 4> m_intValues;
			Array<uint32, 4> m_unsignedIntValues;
			DepthStencilValue m_depthStencilValue;
		};
	};
}
