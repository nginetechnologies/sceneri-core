#include <Renderer/PushConstants/PushConstantsData.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

#include <Common/Reflection/GenericType.h>

#include <Renderer/Assets/Material/MaterialAsset.h>

namespace ngine::Rendering
{
	bool PushConstantDefinition::Serialize(const Serialization::Reader serializer)
	{
		serializer.Serialize("size", m_size);
		serializer.Serialize("stages", m_shaderStages);
		serializer.Serialize("default_value", m_defaultValue);
		if (!serializer.Serialize("preset", m_preset))
		{
			m_preset = PushConstantPreset::Unknown;
		}

		m_alignment = (uint8)m_defaultValue.m_value.Get().GetTypeDefinition().GetAlignment();
		return true;
	}

	bool PushConstantDefinition::Serialize(Serialization::Writer serializer) const
	{
		serializer.Serialize("size", m_size);
		serializer.Serialize("stages", m_shaderStages);
		serializer.Serialize("default_value", m_defaultValue);
		if (m_preset != PushConstantPreset::Unknown)
		{
			serializer.Serialize("preset", m_preset);
		}
		return true;
	}

	bool PushConstant::Serialize(const Serialization::Reader serializer)
	{
		PushConstantType type;
		[[maybe_unused]] const bool readType = serializer.Serialize("type", type);
		Assert(readType);
		switch (type)
		{
			case PushConstantType::Color:
			{
				Math::Color color;
				serializer.Serialize("color", color);
				m_value = color;
			}
			break;
			case PushConstantType::Float:
			{
				float value;
				serializer.Serialize("value", value);
				m_value = value;
			}
			break;
			case PushConstantType::Vector2f:
			{
				Math::Vector2f value;
				serializer.Serialize("value", value);
				m_value = value;
			}
			break;
			case PushConstantType::Int32:
			{
				int32 value;
				serializer.Serialize("value", value);
				m_value = value;
			}
			break;
			case PushConstantType::UInt32:
			{
				uint32 value;
				serializer.Serialize("value", value);
				m_value = value;
			}
			break;
		}
		return true;
	}

	bool PushConstant::Serialize(Serialization::Writer serializer) const
	{
		const PushConstantType type = GetType();
		[[maybe_unused]] const bool wasTypeWritten = serializer.Serialize("type", type);
		Assert(wasTypeWritten);
		switch (type)
		{
			case PushConstantType::Color:
				return serializer.Serialize("color", m_value.GetExpected<Math::Color>());
			case PushConstantType::Float:
				return serializer.Serialize("value", m_value.GetExpected<float>());
			case PushConstantType::Vector2f:
				return serializer.Serialize("value", m_value.GetExpected<Math::Vector2f>());
			case PushConstantType::Int32:
				return serializer.Serialize("value", m_value.GetExpected<int32>());
			case PushConstantType::UInt32:
				return serializer.Serialize("value", m_value.GetExpected<uint32>());
		}
		ExpectUnreachable();
	}

	void PushConstantsData::ResetToDefaults(const ArrayView<const PushConstantDefinition, uint8> pushConstantDefinitions)
	{
		const SizeType pushConstantSize = pushConstantDefinitions.Count(
			[](const PushConstantDefinition& __restrict pushConstant)
			{
				return pushConstant.m_size;
			}
		);
		Assert(pushConstantSize <= Capacity);
		m_size = pushConstantSize;

		TByteView<ByteType, size> pushConstantDataView = GetPushConstantsData();
		for (const PushConstantDefinition& __restrict pushConstantDefinition : pushConstantDefinitions)
		{
			const ConstAnyView defaultValue = pushConstantDefinition.m_defaultValue.m_value.Get();
			const ConstByteView byteView = defaultValue.GetByteView();

			pushConstantDataView.CopyFrom(byteView);
			pushConstantDataView += byteView.GetDataSize();
		}
	}

	bool PushConstantsData::Serialize(
		const Serialization::Reader reader, const ArrayView<const PushConstantDefinition, uint8>& pushConstantDefinitions
	)
	{
		const SizeType pushConstantSize = pushConstantDefinitions.Count(
			[](const PushConstantDefinition& __restrict pushConstant)
			{
				return pushConstant.m_size;
			}
		);
		Assert(pushConstantSize <= Capacity);
		m_size = pushConstantSize;

		ViewType pushConstantDataView = GetPushConstantsData();
		uint8 index{0};
		for (const Serialization::Reader constantSerializer : reader.GetArrayView())
		{
			const PushConstantDefinition& __restrict pushConstantDefinition = pushConstantDefinitions[index];

			switch (pushConstantDefinition.GetType())
			{
				case PushConstantType::Color:
				{
					const Optional<Math::Color> value = constantSerializer.Read<Math::Color>("color");
					Assert(value.IsValid() && pushConstantDataView.GetDataSize() >= sizeof(Math::Color));
					if (UNLIKELY(!value.IsValid() || pushConstantDataView.GetDataSize() < sizeof(Math::Color)))
					{
						return false;
					}

					pushConstantDataView.CopyFrom(ConstByteView::Make(*value));
					pushConstantDataView += sizeof(Math::Color);
				}
				break;
				case PushConstantType::Float:
				{
					const Optional<Serialization::Reader> valueSerializer = constantSerializer.FindSerializer("value");
					Assert(valueSerializer.IsValid());
					if (UNLIKELY(!valueSerializer.IsValid()))
					{
						return false;
					}

					const Serialization::Value& serializedValue = valueSerializer->GetValue().GetValue();
					Assert(serializedValue.IsNumber());
					if (UNLIKELY(!serializedValue.IsNumber()))
					{
						return false;
					}

					Assert(pushConstantDataView.GetDataSize() >= sizeof(float));
					if (UNLIKELY(pushConstantDataView.GetDataSize() < sizeof(float)))
					{
						return false;
					}

					const float value = serializedValue.GetFloat();
					pushConstantDataView.CopyFrom(ConstByteView::Make(value));
					pushConstantDataView += sizeof(float);
				}
				break;
				case PushConstantType::Vector2f:
				{
					const Optional<Math::Vector2f> value = constantSerializer.Read<Math::Vector2f>("value");
					Assert(value.IsValid() && pushConstantDataView.GetDataSize() >= sizeof(Math::Vector2f));
					if (UNLIKELY(!value.IsValid() || pushConstantDataView.GetDataSize() < sizeof(Math::Vector2f)))
					{
						return false;
					}

					pushConstantDataView.CopyFrom(ConstByteView::Make(value.Get()));
					pushConstantDataView += sizeof(Math::Vector2f);
				}
				break;
				case PushConstantType::Int32:
				{
					const Optional<Serialization::Reader> valueSerializer = constantSerializer.FindSerializer("value");
					Assert(valueSerializer.IsValid());
					if (UNLIKELY(!valueSerializer.IsValid()))
					{
						return false;
					}

					const Serialization::Value& serializedValue = valueSerializer->GetValue().GetValue();
					Assert(serializedValue.IsNumber());
					if (UNLIKELY(!serializedValue.IsNumber()))
					{
						return false;
					}

					Assert(pushConstantDataView.GetDataSize() >= sizeof(int32));
					if (UNLIKELY(pushConstantDataView.GetDataSize() < sizeof(int32)))
					{
						return false;
					}

					const int32 value = serializedValue.GetInt();
					pushConstantDataView.CopyFrom(ConstByteView::Make(value));
					pushConstantDataView += sizeof(int32);
				}
				break;
				case PushConstantType::UInt32:
				{
					const Optional<Serialization::Reader> valueSerializer = constantSerializer.FindSerializer("value");
					Assert(valueSerializer.IsValid());
					if (UNLIKELY(!valueSerializer.IsValid()))
					{
						return false;
					}

					const Serialization::Value& serializedValue = valueSerializer->GetValue().GetValue();
					Assert(serializedValue.IsNumber());
					if (UNLIKELY(!serializedValue.IsNumber()))
					{
						return false;
					}

					Assert(pushConstantDataView.GetDataSize() >= sizeof(uint32));
					if (UNLIKELY(pushConstantDataView.GetDataSize() < sizeof(uint32)))
					{
						return false;
					}

					const uint32 value = serializedValue.GetUint();
					pushConstantDataView.CopyFrom(ConstByteView::Make(value));
					pushConstantDataView += sizeof(uint32);
				}
				break;
			}
			++index;
		}

		return true;
	}

	bool PushConstantsData::Serialize(
		Serialization::Writer writer, const ArrayView<const PushConstantDefinition, uint8>& pushConstantDefinitions
	) const
	{
		writer.GetValue().SetArray();

		PushConstantsData::ConstViewType pushConstantsData = GetPushConstantsData();
		return writer.SerializeArrayCallbackInPlace(
			[pushConstantDefinitions, pushConstantsData](Serialization::Writer writer, const uint8 index) mutable
			{
				writer.GetValue().SetObject();

				const PushConstantDefinition& __restrict pushConstantDefinition = pushConstantDefinitions[index];
				const PushConstantType type = pushConstantDefinition.m_defaultValue.GetType();
				writer.Serialize("type", type);
				switch (type)
				{
					case PushConstantType::Color:
						if (!writer.Serialize("value", *reinterpret_cast<const Math::Color*>(pushConstantsData.GetData())))
						{
							return false;
						}
						break;
					case PushConstantType::Float:
						if (!writer.Serialize("value", *reinterpret_cast<const float*>(pushConstantsData.GetData())))
						{
							return false;
						}
						break;
					case PushConstantType::Vector2f:
						if (!writer.Serialize("value", *reinterpret_cast<const Math::Vector2f*>(pushConstantsData.GetData())))
						{
							return false;
						}
						break;
					case PushConstantType::Int32:
						if (!writer.Serialize("value", *reinterpret_cast<const int32*>(pushConstantsData.GetData())))
						{
							return false;
						}
						break;
					case PushConstantType::UInt32:
						if (!writer.Serialize("value", *reinterpret_cast<const uint32*>(pushConstantsData.GetData())))
						{
							return false;
						}
						break;
				}
				pushConstantsData += pushConstantDefinition.m_size;
				return true;
			},
			pushConstantDefinitions.GetSize()
		);
	}
}
