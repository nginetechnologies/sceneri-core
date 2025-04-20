#pragma once

namespace ngine::Widgets
{
	template<uint8 ReferenceDPI>
	struct TReferenceValue;

	inline static constexpr uint8 DpiReference = 96;
	using ReferencePixel = TReferenceValue<DpiReference>;
}
