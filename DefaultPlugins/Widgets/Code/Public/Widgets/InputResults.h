#pragma once

namespace ngine::Widgets
{
	struct Widget;

	struct CursorResult
	{
		Optional<Widget*> pHandledWidget = nullptr;
	};

	struct DragAndDropResult
	{
		Optional<Widget*> pHandledWidget = nullptr;
	};

	enum class PanType
	{
		Default,
		DragWidget,
		MoveWidget
	};

	struct PanResult : public CursorResult
	{
		PanType panType = PanType::Default;
	};
}
