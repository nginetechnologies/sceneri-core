#pragma once

#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/GetNumericSize.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Renderer/PushConstants/ForwardDeclarations/PushConstantValue.h>

namespace ngine::Rendering
{
	struct PushConstantDefinition;

	struct PushConstantsData
	{
		inline static constexpr uint16 Capacity = 128;
		inline static constexpr uint16 Alignment = 16;

		using SizeType = Memory::UnsignedNumericSize<Capacity>;
		using ViewType = TByteView<ByteType, SizeType>;
		using ConstViewType = TByteView<const ByteType, SizeType>;
		struct Container
		{
			alignas(Alignment) ByteType m_storage[Capacity];
		};

		[[nodiscard]] ConstViewType GetPushConstantsData() const
		{
			return ConstViewType::Make(m_data).GetSubView((SizeType)0u, m_size);
		}

		[[nodiscard]] ViewType GetPushConstantsData()
		{
			return ViewType::Make(m_data).GetSubView((SizeType)0u, m_size);
		}

		void ResetToDefaults(const ArrayView<const PushConstantDefinition, uint8> pushConstantDefinitions);
		bool Serialize(const Serialization::Reader reader, const ArrayView<const PushConstantDefinition, uint8>& pushConstantDefinitions);
		bool Serialize(const Serialization::Writer writer, const ArrayView<const PushConstantDefinition, uint8>& pushConstantDefinitions) const;
	protected:
		Container m_data;
		SizeType m_size;
	};
}
