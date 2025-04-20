#pragma once

#include <Renderer/Window/CursorType.h>

namespace ngine::Widgets
{
	struct SetCursorCallback
	{
		SetCursorCallback() = default;
		SetCursorCallback(Rendering::CursorType& target)
			: m_cursorTarget(target)
		{
		}
		SetCursorCallback(const SetCursorCallback&) = delete;
		SetCursorCallback& operator=(const SetCursorCallback&) = delete;
		SetCursorCallback(SetCursorCallback&& other) = default;
		SetCursorCallback& operator=(SetCursorCallback&&) = default;
		~SetCursorCallback()
		{
			if (m_cursorTarget.IsValid())
			{
				*m_cursorTarget = m_exitScopeCursor;
			}
		}

		void SetCursor(const Rendering::CursorType cursor)
		{
			if (LIKELY(m_cursorTarget.IsValid()))
			{
				*m_cursorTarget = cursor;
			}
			m_cursorTarget = Invalid;
		}

		void SetOverridableCursor(const Rendering::CursorType cursor)
		{
			m_exitScopeCursor = cursor;
		}
	protected:
		Optional<Rendering::CursorType*> m_cursorTarget;
		Rendering::CursorType m_exitScopeCursor = Rendering::CursorType::Arrow;
	};
}
