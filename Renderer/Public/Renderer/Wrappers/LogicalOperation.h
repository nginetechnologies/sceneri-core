#pragma once

namespace ngine::Rendering
{
	enum class LogicalOperation : uint32
	{
		Clear = 0,
		And = 1,
		AndReverse = 2,
		Copy = 3,
		AndInverted = 4,
		NoOperation = 5,
		Xor = 6,
		Or = 7,
		Nor = 8,
		Equivalent = 9,
		Invert = 10,
		OrReverse = 11,
		CopyInverted = 12,
		OrInverted = 13,
		Nand = 14,
		Set = 15
	};
}
