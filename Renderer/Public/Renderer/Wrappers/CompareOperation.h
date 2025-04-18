#pragma once

namespace ngine::Rendering
{
	enum class CompareOperation : uint32
	{
		AlwaysFail,
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,
		AlwaysSucceed
	};
}
