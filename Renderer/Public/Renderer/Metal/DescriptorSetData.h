#pragma once

#include <Renderer/Buffers/Buffer.h>
#include <Renderer/Descriptors/DescriptorSetLayoutView.h>
#include <Renderer/Descriptors/DescriptorType.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/ShaderStage.h>

#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/InlineVector.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	namespace Internal
	{
		struct DescriptorSetLayoutData
		{
			struct Binding
			{
				uint16 m_baseIndex;
				uint8 m_batchIndex;
				uint32 m_baseBatchResourceIndex;
				uint32 m_arraySize;
				DescriptorType m_type;
				EnumFlags<ShaderStage> m_stages;
			};
			struct ResourceBatch
			{
				[[nodiscard]] bool operator==(const ResourceBatch& other) const
				{
					return (m_usage == other.m_usage) & (m_stages == other.m_stages);
				}

				MTLResourceUsage m_usage;
				MTLRenderStages m_stages;
				uint32 m_resourceCount;
			};

			EnumFlags<ShaderStage> m_stages;
			InlineVector<Binding, 6, uint16> m_bindings;
			InlineVector<ResourceBatch, 6, uint8> m_resourceBatches;
			id<MTLArgumentEncoder> m_encoder;
			bool m_argumentBuffers;
		};

		struct DescriptorSetData
		{
			DescriptorSetLayoutView m_layout;
			Buffer m_argumentBuffer;

			struct ResourceBatch
			{
				void EmplaceResource(const uint32 index, id<MTLResource> resource)
				{
					uint32 resourceIndex = m_resourceIndices[index];
					if (resourceIndex > 0)
					{
						m_resources[resourceIndex - 1] = resource;
					}
					else
					{
						resourceIndex = m_resources.GetNextAvailableIndex();
						m_resourceIndices[index] = resourceIndex + 1;
						m_resources.EmplaceBack(resource);
					}
				}
				void EmplaceIndirectResource(id<MTLResource> resource)
				{
					m_resources.EmplaceBackUnique(Move(resource));
				}
				[[nodiscard]] id<MTLResource> GetResource(const uint32 index) const
				{
					uint32 resourceIndex = m_resourceIndices[index];
					if (resourceIndex > 0)
					{
						return m_resources[resourceIndex - 1];
					}
					else
					{
						return nil;
					}
				}
				void CopyResourceFrom(const uint32 targetIndex, const uint32 sourceIndex, const ResourceBatch& other)
				{
					EmplaceResource(targetIndex, other.GetResource(sourceIndex));
				}

				InlineVector<id<MTLResource>, 6> m_resources;
				InlineVector<uint32, 6> m_resourceIndices;
			};
			InlineVector<ResourceBatch, 2, uint8> m_resourceBatches;
			InlineVector<id<NSObject>, 6> m_objects;
			InlineVector<size, 6> m_bufferOffsets;
		};
	} // namespace Internal
#elif RENDERER_WEBGPU

	namespace Internal
	{
		struct DescriptorSetLayoutData
		{
			struct Binding
			{
				uint16 m_baseIndex;
				DescriptorType m_type;
			};
			WGPUBindGroupLayout m_bindGroupLayout;
			uint16 m_bindEntryCount;
			InlineVector<Binding, 6, uint16> m_bindings;
		};

		struct DescriptorSetData
		{
			DescriptorSetLayoutView m_layout;
			WGPUBindGroup m_bindGroup;
			InlineVector<WGPUBindGroupEntry, 8, uint16> m_bindGroupEntries;
		};
	} // namespace Internal
#endif
} // namespace ngine::Rendering
