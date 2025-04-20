#include <FontRendering/Point.h>
#include <FontRendering/FontWeight.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

namespace ngine::Font
{
	bool Point::Serialize(const Serialization::Reader reader)
	{
		float value;
		if (reader.SerializeInPlace(value))
		{
			*this = FromValue(value);
			return true;
		}
		return false;
	}

	bool Point::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(GetPoints());
	}

	bool Weight::Serialize(const Serialization::Reader reader)
	{
		uint16 value;
		if (reader.SerializeInPlace(value))
		{
			*this = Weight{value};
			return true;
		}
		return false;
	}

	bool Weight::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(GetValue());
	}
}
