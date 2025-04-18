#include "Assets/Shader/ShaderCache.h"
#include "Assets/Shader/ShaderAsset.h"

#include <Common/Memory/OffsetOf.h>

#include <Engine/Engine.h>
#include <Engine/Asset/AssetType.inl>
#include <Engine/Threading/JobManager.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/File.h>
#include <Common/System/Query.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Index.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Metal/DescriptorSetData.h>

#if RENDERER_METAL
#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS 1
// #define SPIRV_CROSS_NAMESPACE_OVERRIDE SPIRV_CROSS_NAMESPACE

#include <spirv_cross/spirv_msl.hpp>

#import <Foundation/NSString.h>
#endif

namespace ngine::Rendering
{
	LogicalDevice& ShaderCache::GetLogicalDevice()
	{
		return Memory::GetOwnerFromMember(*this, &LogicalDevice::m_shaderCache);
	}

	const LogicalDevice& ShaderCache::GetLogicalDevice() const
	{
		return Memory::GetConstOwnerFromMember(*this, &LogicalDevice::m_shaderCache);
	}

#define SUPPORTS_PIPELINE_CACHE RENDERER_VULKAN

	ShaderCache::ShaderCache()
	{
		if (const Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>())
		{
			RegisterAssetModifiedCallback(*pAssetManager);
		}

#if SUPPORTS_PIPELINE_CACHE
		const IO::Path shaderCacheFilePath = GetShaderCacheFilePath();
		if (shaderCacheFilePath.Exists())
		{
			const IO::File
				shaderCacheFile(shaderCacheFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary, IO::SharingFlags::DisallowWrite);
			const size pipelineSize = (size)shaderCacheFile.GetSize();

			FixedSizeVector<char, size> pipelineCacheData(Memory::ConstructWithSize, Memory::Uninitialized, pipelineSize);
			if (LIKELY(shaderCacheFile.ReadIntoView(pipelineCacheData.GetView())))
			{
#if RENDERER_VULKAN
				const VkPipelineCacheCreateInfo cacheCreateInfo =
					{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, pipelineCacheData.GetSize(), pipelineCacheData.GetData()};

				[[maybe_unused]] const VkResult result = vkCreatePipelineCache(GetLogicalDevice(), &cacheCreateInfo, nullptr, &m_pPipelineCache);
				Assert(result == VK_SUCCESS);
#endif
			}
		}

#if RENDERER_VULKAN
		if (m_pPipelineCache == 0)
		{
			const VkPipelineCacheCreateInfo cacheCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, 0, nullptr};

			[[maybe_unused]] const VkResult result = vkCreatePipelineCache(GetLogicalDevice(), &cacheCreateInfo, nullptr, &m_pPipelineCache);
			Assert(result == VK_SUCCESS);
		}
#endif
#endif
	}

	ShaderCache::~ShaderCache()
	{
	}

	void ShaderCache::Destroy(const LogicalDeviceView logicalDevice)
	{
		SaveToDisk();

		IterateElements(
			[logicalDevice](Shader& shader)
			{
				shader.Destroy(logicalDevice);
			}
		);

#if RENDERER_VULKAN
		vkDestroyPipelineCache(GetLogicalDevice(), m_pPipelineCache, nullptr);
#endif
	}

	void ShaderCache::SaveToDisk() const
	{
#if SUPPORTS_PIPELINE_CACHE
		size pipelineSize;
#if RENDERER_VULKAN
		[[maybe_unused]] const VkResult getSizeResult = vkGetPipelineCacheData(GetLogicalDevice(), m_pPipelineCache, &pipelineSize, nullptr);
		Assert(getSizeResult == VK_SUCCESS);
		pipelineSize = 0;
#endif

		FixedSizeVector<char> pipelineCacheData(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)pipelineSize);

#if RENDERER_VULKAN
		[[maybe_unused]] const VkResult result =
			vkGetPipelineCacheData(GetLogicalDevice(), m_pPipelineCache, &pipelineSize, pipelineCacheData.GetData());
		if (result != VK_SUCCESS)
		{
			return;
		}
#endif

		const IO::Path shaderCacheFilePath = GetShaderCacheFilePath();
		IO::Path(shaderCacheFilePath.GetParentPath()).CreateDirectories();
		const IO::File shaderCacheFile(shaderCacheFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
		shaderCacheFile.Write(pipelineCacheData.GetView());
#endif
	}

#if PLATFORM_APPLE && RENDERER_METAL
	[[nodiscard]] MTLLanguageVersion GetMetalLanguageVersion(const Rendering::LogicalDeviceView logicalDevice)
	{
		using LanguagePair = Pair<MTLGPUFamily, MTLLanguageVersion>;
		InlineVector<LanguagePair, 6> familyLanguagePairs;

		if (@available(macOS 13, iOS 16, *))
		{
			// Apple A16 and later
			familyLanguagePairs.EmplaceBack(LanguagePair{MTLGPUFamilyApple6, MTLLanguageVersion3_0});
		}

		familyLanguagePairs.CopyEmplaceRange(
			familyLanguagePairs.end(),
			Memory::DefaultConstruct,
			Array<const LanguagePair, 5>{// Apple A15
		                               LanguagePair{MTLGPUFamilyApple5, MTLLanguageVersion2_4},
		                               // Apple A14
		                               LanguagePair{MTLGPUFamilyApple4, MTLLanguageVersion2_3},
		                               // Apple A13
		                               LanguagePair{MTLGPUFamilyApple3, MTLLanguageVersion2_2},
		                               // Apple A12
		                               LanguagePair{MTLGPUFamilyApple2, MTLLanguageVersion2_1},
		                               // Apple A7
		                               LanguagePair{MTLGPUFamilyApple1, MTLLanguageVersion1_1}
		  }.GetDynamicView()
		);

		for (const auto& familyLanguagePair : familyLanguagePairs)
		{
			if ([logicalDevice supportsFamily:familyLanguagePair.key])
			{
				return familyLanguagePair.value;
			}
		}

		return MTLLanguageVersion1_1;
	}
#endif

	IO::Path ShaderCache::GetShaderCacheFilePath() const
	{
		return IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("ShaderCache"), MAKE_PATH("Cache.bin"));
	}

	Threading::Job* ShaderCache::FindOrLoad(
		const Asset::Guid guid,
		const ShaderStage stage,
		const ConstZeroTerminatedStringView entryPointName,
		const PipelineLayoutView pipelineLayout,
		const StageInfo stageInfo,
		ShaderRequesters::ListenerData&& listenerData
	)
	{
		const ShaderIdentifier identifier = BaseType::FindOrRegisterAsset(
			guid,
			[](const ShaderIdentifier, const Asset::Guid) -> Shader
			{
				return Shader();
			}
		);

		ShaderRequesters* pShaderRequesters = nullptr;

		{
			Threading::SharedLock lock(m_shaderRequesterMutex);
			decltype(m_shaderRequesterMap)::iterator it = m_shaderRequesterMap.Find(identifier);
			if (it != m_shaderRequesterMap.end())
			{
				pShaderRequesters = it->second;
			}
		}
		if (pShaderRequesters == nullptr)
		{
			Threading::UniqueLock lock(m_shaderRequesterMutex);
			decltype(m_shaderRequesterMap)::iterator it = m_shaderRequesterMap.Find(identifier);
			if (it != m_shaderRequesterMap.end())
			{
				pShaderRequesters = it->second;
			}
			else
			{
				it = m_shaderRequesterMap.Emplace(ShaderIdentifier(identifier), UniqueRef<ShaderRequesters>::Make());
				pShaderRequesters = it->second;
			}
		}

		pShaderRequesters->Emplace(Forward<ShaderRequesters::ListenerData>(listenerData));

		Shader& shader = GetAssetData(identifier);
		if (shader.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = pShaderRequesters->Execute(listenerData.m_identifier, shader);
			Assert(wasExecuted);
			return nullptr;
		}
		else if (m_loadingShaders.Set(identifier))
		{
			if (GetAssetData(identifier).IsValid())
			{
				m_loadingShaders.Clear(identifier);
				[[maybe_unused]] const bool wasExecuted = pShaderRequesters->Execute(listenerData.m_identifier, shader);
				Assert(wasExecuted);
				return nullptr;
			}
			else
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				return assetManager.RequestAsyncLoadAssetBinary(
					guid,
					Threading::JobPriority::LoadShader,
					[this, identifier, stage, entryPointName, pipelineLayout, stageInfo](const ConstByteView data)
					{
						ShaderView shader;

						Assert(data.HasElements());
						if (LIKELY(data.HasElements()))
						{
#if RENDERER_WEBGPU
							// WebGPU relies on a zero-terminated string for source
							Vector<ByteType, size> storedData(Memory::ConstructWithSize, Memory::Uninitialized, data.GetDataSize() + 1);
							ByteView(storedData.GetSubView(0, data.GetDataSize())).CopyFrom(data);
							storedData[data.GetDataSize()] = '\0';
							shader = GetAssetData(identifier
						  ) = CreateShaderInternal(storedData.GetView(), stage, entryPointName, pipelineLayout, stageInfo);
#elif RENDERER_METAL
							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							IO::Path assetPath = assetManager.GetAssetPath(GetAssetGuid(identifier));
							assetPath = IO::Path::Merge(assetPath.GetWithoutExtensions(), MAKE_PATH(".msl"));
							if (assetPath.Exists())
							{
								IO::File file(assetPath, IO::AccessModeFlags::ReadBinary);
								Assert(file.IsValid());
								Vector<char, IO::File::SizeType> fileData(Memory::ConstructWithSize, Memory::Uninitialized, file.GetSize() + 1);
								[[maybe_unused]] const bool wasRead = file.ReadIntoView(fileData.GetView().GetSubView(0, fileData.GetSize() - 1));
								Assert(wasRead);
								fileData.GetLastElement() = '\0';

								MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
#if PLATFORM_APPLE_VISIONOS
								compileOptions.mathMode = MTLMathModeFast;
#else
								if (@available(macOS 15.0, visionOS 2.0, iOS 18.0, *))
								{
									compileOptions.mathMode = MTLMathModeFast;
								}
								else
								{
									compileOptions.fastMathEnabled = true;
								}
#endif
								compileOptions.languageVersion = GetMetalLanguageVersion(GetLogicalDevice());

								NSError* error{nil};
								NSString* source = [NSString stringWithUTF8String:fileData.GetData()];
								id<MTLLibrary> library = [GetLogicalDevice() newLibraryWithSource:source options:compileOptions error:&error];

								shader = GetAssetData(identifier) = Shader(ShaderView{library, [library newFunctionWithName:@"main0"]});
							}
							else
							{
								shader = GetAssetData(identifier) = CreateShaderInternal(data, stage, entryPointName, pipelineLayout, stageInfo);
							}
#else
							shader = GetAssetData(identifier) = CreateShaderInternal(data, stage, entryPointName, pipelineLayout, stageInfo);
#endif
						}

						{
							Threading::SharedLock lock(m_shaderRequesterMutex);
							auto it = m_shaderRequesterMap.Find(identifier);
							Assert(it != m_shaderRequesterMap.end());
							if (LIKELY(it != m_shaderRequesterMap.end()))
							{
								(*it->second)(shader);
							}

							[[maybe_unused]] const bool wasCleared = m_loadingShaders.Clear(identifier);
							Assert(wasCleared);
						}
					}
				);
			}
		}
		else
		{
			return nullptr;
		}
	}

	Shader ShaderCache::CreateShaderInternal(
		const ConstByteView shaderFileContents,
		[[maybe_unused]] const ShaderStage stage,
		[[maybe_unused]] const ConstZeroTerminatedStringView entryPointName,
		[[maybe_unused]] const PipelineLayoutView pipelineLayout,
		[[maybe_unused]] const StageInfo stageInfo
	)
	{
		Rendering::LogicalDevice& logicalDevice = GetLogicalDevice();

#if RENDERER_VULKAN || RENDERER_WEBGPU
		Shader shader(logicalDevice, shaderFileContents);
		Assert(shader.IsValid());
		return shader;
#elif RENDERER_METAL
		// TODO: Incorporate with pipeline cache
		// TODO: Support precompiled shaders that ship with the app too.

		id<MTLDevice> device = logicalDevice;

		std::vector<uint32> spirvData;
		spirvData.resize(shaderFileContents.GetDataSize() / sizeof(uint32));
		ByteView{spirvData.data(), spirvData.size()}.CopyFrom(ConstByteView(shaderFileContents));

		const spv::ExecutionModel executionModel = [](const Rendering::ShaderStage stage) -> spv::ExecutionModel
		{
			switch (stage)
			{
				case Rendering::ShaderStage::Vertex:
					return spv::ExecutionModel::ExecutionModelVertex;
				case Rendering::ShaderStage::Fragment:
					return spv::ExecutionModel::ExecutionModelFragment;
				case Rendering::ShaderStage::Compute:
					return spv::ExecutionModel::ExecutionModelGLCompute;
				case Rendering::ShaderStage::Geometry:
					return spv::ExecutionModel::ExecutionModelGeometry;

				case Rendering::ShaderStage::TessellationControl:
					return spv::ExecutionModel::ExecutionModelTessellationControl;
				case Rendering::ShaderStage::TessellationEvaluation:
					return spv::ExecutionModel::ExecutionModelTessellationEvaluation;

				case Rendering::ShaderStage::RayGeneration:
					return spv::ExecutionModel::ExecutionModelRayGenerationKHR;
				case Rendering::ShaderStage::RayAnyHit:
					return spv::ExecutionModel::ExecutionModelAnyHitKHR;
				case Rendering::ShaderStage::RayClosestHit:
					return spv::ExecutionModel::ExecutionModelClosestHitKHR;
				case Rendering::ShaderStage::RayIntersection:
					return spv::ExecutionModel::ExecutionModelIntersectionKHR;
				case Rendering::ShaderStage::RayMiss:
					return spv::ExecutionModel::ExecutionModelMissKHR;
				case Rendering::ShaderStage::RayCallable:
					return spv::ExecutionModel::ExecutionModelCallableKHR;

				case Rendering::ShaderStage::Invalid:
				case Rendering::ShaderStage::Count:
				case Rendering::ShaderStage::All:
					ExpectUnreachable();
			}
		}(stage);

		SPIRV_CROSS_NAMESPACE::CompilerMSL* pMSLCompiler = new SPIRV_CROSS_NAMESPACE::CompilerMSL(spirvData.data(), spirvData.size());

		pMSLCompiler->set_entry_point(std::string(entryPointName, entryPointName.GetSize()), executionModel);

		SPIRV_CROSS_NAMESPACE::CompilerMSL::Options mslOptions;
		mslOptions.platform = PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST || PLATFORM_APPLE_VISIONOS
		                        ? SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::Platform::macOS
		                        : SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::Platform::iOS;

		switch (GetMetalLanguageVersion(logicalDevice))
		{
			case MTLLanguageVersion3_2:
				mslOptions.set_msl_version(3, 2);
				break;
			case MTLLanguageVersion3_1:
				mslOptions.set_msl_version(3, 1);
				break;
			case MTLLanguageVersion3_0:
				mslOptions.set_msl_version(3, 0);
				break;
			case MTLLanguageVersion2_4:
				mslOptions.set_msl_version(2, 4);
				break;
			case MTLLanguageVersion2_3:
				mslOptions.set_msl_version(2, 3);
				break;
			case MTLLanguageVersion2_2:
				mslOptions.set_msl_version(2, 2);
				break;
			case MTLLanguageVersion2_1:
				mslOptions.set_msl_version(2, 1);
				break;
			case MTLLanguageVersion2_0:
				mslOptions.set_msl_version(2, 1);
				break;
			case MTLLanguageVersion1_2:
				mslOptions.set_msl_version(1, 2);
				break;
			case MTLLanguageVersion1_1:
				mslOptions.set_msl_version(1, 1);
				break;
#if !PLATFORM_APPLE_MACOS && !PLATFORM_APPLE_MACCATALYST && !PLATFORM_APPLE_VISIONOS
			case MTLLanguageVersion1_0:
				Assert(false, "Metal version 1.0 is not supported");
				break;
#endif
		}

		mslOptions.vertex_index_type = TypeTraits::IsSame<Rendering::Index, uint32>
		                                 ? SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::IndexType::UInt32
		                                 : SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::IndexType::UInt16;

		uint8 argumentBuffersTier;
		if (mslOptions.supports_msl_version(3, 0))
		{
			argumentBuffersTier = 2;
		}
		else
		{
			argumentBuffersTier = 0;
		}

		switch (argumentBuffersTier)
		{
			case 0:
				mslOptions.argument_buffers = 0;
				break;
			case 1:
				mslOptions.argument_buffers = 1;
				mslOptions.argument_buffers_tier = SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::ArgumentBuffersTier::Tier1;
				break;
			case 2:
				mslOptions.argument_buffers = 1;
				mslOptions.argument_buffers_tier = SPIRV_CROSS_NAMESPACE::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
				break;
		}
		mslOptions.force_active_argument_buffer_resources = true;
		// mslOptions.pad_argument_buffer_resources = true;

		mslOptions.pad_fragment_output_components = true;

		// mslOptions.use_framebuffer_fetch_subpasses = true;

		mslOptions.texture_buffer_native = true;

		mslOptions.ios_support_base_vertex_instance = true;
		mslOptions.ios_use_simdgroup_functions = true;

		// Establish the MSL options for the compiler
		// This needs to be done in two steps...for CompilerMSL and its superclass.
		pMSLCompiler->set_msl_options(mslOptions);

		auto scOpts = pMSLCompiler->get_common_options();
		scOpts.vertex.flip_vert_y = true;
		pMSLCompiler->set_common_options(scOpts);

		Internal::PipelineLayoutData* pPipelineLayoutData = pipelineLayout;

		// Push constants come after all descriptors in our case
		// New rule could be: push constants -> vertex buffers -> descriptors
		// Push constants first so we don't have to rebind them

		// Add shader inputs and outputs
		switch (stage)
		{
			case Rendering::ShaderStage::Vertex:
			{
				const VertexInfo& __restrict vertexInfo = stageInfo.GetExpected<VertexInfo>();
				// Specify vertex attributes
				for (const VertexInputAttributeDescription& vertexInputAttribute : vertexInfo.m_inputAttributes)
				{
					SPIRV_CROSS_NAMESPACE::MSLShaderInterfaceVariable shaderInput;
					shaderInput.location = vertexInputAttribute.shaderLocation;
					shaderInput.component = 0;
					shaderInput.builtin = spv::BuiltInMax;
					shaderInput.vecsize = 0;

					Assert(shaderInput.location == vertexInputAttribute.shaderLocation);
					switch (vertexInfo.m_inputBindings[(uint8)vertexInputAttribute.binding].inputRate)
					{
						case VertexInputRate::Vertex:
							shaderInput.rate = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_RATE_PER_VERTEX;
							break;
						case VertexInputRate::Instance:
							shaderInput.rate = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_RATE_PER_PRIMITIVE;
							break;
					}

					pMSLCompiler->add_msl_shader_input(shaderInput);
				}
			}
			break;
			case ShaderStage::Fragment:
			{
				const FragmentInfo& __restrict fragmentInfo = stageInfo.GetExpected<FragmentInfo>();
				const Internal::RenderPassData* __restrict pRenderPassData = fragmentInfo.m_renderPass;

				const ArrayView<const uint8> subpassColorAttachmentIndices =
					pRenderPassData->m_subpassColorAttachmentIndices[fragmentInfo.m_subpassIndex];
				for (const uint8 attachmentIndex : subpassColorAttachmentIndices)
				{
					const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[attachmentIndex];
					const FormatInfo& __restrict attachmentFormatInfo = GetFormatInfo(attachmentDescription.m_format);

					SPIRV_CROSS_NAMESPACE::MSLShaderInterfaceVariable shaderOutput;
					shaderOutput.location = attachmentIndex;
					shaderOutput.component = attachmentFormatInfo.m_componentCount;
					shaderOutput.builtin = spv::BuiltInMax;
					shaderOutput.vecsize = 0;

					shaderOutput.rate = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_RATE_PER_VERTEX;

					if (attachmentFormatInfo.m_flags.AreAllSet(FormatFlags::Unsigned | FormatFlags::Packed8))
					{
						shaderOutput.format = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_FORMAT_UINT8;
					}
					else if (attachmentFormatInfo.m_flags.AreAllSet(FormatFlags::Unsigned | FormatFlags::Packed16))
					{
						shaderOutput.format = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_FORMAT_UINT16;
					}
					else if (attachmentFormatInfo.m_flags.AreAllSet(FormatFlags::Packed16))
					{
						shaderOutput.format = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_FORMAT_ANY16;
					}
					else if (attachmentFormatInfo.m_flags.AreAllSet(FormatFlags::Packed32))
					{
						shaderOutput.format = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_FORMAT_ANY32;
					}
					else
					{
						shaderOutput.format = SPIRV_CROSS_NAMESPACE::MSL_SHADER_VARIABLE_FORMAT_OTHER;
					}
					pMSLCompiler->add_msl_shader_output(shaderOutput);
				}
			}
			break;

			case Rendering::ShaderStage::Compute:
			case Rendering::ShaderStage::Geometry:
			case Rendering::ShaderStage::TessellationControl:
			case Rendering::ShaderStage::TessellationEvaluation:
			case Rendering::ShaderStage::RayGeneration:
			case Rendering::ShaderStage::RayAnyHit:
			case Rendering::ShaderStage::RayClosestHit:
			case Rendering::ShaderStage::RayIntersection:
			case Rendering::ShaderStage::RayMiss:
			case Rendering::ShaderStage::RayCallable:
				break;

			case Rendering::ShaderStage::Invalid:
			case Rendering::ShaderStage::Count:
			case Rendering::ShaderStage::All:
				ExpectUnreachable();
		}

		uint8 resourceBindingOffset;
		switch (stage)
		{
			case Rendering::ShaderStage::Vertex:
			{
				const VertexInfo& __restrict vertexInfo = stageInfo.GetExpected<VertexInfo>();
				resourceBindingOffset = (uint8)vertexInfo.m_inputBindings.GetSize();
			}
			break;
			case Rendering::ShaderStage::Fragment:
				resourceBindingOffset = 0;
				break;
			case Rendering::ShaderStage::Compute:
			case Rendering::ShaderStage::Geometry:
				resourceBindingOffset = 0;
				break;

			case Rendering::ShaderStage::TessellationControl:
			case Rendering::ShaderStage::TessellationEvaluation:
				resourceBindingOffset = 0;
				break;

			case Rendering::ShaderStage::RayGeneration:
			case Rendering::ShaderStage::RayAnyHit:
			case Rendering::ShaderStage::RayClosestHit:
			case Rendering::ShaderStage::RayIntersection:
			case Rendering::ShaderStage::RayMiss:
			case Rendering::ShaderStage::RayCallable:
				resourceBindingOffset = 0;
				break;

			case Rendering::ShaderStage::Invalid:
			case Rendering::ShaderStage::Count:
			case Rendering::ShaderStage::All:
				ExpectUnreachable();
		}

		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts = pPipelineLayoutData->m_descriptorSetLayouts.GetView();
		for (const DescriptorSetLayoutView& descriptorSetLayout : descriptorSetLayouts)
		{
			const Internal::DescriptorSetLayoutData* pDescriptorSetLayoutData = descriptorSetLayout;
			if (pDescriptorSetLayoutData->m_stages.IsSet(stage))
			{
				const uint8 descriptorSetIndex = descriptorSetLayouts.GetIteratorIndex(&descriptorSetLayout);

				if (mslOptions.supports_msl_version(3, 0))
				{
					{
						SPIRV_CROSS_NAMESPACE::MSLResourceBinding resourceBinding;
						resourceBinding.stage = executionModel;
						resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Struct;
						resourceBinding.desc_set = descriptorSetIndex;
						resourceBinding.binding = SPIRV_CROSS_NAMESPACE::kArgumentBufferBinding;
						resourceBinding.count = 1;
						resourceBinding.msl_buffer = resourceBindingOffset;
						pMSLCompiler->add_msl_resource_binding(resourceBinding);
					}

					uint32 localResourceIndex = 0;

					for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
					{
						const uint16 bindingIndex = pDescriptorSetLayoutData->m_bindings.GetIteratorIndex(&descriptorSetBinding);

						SPIRV_CROSS_NAMESPACE::MSLResourceBinding resourceBinding;
						resourceBinding.stage = executionModel;
						switch (descriptorSetBinding.m_type)
						{
							case DescriptorType::Sampler:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Sampler;
								break;
							case DescriptorType::CombinedImageSampler:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::SampledImage;
								break;
							case DescriptorType::SampledImage:
							case DescriptorType::StorageImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
							case DescriptorType::StorageTexelBuffer:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Image;
								break;
							case DescriptorType::UniformBuffer:
							case DescriptorType::StorageBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBufferDynamic:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Void;
								break;

							case DescriptorType::AccelerationStructure:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::AccelerationStructure;
								break;
						}
						resourceBinding.desc_set = descriptorSetIndex;
						resourceBinding.binding = bindingIndex;
						resourceBinding.count = descriptorSetBinding.m_arraySize;

						switch (descriptorSetBinding.m_type)
						{
							case DescriptorType::Sampler:
								resourceBinding.msl_sampler = localResourceIndex;
								localResourceIndex += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::SampledImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
								resourceBinding.msl_texture = localResourceIndex;
								localResourceIndex += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::UniformBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBuffer:
							case DescriptorType::StorageBufferDynamic:
							case DescriptorType::AccelerationStructure:
								resourceBinding.msl_buffer = localResourceIndex;
								localResourceIndex += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::CombinedImageSampler:
							{
								resourceBinding.msl_texture = localResourceIndex;
								resourceBinding.msl_sampler = localResourceIndex + 1;
								localResourceIndex += descriptorSetBinding.m_arraySize * 2;
							}
							break;
							case DescriptorType::StorageImage:
							case DescriptorType::StorageTexelBuffer:
								resourceBinding.msl_texture = localResourceIndex;
								localResourceIndex += descriptorSetBinding.m_arraySize;
								break;
						}

						pMSLCompiler->add_msl_resource_binding(resourceBinding);
					}

					resourceBindingOffset++;
				}
				else
				{
					for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
					{
						if (descriptorSetBinding.m_stages.IsNotSet(stage))
						{
							continue;
						}

						const uint16 bindingIndex = pDescriptorSetLayoutData->m_bindings.GetIteratorIndex(&descriptorSetBinding);

						SPIRV_CROSS_NAMESPACE::MSLResourceBinding resourceBinding;
						resourceBinding.stage = executionModel;
						switch (descriptorSetBinding.m_type)
						{
							case DescriptorType::Sampler:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Sampler;
								break;
							case DescriptorType::CombinedImageSampler:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::SampledImage;
								break;
							case DescriptorType::SampledImage:
							case DescriptorType::StorageImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
							case DescriptorType::StorageTexelBuffer:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Image;
								break;
							case DescriptorType::UniformBuffer:
							case DescriptorType::StorageBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBufferDynamic:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Void;
								break;

							case DescriptorType::AccelerationStructure:
								resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::AccelerationStructure;
								break;
						}
						resourceBinding.desc_set = descriptorSetIndex;
						resourceBinding.binding = bindingIndex;
						resourceBinding.count = descriptorSetBinding.m_arraySize;

						switch (descriptorSetBinding.m_type)
						{
							case DescriptorType::Sampler:
								resourceBinding.msl_sampler = resourceBindingOffset;
								resourceBindingOffset += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::SampledImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
								resourceBinding.msl_texture = resourceBindingOffset;
								resourceBindingOffset += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::UniformBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBuffer:
							case DescriptorType::StorageBufferDynamic:
							case DescriptorType::AccelerationStructure:
								resourceBinding.msl_buffer = resourceBindingOffset;
								resourceBindingOffset += descriptorSetBinding.m_arraySize;
								break;
							case DescriptorType::CombinedImageSampler:
							{
								resourceBinding.msl_texture = resourceBindingOffset;
								resourceBinding.msl_sampler = resourceBindingOffset + 1;
								resourceBindingOffset += descriptorSetBinding.m_arraySize * 2;
							}
							break;
							case DescriptorType::StorageImage:
							case DescriptorType::StorageTexelBuffer:
								resourceBinding.msl_texture = resourceBindingOffset;
								resourceBindingOffset += descriptorSetBinding.m_arraySize;
								break;
						}

						pMSLCompiler->add_msl_resource_binding(resourceBinding);
					}
				}
			}
		}

		if (pPipelineLayoutData->m_pushConstantsStages.IsSet(stage))
		{
			SPIRV_CROSS_NAMESPACE::MSLResourceBinding resourceBinding;
			resourceBinding.stage = executionModel;
			resourceBinding.basetype = SPIRV_CROSS_NAMESPACE::SPIRType::Void;
			resourceBinding.desc_set = SPIRV_CROSS_NAMESPACE::ResourceBindingPushConstantDescriptorSet;
			resourceBinding.binding = SPIRV_CROSS_NAMESPACE::ResourceBindingPushConstantBinding;
			resourceBinding.count = 1;
			resourceBinding.msl_buffer = resourceBindingOffset;
			pMSLCompiler->add_msl_resource_binding(resourceBinding);

			resourceBindingOffset++;
		}

		if (mslOptions.supports_msl_version(3, 0))
		{
			for (uint8 i = 0; i < SPIRV_CROSS_NAMESPACE::kMaxArgumentBuffers; ++i)
			{
				pMSLCompiler->set_argument_buffer_device_address_space(i, true);
			}
		}

		std::string msl = pMSLCompiler->compile();
		Assert(!msl.empty());
		if (UNLIKELY_ERROR(msl.empty()))
		{
			return Shader();
		}

		MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
#if PLATFORM_APPLE_VISIONOS
		compileOptions.mathMode = MTLMathModeFast;
#else
		if (@available(macOS 15.0, visionOS 2.0, iOS 18.0, *))
		{
			compileOptions.mathMode = MTLMathModeFast;
		}
		else
		{
			compileOptions.fastMathEnabled = true;
		}
#endif
		compileOptions.languageVersion = GetMetalLanguageVersion(logicalDevice);

		NSError* error{nil};
		NSString* source = [NSString stringWithUTF8String:msl.c_str()];
		id<MTLLibrary> library = [device newLibraryWithSource:source options:compileOptions error:&error];

		Shader shader(ShaderView{library, [library newFunctionWithName:@"main0"]});
		Assert(shader.IsValid());
		return shader;
#endif
	}

	void
	ShaderCache::OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath)
	{
		UNUSED(assetGuid);
		UNUSED(identifier);
		/*const IO::File shaderFile =
		  System::Get<Asset::Manager>().OpenBinaryShared(assetGuid, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
		Assert(shaderFile.IsValid());
		if (UNLIKELY_ERROR(!shaderFile.IsValid()))
		{
		  return;
		}

		Engine& engine = System::Get<Engine>();
		const uint8 frameIndex = (engine.GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount;

		Shader& shader = GetAssetData(identifier);
		Shader previousShader;
		{
		  const IO::FileView::SizeType fileSize = shaderFile.GetSize();
		  FixedSizeVector<char, IO::FileView::SizeType>
		    shaderFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (size)fileSize + RENDERER_WEBGPU);
		  if (LIKELY(shaderFile.ReadIntoView(shaderFileContents.GetSubView(0, fileSize))))
		  {
#if RENDERER_WEBGPU
		    shaderFileContents[fileSize] = '\0';
#endif

		    Assert(false, "TODO: Support storing permutations, know the stage info");
		    Shader newShader; // = CreateShaderInternal(shaderFileContents.GetView(), stage, entryPointName, pipelineLayout, stageInfo);
		    previousShader = Move(shader);
		    shader = Move(newShader);
		  }
		  else
		  {
		    return;
		  }
		}

		struct DestroyJob final : public Threading::Job
		{
		  DestroyJob(const LogicalDevice& logicalDevice, Shader&& shader, const uint8 frameIndex)
		    : Threading::Job(Threading::JobPriority::DeallocateResourcesMin)
		    , m_logicalDevice(logicalDevice)
		    , m_shader(Forward<Shader>(shader))
		    , m_frameIndex(frameIndex)
		  {
		  }

		  virtual Result OnExecute([[maybe_unused]] Threading::JobRunnerThread& thread) override
		  {
		    if (System::Get<Engine>().GetCurrentFrameIndex() == m_frameIndex)
		    {
		      m_shader.Destroy(m_logicalDevice);

		      return Result::FinishedAndDelete;
		    }

		    return Result::TryRequeue;
		  }
		protected:
		  const LogicalDevice& m_logicalDevice;
		  Shader m_shader;
		  const uint8 m_frameIndex;
		};

		DestroyJob* pJob = new DestroyJob(GetLogicalDevice(), Move(previousShader), frameIndex);
		pJob->Queue(System::Get<Threading::JobManager>());

		Threading::SharedLock requestersLock(m_shaderRequesterMutex);
		auto it = m_shaderRequesterMap.Find(identifier);
		if (it != m_shaderRequesterMap.end())
		{
		  ShaderRequesters& requesters = *it->second;
		  requestersLock.Unlock();
		  requesters(shader);
		}*/
	}
}
