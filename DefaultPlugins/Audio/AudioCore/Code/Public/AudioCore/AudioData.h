#pragma once

#include "AudioIdentifier.h"

#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine::Audio
{
	struct AudioAsset;

	struct Data
	{
		Data(const Identifier identifier)
			: m_identifier(identifier)
		{
		}

		virtual ~Data()
		{
		}

		Data(const Data&) = delete;
		Data& operator=(const Data&) = delete;

		[[nodiscard]] Identifier GetIdentifier() const
		{
			return m_identifier;
		}

		virtual bool Load(const ConstByteView data) = 0;

		[[nodiscard]] virtual bool IsLoaded() const = 0;
		[[nodiscard]] virtual bool IsValid() const = 0;
	protected:
		Identifier m_identifier;
	};
}
