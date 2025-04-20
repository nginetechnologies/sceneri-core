#include <Widgets/Style/Entry.h>
#include <Widgets/Style/ComputedStylesheet.h>
#include <Widgets/Style/ReferenceValue.h>
#include <Widgets/Style/SizeAxis.h>
#include <Widgets/Style/Size.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Reflection/GenericType.h>

#include <Common/Memory/Serialization/Variant.h>
#include <Common/System/Query.h>

namespace ngine::Widgets::Style
{
	bool Entry::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<ConstStringView> string = reader.ReadInPlace<ConstStringView>())
		{
			ModifierValues& modifier = FindOrEmplaceExactModifierMatch(Modifier::None);
			modifier.ParseFromCSS(*string);
			m_valueTypeMask |= modifier.GetValueTypeMask();
			m_dynamicValueTypeMask |= modifier.GetDynamicValueTypeMask();
			return true;
		}
		return false;
	}

	bool ComputedStylesheet::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<ConstStringView> string = reader.ReadInPlace<ConstStringView>())
		{
			ParseFromCSS(*string);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool SizeAxis::Serialize(const Serialization::Reader reader)
	{
		return reader.SerializeInPlace(m_value);
	}

	bool SizeAxis::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(m_value);
	}

	bool SizeAxisExpression::Serialize(const Serialization::Reader reader)
	{
		SizeAxis sizeAxis;
		const bool result = reader.SerializeInPlace(sizeAxis);
		*this = Move(sizeAxis);
		return result;
	}

	bool SizeAxisExpression::Serialize(Serialization::Writer writer) const
	{
		const Optional<SizeAxis> sizeAxis = Get();
		if (!sizeAxis.IsValid() || !sizeAxis->m_value.HasValue())
		{
			return false;
		}
		return writer.SerializeInPlace(*sizeAxis);
	}

	bool Size::Serialize(const Serialization::Reader reader)
	{
		const Serialization::Value& currentElement = reader.GetValue();
		Assert(currentElement.IsArray());
		Assert(currentElement.Size() == 2);
		Serialization::Reader xReader(currentElement[0], reader.GetDocument());
		xReader.SerializeInPlace(x);
		Serialization::Reader yReader(currentElement[1], reader.GetDocument());
		yReader.SerializeInPlace(y);
		return true;
	}

	bool Size::Serialize(Serialization::Writer writer) const
	{
		const Optional<SizeAxis> sizeAxisX = x.Get();
		const Optional<SizeAxis> sizeAxisY = y.Get();
		if (!sizeAxisX.IsValid() || !sizeAxisX->m_value.HasValue() || !sizeAxisY.IsValid() || !sizeAxisY->m_value.HasValue())
		{
			return false;
		}

		Serialization::Value& currentElement = writer.GetValue();
		currentElement.SetArray();
		currentElement = Serialization::Value(rapidjson::Type::kArrayType);
		currentElement.Reserve(2, writer.GetDocument().GetAllocator());

		Serialization::Value value;
		Serialization::Writer valueWriter(value, writer.GetData());
		valueWriter.SerializeInPlace(*sizeAxisX);
		currentElement.PushBack(Move(value), writer.GetDocument().GetAllocator());
		valueWriter.SerializeInPlace(*sizeAxisY);
		currentElement.PushBack(Move(value), writer.GetDocument().GetAllocator());
		return true;
	}
}

namespace ngine::Widgets
{
	template<uint8 ReferenceDPI>
	bool TReferenceValue<ReferenceDPI>::Serialize(const Serialization::Reader reader)
	{
		return reader.SerializeInPlace(m_value);
	}

	template<uint8 ReferenceDPI>
	bool TReferenceValue<ReferenceDPI>::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(m_value);
	}

	template struct TReferenceValue<DpiReference>;
}
