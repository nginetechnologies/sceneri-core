#include "AssetCompilerCore/AssetCompilers/Textures/GenericTextureCompiler.h"

#include <Common/Asset/Asset.h>
#include <Common/Asset/Context.h>

#include <Common/Platform/Type.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/IO/File.h>
#include <Common/IO/Library.h>
#include <Common/Math/Max.h>
#include <Common/Math/Min.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Format/Vector2.h>
#include <Common/Math/Format/Vector3.h>
#include <Common/Math/Vector3/Min.h>
#include <Common/Math/Vector4.h>
#include <Common/Math/PowerOfTwo.h>
#include <Common/Math/Floor.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Time/Timestamp.h>

#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Texture/GetBestPresetFormat.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Wrappers/Image.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Engine/Asset/AssetManager.h>

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(5219 4619 5266)

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wnan-infinity-disabled")

#include <EditorCommon/3rdparty/gli/gli.h>

POP_CLANG_WARNINGS
POP_MSVC_WARNINGS

#if SUPPORT_COMPRESSONATOR
PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4324 4310 4458 4100 4296 4701)

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wunused-parameter");
DISABLE_CLANG_WARNING("-Wunused-variable");

PUSH_GCC_WARNINGS
DISABLE_CLANG_WARNING("-Wunused-parameter");

#include <3rdparty/compressonator/include/Compressonator.h>

POP_CLANG_WARNINGS
POP_GCC_WARNINGS
POP_MSVC_WARNINGS
#endif

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4018 4389 4706 4242 4800)

#define TINYEXR_IMPLEMENTATION
#include <3rdparty/tinyexr/tinyexr.h>

POP_MSVC_WARNINGS

#if SUPPORT_ASTCENC
#include <3rdparty/astc-encoder/include/astcenc.h>
#endif

#include "IBLSampler/RenderHelper.h"
#include "IBLSampler/Sampling.h"

#include <Common/Memory/New.h>
#include <Common/Assert/Assert.h>
#include <Common/Math/Color.h>

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wreserved-id-macro");
DISABLE_CLANG_WARNING("-Wzero-as-null-pointer-constant");
DISABLE_CLANG_WARNING("-Wold-style-cast");
DISABLE_CLANG_WARNING("-Wcast-qual");
DISABLE_CLANG_WARNING("-Wsign-conversion");
DISABLE_CLANG_WARNING("-Wdisabled-macro-expansion");
DISABLE_CLANG_WARNING("-Wcast-align");
DISABLE_CLANG_WARNING("-Wdouble-promotion");
DISABLE_CLANG_WARNING("-Wconversion");
DISABLE_CLANG_WARNING("-Wimplicit-fallthrough");
DISABLE_CLANG_WARNING("-Wconditional-uninitialized");
DISABLE_CLANG_WARNING("-Wshadow");

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4244 4701 4703 4296 5219 4242)

#if !USE_SSE
#define STBI_NO_SIMD 1
#endif

#define STBI_ASSERT Assert
#define STBI_MALLOC ngine::Memory::Allocate

void* StbiReallocate(void* pPtr, const ngine::size newSize)
{
	if (pPtr == nullptr)
	{
		return STBI_MALLOC(newSize);
	}

	return ngine::Memory::Reallocate(pPtr, newSize);
}

#define STBI_REALLOC(pPtr, newSize) StbiReallocate(pPtr, newSize)
#define STBI_FREE ngine::Memory::Deallocate

#define STB_IMAGE_IMPLEMENTATION
#include "3rdparty/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "3rdparty/stb/stb_image_write.h"
#define STB_DXT_IMPLEMENTATION
#include "3rdparty/stb/stb_dxt.h"

POP_MSVC_WARNINGS
POP_CLANG_WARNINGS

#if SUPPORT_TIFF
#include "3rdparty/tiff/include/libtiff/tiffio.h"
#endif

#if SUPPORT_LIBKTX
#include "3rdparty/libktx/include/ktx.h"
#endif

#if SUPPORT_LUNASVG
#include "3rdparty/lunasvg/include/lunasvg.h"
#endif

namespace ngine::AssetCompiler::Compilers
{
	Rendering::Format GetUncompressedTextureFormat(const uint32 channelCount, const uint8 bitsPerChannel)
	{
		switch (bitsPerChannel)
		{
			case 8:
			{
				switch (channelCount)
				{
					case 1:
						return Rendering::Format::R8_UNORM;
					case 2:
						return Rendering::Format::R8G8_UNORM;
					case 3:
						return Rendering::Format::R8G8B8_UNORM;
					case 4:
						return Rendering::Format::R8G8B8A8_UNORM_PACK8;
					default:
						ExpectUnreachable();
				}
			}
			case 16:
			{
				switch (channelCount)
				{
					case 1:
						return Rendering::Format::R16_UNORM;
					case 2:
						return Rendering::Format::R16G16_UNORM;
					case 3:
						return Rendering::Format::R16G16B16_UNORM;
					case 4:
						return Rendering::Format::R16G16B16A16_UNORM;
					default:
						ExpectUnreachable();
				}
			}
			case 32:
			{
				switch (channelCount)
				{
					case 1:
						return Rendering::Format::R32_SFLOAT;
					case 2:
						return Rendering::Format::R32G32_SFLOAT;
					case 3:
						return Rendering::Format::R32G32B32_SFLOAT;
					case 4:
						return Rendering::Format::R32G32B32A32_SFLOAT;
					default:
						ExpectUnreachable();
				}
			}
		}

		ExpectUnreachable();
	}

	[[nodiscard]] Optional<Rendering::Format>
	GetRequiredSourceFormatForCompression(const Rendering::Format targetFormat, const Rendering::Format currentSourceFormat)
	{
		switch (targetFormat)
		{
			case Rendering::Format::ASTC_4X4_LDR:
			case Rendering::Format::ASTC_5X4_LDR:
			case Rendering::Format::ASTC_5X5_LDR:
			case Rendering::Format::ASTC_6X5_LDR:
			case Rendering::Format::ASTC_6X6_LDR:
			case Rendering::Format::ASTC_8X5_LDR:
			case Rendering::Format::ASTC_8X6_LDR:
			case Rendering::Format::ASTC_8X8_LDR:
			case Rendering::Format::ASTC_10X5_LDR:
			case Rendering::Format::ASTC_10X6_LDR:
			case Rendering::Format::ASTC_10X8_LDR:
			case Rendering::Format::ASTC_10X10_LDR:
			case Rendering::Format::ASTC_12X10_LDR:
			case Rendering::Format::ASTC_12X12_LDR:
			{
				switch (currentSourceFormat)
				{
					case Rendering::Format::R16G16B16A16_SFLOAT:
						return Rendering::Format::R16G16B16A16_SFLOAT;
					case Rendering::Format::R32G32B32A32_SFLOAT:
						return Rendering::Format::R32G32B32A32_SFLOAT;
					default:
						return Rendering::Format::R8G8B8A8_UNORM_PACK8;
				}
			}

			case Rendering::Format::ASTC_4X4_SRGB:
			case Rendering::Format::ASTC_5X4_SRGB:
			case Rendering::Format::ASTC_5X5_SRGB:
			case Rendering::Format::ASTC_6X5_SRGB:
			case Rendering::Format::ASTC_6X6_SRGB:
			case Rendering::Format::ASTC_8X5_SRGB:
			case Rendering::Format::ASTC_8X6_SRGB:
			case Rendering::Format::ASTC_8X8_SRGB:
			case Rendering::Format::ASTC_10X5_SRGB:
			case Rendering::Format::ASTC_10X6_SRGB:
			case Rendering::Format::ASTC_10X8_SRGB:
			case Rendering::Format::ASTC_10X10_SRGB:
			case Rendering::Format::ASTC_12X10_SRGB:
			case Rendering::Format::ASTC_12X12_SRGB:
				return Rendering::Format::R32G32B32A32_SFLOAT;
			case Rendering::Format::BC4_R_UNORM:
			case Rendering::Format::BC5_RG_UNORM:
			case Rendering::Format::BC1_RGB_UNORM:
			case Rendering::Format::BC1_RGBA_UNORM:
			case Rendering::Format::BC2_RGBA_UNORM:
			case Rendering::Format::BC3_RGBA_UNORM:
			case Rendering::Format::BC7_RGBA_UNORM:
				return Rendering::Format::R8G8B8A8_UNORM_PACK8;
			case Rendering::Format::BC6H_RGB_SFLOAT:
			case Rendering::Format::BC6H_RGB_UFLOAT:
				return Rendering::Format::R16G16B16A16_SFLOAT;
			default:
				return targetFormat;
		}
	}

	[[nodiscard]] Rendering::Format GetBestExportFormat(const Rendering::FormatInfo& formatInfo)
	{
		if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Float))
		{
			return Rendering::Format::R32_SFLOAT;
		}
		else if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Normalized))
		{
			if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Signed))
			{
				switch (formatInfo.m_componentCount)
				{
					case 1:
						return Rendering::Format::R8_SNORM;
					case 2:
						return Rendering::Format::R8G8_SNORM;
					case 3:
						return Rendering::Format::R8G8B8_SNORM;
					case 4:
						return Rendering::Format::R8G8B8A8_SNORM_PACK8;
				}
			}
			else
			{
				switch (formatInfo.m_componentCount)
				{
					case 1:
						return Rendering::Format::R8_UNORM;
					case 2:
						return Rendering::Format::R8G8_UNORM;
					case 3:
						return Rendering::Format::R8G8B8_UNORM;
					case 4:
						return Rendering::Format::R8G8B8A8_UNORM_PACK8;
				}
			}
		}
		else if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Signed))
		{
			switch (formatInfo.m_componentCount)
			{
				case 1:
					return Rendering::Format::R8_SINT;
				case 2:
					return Rendering::Format::R8G8_SINT;
				case 3:
					return Rendering::Format::R8G8B8_SINT;
				case 4:
					return Rendering::Format::R8G8B8A8_SINT_PACK8;
			}
		}
		else
		{
			switch (formatInfo.m_componentCount)
			{
				case 1:
					return Rendering::Format::R8_UINT;
				case 2:
					return Rendering::Format::R8G8_UINT;
				case 3:
					return Rendering::Format::R8G8B8_UINT;
				case 4:
					return Rendering::Format::R8G8B8A8_UINT_PACK8;
			}
		}
		ExpectUnreachable();
	}

	[[nodiscard]] constexpr uint8 GetUncompressedFormatBitsPerChannel(const Rendering::Format format)
	{
		return Rendering::GetFormatInfo(format).GetBlockDataSizePerChannel() * 8;
	}

	[[nodiscard]] Optional<gli::texture> ConvertTexture(gli::texture&& sourceTexture, const Rendering::Format targetFormat)
	{
		switch (sourceTexture.target())
		{
			case gli::target::TARGET_1D:
			{
				const gli::texture1d sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_1D_ARRAY:
			{
				const gli::texture1d_array sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_2D:
			{
				const gli::texture2d sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_2D_ARRAY:
			{
				const gli::texture2d_array sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_3D:
			{
				const gli::texture3d sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_CUBE:
			{
				const gli::texture_cube sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			case gli::target::TARGET_CUBE_ARRAY:
			{
				const gli::texture_cube_array sourceTextureWithTarget(sourceTexture);
				return gli::convert(sourceTextureWithTarget, static_cast<gli::format>(targetFormat));
			}
			default:
				return Invalid;
		}
	}

	/// Header for the on-disk format generated by astcenc.
	struct ASTCHeader
	{
		/// Magic value
		uint8_t magic[4];
		/// Block size in X
		uint8_t blockdimX;
		/// Block size in Y
		uint8_t blockdimY;
		/// Block size in Z
		uint8_t blockdimZ;
		/// Size of the image in pixels (X), least significant byte first.
		uint8_t xSize[3];
		/// Size of the image in pixels (Y), least significant byte first.
		uint8_t ySize[3];
		/// Size of the image in pixels (Z), least significant byte first.
		uint8_t zSize[3];
	};

#if SUPPORT_ASTCENC
	[[nodiscard]] constexpr astcenc_type GetAstcencSourceType(const Rendering::Format format)
	{
		if (Rendering::GetFormatInfo(format).m_flags.IsSet(Rendering::FormatFlags::Float))
		{
			switch (Rendering::GetFormatInfo(format).GetBlockDataSizePerChannel())
			{
				case 2:
					return ASTCENC_TYPE_F16;
				case 4:
					return ASTCENC_TYPE_F32;
				default:
					Assert(false);
					return ASTCENC_TYPE_F32;
			}
		}
		else
		{
			return ASTCENC_TYPE_U8;
		}
	}

	[[nodiscard]] constexpr astcenc_profile GetAstcencProfile([[maybe_unused]] const Rendering::Format format)
	{
		constexpr bool SupportAstcHDR = false;
		if constexpr (SupportAstcHDR)
		{
			switch (format)
			{
				case Rendering::Format::R16G16B16_SFLOAT:
				case Rendering::Format::R16G16B16A16_SFLOAT:
				case Rendering::Format::R32G32B32A32_SFLOAT:
					return ASTCENC_PRF_HDR_RGB_LDR_A;
				default:
					return ASTCENC_PRF_LDR;
			}
		}
		else
		{
			return ASTCENC_PRF_LDR;
		}
	}
#endif

	[[nodiscard]] Threading::JobBatch CompressMip(
		gli::image uncompressedTexture,
		const ByteView targetView,
		const Rendering::TextureAsset::BinaryInfo& __restrict binaryInfo,
		[[maybe_unused]] const Rendering::TexturePreset texturePreset,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		const Rendering::Format targetFormat = binaryInfo.GetFormat();
		const EnumFlags<Rendering::FormatFlags> targetFormatFlags = Rendering::GetFormatInfo(targetFormat).m_flags;
		if (targetFormatFlags.IsSet(Rendering::FormatFlags::ASTC))
		{
#if SUPPORT_ASTCENC
			astcenc_config astcConfig;
			const astcenc_profile colorProfile = GetAstcencProfile(static_cast<Rendering::Format>(uncompressedTexture.format()));

			uint32 astcFlags{0};
			switch (texturePreset)
			{
				case Rendering::TexturePreset::GreyscaleWithAlpha8:
				case Rendering::TexturePreset::DiffuseWithAlphaMask:
				case Rendering::TexturePreset::DiffuseWithAlphaTransparency:
				case Rendering::TexturePreset::Alpha:
					astcFlags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;
					break;
				case Rendering::TexturePreset::Normals:
					astcFlags |= ASTCENC_FLG_MAP_NORMAL;
					break;
				default:
					break;
			}

			const Math::TVector3<uint8> blockExtent = Rendering::GetFormatInfo(targetFormat).m_blockExtent;

			const Optional<Math::Ratiof> compressionQuality = binaryInfo.GetCompressionQuality();
			const float quality = compressionQuality.IsValid() ? compressionQuality.Get() * 100.f : ASTCENC_PRE_MEDIUM;

			const astcenc_error configError =
				astcenc_config_init(colorProfile, blockExtent.x, blockExtent.y, blockExtent.z, quality, astcFlags, &astcConfig);
			if (configError != ASTCENC_SUCCESS)
			{
				LogError("Failed to create ASTC config");
				failedAnyTasksOut = true;
				return {};
			}

			Threading::JobRunnerThread& currentThread = *Threading::JobRunnerThread::GetCurrent();
			Threading::JobManager& jobManager = currentThread.GetJobManager();
			const uint32 jobCount = jobManager.GetJobThreads().GetSize();

			astcenc_context* pAstcContext;

			const astcenc_error contextError = astcenc_context_alloc(&astcConfig, jobCount, &pAstcContext);
			if (contextError != ASTCENC_SUCCESS)
			{
				LogError("Failed to create ASTC context");
				failedAnyTasksOut = true;
				return {};
			}

			const Math::Vector3ui extent{
				(uint32)uncompressedTexture.extent().x,
				(uint32)uncompressedTexture.extent().y,
				(uint32)uncompressedTexture.extent().z
			};

			Threading::JobBatch jobBatch;
			for (uint32 jobIndex = 0; jobIndex < jobCount; ++jobIndex)
			{
				jobBatch.QueueAfterStartStage(Threading::CreateCallback(
					[pAstcContext,
				   extent,
				   sourceType = GetAstcencSourceType(static_cast<Rendering::Format>(uncompressedTexture.format())),
				   pSourceImageData = uncompressedTexture.data(),
				   targetView,
				   jobIndex,
				   &failedAnyTasksOut](Threading::JobRunnerThread&) mutable
					{
						// TODO: Create an array of pointers for each Z dimension for 3D textures
						Array<void*, 1> sourceTextureData{pSourceImageData};

						astcenc_image sourceImage{extent.x, extent.y, extent.z, sourceType, sourceTextureData.GetData()};

						const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};

						const astcenc_error error =
							astcenc_compress_image(pAstcContext, &sourceImage, &swizzle, targetView.GetData(), targetView.GetDataSize(), jobIndex);
						failedAnyTasksOut |= (error != ASTCENC_SUCCESS);
					},
					Threading::JobPriority::AssetCompilation
				));
			}

			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[pAstcContext](Threading::JobRunnerThread&)
				{
					astcenc_compress_reset(pAstcContext);
					astcenc_context_free(pAstcContext);
				},
				Threading::JobPriority::AssetCompilation
			));

			return jobBatch;
#else
			failedAnyTasksOut = true;
			return {};
#endif
		}
		else if (targetFormatFlags.AreAnySet(Rendering::FormatFlags::ASTC | Rendering::FormatFlags::BC))
		{
#if SUPPORT_COMPRESSONATOR
			auto convertFormat = [](const Rendering::Format format)
			{
				switch (format)
				{
					case Rendering::Format::R8_UNORM:
						return CMP_FORMAT_R_8;
					case Rendering::Format::R8G8_UNORM:
						return CMP_FORMAT_RG_8;
					case Rendering::Format::R8G8B8_UNORM:
						return CMP_FORMAT_RGB_888;
					case Rendering::Format::R8G8B8A8_UNORM_PACK8:
						return CMP_FORMAT_RGBA_8888;
					case Rendering::Format::R16G16B16A16_SFLOAT:
						return CMP_FORMAT_ARGB_16F;
					case Rendering::Format::BC1_RGB_UNORM:
						return CMP_FORMAT_BC1;
					case Rendering::Format::BC1_RGBA_UNORM:
						return CMP_FORMAT_BC1;
					case Rendering::Format::BC2_RGBA_UNORM:
						return CMP_FORMAT_BC2;
					case Rendering::Format::BC3_RGBA_UNORM:
						return CMP_FORMAT_BC3;
					case Rendering::Format::BC4_R_UNORM:
						return CMP_FORMAT_BC4;
					case Rendering::Format::BC5_RG_UNORM:
						return CMP_FORMAT_BC5;
					case Rendering::Format::BC6H_RGB_SFLOAT:
						return CMP_FORMAT_BC6H_SF;
					case Rendering::Format::BC6H_RGB_UFLOAT:
						return CMP_FORMAT_BC6H;
					case Rendering::Format::BC7_RGBA_UNORM:
						return CMP_FORMAT_BC7;
					default:
						return CMP_FORMAT_Unknown;
				}
			};

			float quality;
			const Optional<Math::Ratiof> compressionQuality = binaryInfo.GetCompressionQuality();
			if (targetFormatFlags.IsSet(Rendering::FormatFlags::ASTC))
			{
				quality = compressionQuality.IsValid() ? (float)compressionQuality.Get() : 0.05f;
			}
			else
			{
				switch (targetFormat)
				{
					case Rendering::Format::BC6H_RGB_SFLOAT:
					case Rendering::Format::BC6H_RGB_UFLOAT:
					case Rendering::Format::BC7_RGBA_UNORM:
						quality = compressionQuality.IsValid() ? (float)compressionQuality.Get() : 0.05f;
						break;
					default:
						quality = compressionQuality.IsValid() ? (float)compressionQuality.Get() : 1.f;
				}
			}

#define BC_COMPRESS_JOBS (PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_APPLE_MACOS)
#if BC_COMPRESS_JOBS
			Threading::JobRunnerThread& currentThread = *Threading::JobRunnerThread::GetCurrent();
			Threading::JobManager& jobManager = currentThread.GetJobManager();

			const Rendering::FormatInfo& sourceFormatInfo = Rendering::GetFormatInfo(static_cast<Rendering::Format>(uncompressedTexture.format())
			);
			const Rendering::FormatInfo& targetFormatInfo = Rendering::GetFormatInfo(targetFormat);

			CMP_EncoderSetting encodeSettings;
			encodeSettings.format = convertFormat(targetFormat);
			if (UNLIKELY_ERROR(encodeSettings.format == CMP_FORMAT_Unknown))
			{
				Assert(false);
				failedAnyTasksOut = true;
				return {};
			}

			encodeSettings.quality = quality;
			encodeSettings.width = uncompressedTexture.extent().x;
			encodeSettings.height = uncompressedTexture.extent().y;

			void* pEncoder{nullptr};
			if (CMP_CreateBlockEncoder(&pEncoder, encodeSettings) != 0)
			{
				Assert(false);
				failedAnyTasksOut = true;
				return {};
			}

			const uint32 blockWidth = encodeSettings.width / 4;
			const uint32 blockHeight = encodeSettings.height / 4;

			const uint32 srcStride = sourceFormatInfo.GetBytesPerRow(encodeSettings.width);
			const uint32 dstStride = targetFormatInfo.GetBytesPerRow(encodeSettings.width);

			const uint32 jobRunnerCount = jobManager.GetJobThreads().GetSize();
			const uint32 rowsPerRunner = blockHeight / jobRunnerCount;
			const uint32 remainingRows = blockHeight % jobRunnerCount;

			void* pDataIn = uncompressedTexture.data();

			Threading::JobBatch jobBatch;
			for (uint32 jobIndex = 0; jobIndex < jobRunnerCount; ++jobIndex)
			{
				const uint32 startY = jobIndex * rowsPerRunner;
				uint32 endY = (jobIndex + 1) * rowsPerRunner;
				if (jobIndex == jobRunnerCount - 1)
				{
					// Award the remaining rows to the last job
					endY += remainingRows;
				}

				jobBatch.QueueAfterStartStage(Threading::CreateCallback(
					[pEncoder, pDataIn, srcStride, targetView, dstStride, blockWidth, startY, endY, &failedAnyTasksOut](Threading::
				                                                                                                        JobRunnerThread&) mutable
					{
						for (uint32 y = startY; y < endY; y++)
						{
							for (uint32 x = 0; x < blockWidth; x++)
							{
								if (CMP_CompressBlockXY(&pEncoder, x, y, pDataIn, srcStride, targetView.GetData(), dstStride) != 0)
								{
									Assert(false);
									failedAnyTasksOut = true;
									return;
								}
							}
						}
					},
					Threading::JobPriority::AssetCompilation
				));
			}

			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[pEncoder](Threading::JobRunnerThread&) mutable
				{
					CMP_DestroyBlockEncoder(&pEncoder);
				},
				Threading::JobPriority::AssetCompilation
			));

			return jobBatch;
#else
			CMP_Texture textureSource;
			Memory::Set(&textureSource, 0, sizeof(textureSource));
			textureSource.dwSize = sizeof(textureSource);
			textureSource.dwWidth = uncompressedTexture.extent().x;
			textureSource.dwHeight = uncompressedTexture.extent().y;
			textureSource.format = convertFormat(static_cast<Rendering::Format>(uncompressedTexture.format()));
			textureSource.nBlockWidth = 1;
			textureSource.nBlockHeight = 1;
			textureSource.nBlockDepth = 1;
			textureSource.pData = reinterpret_cast<CMP_BYTE*>(uncompressedTexture.data());
			textureSource.dwDataSize = CMP_CalculateBufferSize(&textureSource);
			Assert(uncompressedTexture.size() == textureSource.dwDataSize);

			CMP_Texture textureTarget;
			Memory::Set(&textureTarget, 0, sizeof(textureTarget));
			textureTarget.dwSize = sizeof(textureTarget);
			textureTarget.dwWidth = uncompressedTexture.extent().x;
			textureTarget.dwPitch = textureTarget.dwWidth;
			textureTarget.dwHeight = uncompressedTexture.extent().y;
			textureTarget.format = convertFormat(static_cast<Rendering::Format>(targetFormat));
			textureTarget.nBlockWidth = textureSource.nBlockWidth;
			textureTarget.nBlockHeight = textureSource.nBlockHeight;
			textureTarget.nBlockDepth = 1;
			textureTarget.pData = targetView.GetData();
			textureTarget.dwDataSize = CMP_CalculateBufferSize(&textureTarget);
			Assert(targetView.GetDataSize() == textureTarget.dwDataSize);

			CMP_CompressOptions options = {0};
			options.fquality = quality;
			switch (targetFormat)
			{
				case Rendering::Format::BC1_RGBA_UNORM:
					options.bDXT1UseAlpha = true;
					options.nAlphaThreshold = 1;
					break;
				default:
					break;
			}

			return Threading::CreateCallback(
				[&failedAnyTasksOut,
			   textureSource = Move(textureSource),
			   textureTarget = Move(textureTarget),
			   options = Move(options)](Threading::JobRunnerThread&) mutable
				{
					options.dwSize = sizeof(options);
					options.dwnumThreads = 0; // Uses auto, else set number of threads from 1..127 max
					options.bUseCGCompress = true;
					options.nEncodeWith = CMP_Compute_type::CMP_GPU_VLK;

					options.bUseRefinementSteps = true;
					options.nRefinementSteps = 1;

					const CMP_ERROR error = CMP_ConvertTexture(&textureSource, &textureTarget, &options, nullptr);

					Assert(error == CMP_ERROR::CMP_OK);
					failedAnyTasksOut |= error != CMP_ERROR::CMP_OK;
				},
				Threading::JobPriority::AssetCompilation
			);
#endif

#else
			failedAnyTasksOut = true;
			return {};
#endif
		}
		else if (static_cast<Rendering::Format>(uncompressedTexture.format()) == targetFormat)
		{
			const ConstByteView sourceView{reinterpret_cast<const ByteType*>(uncompressedTexture.data()), (size)uncompressedTexture.size()};
			failedAnyTasksOut |= !targetView.CopyFrom(sourceView);
			return {};
		}
		else
		{
			failedAnyTasksOut = true;
			return {};
		}
	}

	[[nodiscard]] Rendering::TexturePreset GetBestDefaultPreset(
		const uint8 channelCount,
		const uint8 bitsPerChannel,
		const uint8 arrayElementCount,
		const GenericTextureCompiler::AlphaChannelUsageType alphaChannelUsageType
	)
	{
		switch (bitsPerChannel)
		{
			case 8:
			{
				Assert(arrayElementCount == 1);

				switch (channelCount)
				{
					case 1:
						return Rendering::TexturePreset::Greyscale8;
					case 2:
						return Rendering::TexturePreset::GreyscaleWithAlpha8;
					case 3:
						return Rendering::TexturePreset::Diffuse;
					case 4:
					{
						switch (alphaChannelUsageType)
						{
							case GenericTextureCompiler::AlphaChannelUsageType::None:
								return Rendering::TexturePreset::Diffuse;
							case GenericTextureCompiler::AlphaChannelUsageType::Mask:
								return Rendering::TexturePreset::DiffuseWithAlphaMask;
							case GenericTextureCompiler::AlphaChannelUsageType::Transparency:
								return Rendering::TexturePreset::DiffuseWithAlphaTransparency;
						}
					}
					default:
						return Rendering::TexturePreset::Unknown;
				}
			}
			case 16:
			{
				Assert(arrayElementCount == 1 || arrayElementCount == 6);
				if (arrayElementCount == 6)
				{
					Assert(channelCount == 3 || channelCount == 4);
					return Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR;
				}
				else
				{
					switch (channelCount)
					{
						case 1:
							return Rendering::TexturePreset::Greyscale8;
						case 2:
							return Rendering::TexturePreset::GreyscaleWithAlpha8;
						case 3:
							return Rendering::TexturePreset::Diffuse;
						case 4:
						{
							switch (alphaChannelUsageType)
							{
								case GenericTextureCompiler::AlphaChannelUsageType::None:
									return Rendering::TexturePreset::Diffuse;
								case GenericTextureCompiler::AlphaChannelUsageType::Mask:
									return Rendering::TexturePreset::DiffuseWithAlphaMask;
								case GenericTextureCompiler::AlphaChannelUsageType::Transparency:
									return Rendering::TexturePreset::DiffuseWithAlphaTransparency;
							}
						}
						default:
							return Rendering::TexturePreset::Unknown;
					}
				}
			}
			case 32:
			{
				Assert(arrayElementCount == 1 || arrayElementCount == 6);
				if (arrayElementCount == 6)
				{
					Assert(channelCount == 3 || channelCount == 4);
					return Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR;
				}
				else
				{
					switch (channelCount)
					{
						case 1:
							return Rendering::TexturePreset::Greyscale8;
						case 2:
							return Rendering::TexturePreset::GreyscaleWithAlpha8;
						case 3:
							return Rendering::TexturePreset::Diffuse;
						case 4:
						{
							switch (alphaChannelUsageType)
							{
								case GenericTextureCompiler::AlphaChannelUsageType::None:
									return Rendering::TexturePreset::Diffuse;
								case GenericTextureCompiler::AlphaChannelUsageType::Mask:
									return Rendering::TexturePreset::DiffuseWithAlphaMask;
								case GenericTextureCompiler::AlphaChannelUsageType::Transparency:
									return Rendering::TexturePreset::DiffuseWithAlphaTransparency;
							}
						}
						default:
							return Rendering::TexturePreset::Unknown;
					}
				}
			}
		}

		return Rendering::TexturePreset::Unknown;
	}

	enum class PixelFlags : uint8
	{
		IsMasked = 1 << 0,
		AreAllPixelsOpaque = 1 << 1,
		AreAllPixelsInvisible = 1 << 2,
	};
	ENUM_FLAG_OPERATORS(PixelFlags);

	template<typename PixelUnitType>
	[[nodiscard]] EnumFlags<PixelFlags> GetPixelFlags(EnumFlags<PixelFlags> existingFlags, const PixelUnitType alpha)
	{
		if constexpr (TypeTraits::IsIntegral<PixelUnitType>)
		{
			const bool requiresTransparency = alpha > 0 && alpha < Math::NumericLimits<PixelUnitType>::Max;
			const bool isOpaque = alpha == Math::NumericLimits<PixelUnitType>::Max;
			const bool isTransparent = alpha == 0;
			existingFlags &= ~(PixelFlags::IsMasked * requiresTransparency);
			existingFlags &= ~(PixelFlags::AreAllPixelsOpaque * !isOpaque);
			existingFlags &= ~(PixelFlags::AreAllPixelsInvisible * !isTransparent);
			return existingFlags;
		}
		else
		{
			const bool requiresTransparency = alpha > 0 && alpha < PixelUnitType(1);
			const bool isOpaque = alpha == PixelUnitType(1);
			const bool isTransparent = alpha == 0;
			existingFlags &= ~(PixelFlags::IsMasked * requiresTransparency);
			existingFlags &= ~(PixelFlags::AreAllPixelsOpaque * !isOpaque);
			existingFlags &= ~(PixelFlags::AreAllPixelsInvisible * !isTransparent);
			return existingFlags;
		}
	}

	[[nodiscard]] GenericTextureCompiler::AlphaChannelUsageType GetAlphaUsageType(const EnumFlags<PixelFlags> flags)
	{
		if (flags.IsSet(PixelFlags::AreAllPixelsOpaque))
		{
			return GenericTextureCompiler::AlphaChannelUsageType::None;
		}
		else if (flags.IsSet(PixelFlags::IsMasked))
		{
			return GenericTextureCompiler::AlphaChannelUsageType::Mask;
		}
		Assert(flags.IsNotSet(PixelFlags::IsMasked));
		Assert(flags.IsNotSet(PixelFlags::AreAllPixelsOpaque));
		Assert(flags.IsNotSet(PixelFlags::AreAllPixelsInvisible));
		return GenericTextureCompiler::AlphaChannelUsageType::Transparency;
	}

	Threading::JobBatch CompileUncompressedTextureBinaryType(
		gli::texture& uncompressedTexture,
		const Rendering::TextureAsset::BinaryType binaryType,
		Rendering::TextureAsset& textureAsset,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		IO::Path binaryFilePath = textureAsset.GetBinaryFilePath(binaryType);

		const Math::Vector2ui resolution{(uint32)uncompressedTexture.extent().x, (uint32)uncompressedTexture.extent().y};

		Rendering::TextureAsset::BinaryInfo& __restrict binaryInfo = textureAsset.GetBinaryAssetInfo(binaryType);
		binaryInfo.ClearMipmaps();

		if (textureAsset.GetPreset() != Rendering::TexturePreset::Explicit)
		{
			const Rendering::Format textureFormat = Rendering::GetBestPresetFormat(binaryType, textureAsset.GetPreset(), resolution);
			if (UNLIKELY_ERROR(textureFormat == Rendering::Format::Invalid))
			{
				LogError("Invalid texture format");
				failedAnyTasksOut = true;
				return {};
			}
			binaryInfo.SetFormat(textureFormat);
		}
		else if (UNLIKELY_ERROR(binaryInfo.GetFormat() == Rendering::Format::Invalid))
		{
			LogError("Invalid rendering format");
			failedAnyTasksOut = true;
			return {};
		}

		const Rendering::FormatInfo formatInfo = Rendering::GetFormatInfo(binaryInfo.GetFormat());
		const Math::TBoolVector2<uint32> isDivisibleByBlockExtent =
			Math::Mod(resolution, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}) == Math::Zero;
		if (UNLIKELY(!isDivisibleByBlockExtent.AreAllSet()))
		{
			LogError("Texture resolution {} was not divisible by format block extent {}", resolution, formatInfo.m_blockExtent);
			failedAnyTasksOut = true;
			return {};
		}

		if (textureAsset.ShouldGenerateMips())
		{
			const Rendering::MipMask blockExtentMipMask =
				Rendering::MipMask::FromSizeAllToLargest(Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y});
			const Rendering::MipMask::StoredType blockExtentMipCount = blockExtentMipMask.GetSize();
			Assert(blockExtentMipCount > 0);
			// Skip mips that are less than the format's block size
			const Rendering::MipMask::StoredType skippedMipCount = blockExtentMipCount - 1;
			const Rendering::MipMask mipMask = Rendering::MipMask::FromSizeAllToLargest(resolution);
			const Rendering::MipMask::StoredType mipCount = mipMask.GetSize();
			binaryInfo.SetMipmapCount(mipCount - skippedMipCount);
		}
		else
		{
			binaryInfo.SetMipmapCount(1);
		}

		{
			const Rendering::Format currentSourceFormat = static_cast<Rendering::Format>(uncompressedTexture.format());
			const Optional<Rendering::Format> requiredSourceFormat =
				GetRequiredSourceFormatForCompression(binaryInfo.GetFormat(), currentSourceFormat);
			if (requiredSourceFormat.IsValid() && requiredSourceFormat.Get() != currentSourceFormat)
			{
				if (Optional<gli::texture> convertedTexture = ConvertTexture(Forward<gli::texture>(uncompressedTexture), requiredSourceFormat.Get()))
				{
					uncompressedTexture = convertedTexture.Get();
				}
				else
				{
					if (binaryFilePath.Exists())
					{
						binaryFilePath.RemoveFile();
					}
					LogError("Failed to convert texture");
					failedAnyTasksOut = true;
					return {};
				}
			}
		}

		switch (uncompressedTexture.target())
		{
			case gli::target::TARGET_2D:
			{
				gli::texture2d uncompressedTexture2D;
				if (uncompressedTexture.levels() != binaryInfo.GetMipInfoView().GetSize())
				{
					gli::texture2d newTexture2D = gli::texture2d(
						uncompressedTexture.format(),
						gli::extent2d{uncompressedTexture.extent().x, uncompressedTexture.extent().y},
						binaryInfo.GetMipInfoView().GetSize(),
						uncompressedTexture.swizzles()
					);
					Memory::CopyNonOverlappingElements(
						static_cast<unsigned char*>(newTexture2D.data()),
						static_cast<const unsigned char*>(uncompressedTexture.data()),
						Math::Min(newTexture2D.size(), uncompressedTexture.size())
					);
					uncompressedTexture = Move(newTexture2D);

					gli::fsampler2D sampler(gli::texture2d(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
					sampler.generate_mipmaps(0, binaryInfo.GetMipInfoView().GetSize() - 1, gli::filter::FILTER_LINEAR);
					uncompressedTexture2D = gli::texture2d(sampler());
				}
				else
				{
					uncompressedTexture2D = gli::texture2d(uncompressedTexture);
				}

				const Rendering::Format targetFormat = binaryInfo.GetFormat();
				size totalDataSize{0};

				for (uint8 mipLevel = 0; mipLevel < binaryInfo.GetMipInfoView().GetSize(); ++mipLevel)
				{
					const Math::Vector2ui mipSize = resolution >> mipLevel;
					totalDataSize += GetFormatInfo(targetFormat).GetBytesPerLayer(mipSize);
				}

				FixedSizeVector<ByteType, size> mipData(Memory::ConstructWithSize, Memory::Uninitialized, totalDataSize);
				ArrayView<ByteType, size> remainingMipDataView = mipData.GetView();

				Threading::JobBatch compileTextureJobBatch;
				const Rendering::TexturePreset texturePreset = textureAsset.GetPreset();

				for (uint8 mipLevel = 0; mipLevel < binaryInfo.GetMipInfoView().GetSize(); ++mipLevel)
				{
					const Math::Vector2ui mipSize = resolution >> mipLevel;
					const size expectedMipDataSize = GetFormatInfo(targetFormat).GetBytesPerLayer(mipSize);
					const size mipFileOffset = mipData.GetView().GetIteratorIndex(remainingMipDataView.GetData());
					const ArrayView<ByteType, size> mipDataView = remainingMipDataView.GetSubView(0, expectedMipDataSize);
					remainingMipDataView += expectedMipDataSize;

					compileTextureJobBatch.QueueAfterStartStage(
						CompressMip(uncompressedTexture2D[mipLevel], mipDataView, binaryInfo, texturePreset, failedAnyTasksOut)
					);
					if (UNLIKELY_ERROR(failedAnyTasksOut))
					{
						if (binaryFilePath.Exists())
						{
							binaryFilePath.RemoveFile();
						}
						LogError("Failed to compress mip level {0}", mipLevel);
						failedAnyTasksOut = true;
						return {};
					}

					binaryInfo.StoreMipInfo(Rendering::TextureAsset::MipInfo{mipLevel, mipFileOffset, expectedMipDataSize}, mipLevel);
				}

				Assert(remainingMipDataView.IsEmpty());

				compileTextureJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[binaryFilePath = Move(binaryFilePath),
				   uncompressedTexture2D = Move(uncompressedTexture2D),
				   mipData = Move(mipData),
				   &failedAnyTasksOut](Threading::JobRunnerThread&)
					{
						IO::File targetFile(binaryFilePath, IO::AccessModeFlags::WriteBinary);
						if (UNLIKELY_ERROR(!targetFile.IsValid()))
						{
							LogError("Target file is not valid {0}", binaryFilePath);
							failedAnyTasksOut = true;
							return;
						}

						if (UNLIKELY_ERROR(failedAnyTasksOut))
						{
							targetFile.Close();
							if (binaryFilePath.Exists())
							{
								binaryFilePath.RemoveFile();
							}
							LogError("Failed to compress texture");
							failedAnyTasksOut = true;
							return;
						}

						const bool writeFullData = targetFile.Write(mipData.GetView()) == mipData.GetDataSize();
						if (UNLIKELY_ERROR(!writeFullData))
						{
							LogError("Written bytes don't match texture size {1}", mipData.GetDataSize());
							failedAnyTasksOut = true;
							return;
						}

						// Flush immediately as file may be needed by other runners very soon
						targetFile.Flush();
					},
					Threading::JobPriority::AssetCompilation
				));
				return compileTextureJobBatch;
			}
			case gli::TARGET_CUBE:
			{
				gli::texture_cube uncompressedTextureCube;
				if (uncompressedTexture.levels() != binaryInfo.GetMipInfoView().GetSize())
				{
					gli::texture_cube existingCube(uncompressedTexture);
					gli::texture_cube newTextureCube = gli::texture_cube(
						uncompressedTexture.format(),
						gli::extent2d{uncompressedTexture.extent().x, uncompressedTexture.extent().y},
						binaryInfo.GetMipInfoView().GetSize(),
						uncompressedTexture.swizzles()
					);
					for (uint8 faceIndex = 0; faceIndex < 6; ++faceIndex)
					{
						gli::texture2d targetFace = newTextureCube[faceIndex];
						gli::texture2d sourceFace = existingCube[faceIndex];

						Memory::CopyNonOverlappingElements(
							static_cast<unsigned char*>(targetFace.data()),
							static_cast<const unsigned char*>(sourceFace.data()),
							Math::Min(sourceFace.size(), targetFace.size())
						);
					}
					uncompressedTexture = Move(newTextureCube);

					gli::fsamplerCube sampler(gli::texture_cube(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
					sampler.generate_mipmaps(0, 5, 0, binaryInfo.GetMipInfoView().GetSize() - 1, gli::filter::FILTER_LINEAR);

					uncompressedTextureCube = gli::texture_cube(sampler());
				}
				else
				{
					uncompressedTextureCube = gli::texture_cube(uncompressedTexture);
				}

				const Rendering::Format targetFormat = binaryInfo.GetFormat();
				size totalDataSize{0};

				for (uint8 mipLevel = 0; mipLevel < binaryInfo.GetMipInfoView().GetSize(); ++mipLevel)
				{
					const Math::Vector2ui mipSize = resolution >> mipLevel;
					totalDataSize += GetFormatInfo(targetFormat).GetBytesPerLayer(mipSize) * 6;
				}

				FixedSizeVector<ByteType, size> mipData(Memory::ConstructWithSize, Memory::Uninitialized, totalDataSize);
				ArrayView<ByteType, size> remainingMipDataView = mipData.GetView();

				Threading::JobBatch compileTextureJobBatch;
				const Rendering::TexturePreset texturePreset = textureAsset.GetPreset();

				for (uint8 mipLevel = 0, numMips = (uint8)uncompressedTexture.levels(); mipLevel < numMips; ++mipLevel)
				{
					const Math::Vector2ui mipSize = resolution >> mipLevel;
					const size expectedMipDataSize = GetFormatInfo(targetFormat).GetBytesPerLayer(mipSize);
					const size mipFileOffset = mipData.GetView().GetIteratorIndex(remainingMipDataView.GetData());
					const ArrayView<ByteType, size> mipDataView = remainingMipDataView.GetSubView(0, expectedMipDataSize * 6);
					remainingMipDataView += expectedMipDataSize * 6;

					for (uint8 faceIndex = 0; faceIndex < 6; ++faceIndex)
					{
						const ArrayView<ByteType, size> faceDataView = mipDataView.GetSubView(expectedMipDataSize * faceIndex, expectedMipDataSize);

						gli::texture2d faceTexture = uncompressedTextureCube[faceIndex];
						compileTextureJobBatch.QueueAfterStartStage(
							CompressMip(faceTexture[mipLevel], faceDataView, binaryInfo, texturePreset, failedAnyTasksOut)
						);
						if (UNLIKELY_ERROR(failedAnyTasksOut))
						{
							if (binaryFilePath.Exists())
							{
								binaryFilePath.RemoveFile();
							}
							LogError("Failed to compress mip level {0}", mipLevel);
							failedAnyTasksOut = true;
							return {};
						}
					}

					binaryInfo.StoreMipInfo(Rendering::TextureAsset::MipInfo{mipLevel, mipFileOffset, expectedMipDataSize * 6}, mipLevel);
				}

				Assert(remainingMipDataView.IsEmpty());

				compileTextureJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[binaryFilePath = Move(binaryFilePath),
				   mipData = Move(mipData),
				   uncompressedTextureCube = Move(uncompressedTextureCube),
				   &failedAnyTasksOut](Threading::JobRunnerThread&)
					{
						IO::File targetFile(binaryFilePath, IO::AccessModeFlags::WriteBinary);
						if (UNLIKELY_ERROR(!targetFile.IsValid()))
						{
							LogError("Target file is not valid {0}", binaryFilePath);
							failedAnyTasksOut = true;
							return;
						}

						if (UNLIKELY_ERROR(failedAnyTasksOut))
						{
							targetFile.Close();
							if (binaryFilePath.Exists())
							{
								binaryFilePath.RemoveFile();
							}
							LogError("Failed to compress texture");
							return;
						}

						const bool writeFullData = targetFile.Write(mipData.GetView()) == mipData.GetDataSize();
						if (UNLIKELY_ERROR(!writeFullData))
						{
							LogError("Written bytes don't match texture size {1}", mipData.GetDataSize());
							failedAnyTasksOut = true;
							return;
						}

						// Flush immediately as file may be needed by other runners very soon
						targetFile.Flush();
					},
					Threading::JobPriority::AssetCompilation
				));
				return compileTextureJobBatch;
			}
			default:
			{
				if (binaryFilePath.Exists())
				{
					binaryFilePath.RemoveFile();
				}
				LogError("Unknown texture target: {0}", (uint32)uncompressedTexture.target());
				failedAnyTasksOut = true;
				return {};
			}
		}
		ExpectUnreachable();
	}

	[[nodiscard]] Threading::JobBatch CompileUncompressedTexture(
		gli::texture& uncompressedTexture,
		const uint8 channelCount,
		const uint8 bitsPerChannel,
		const uint8 arrayElementCount,
		Serialization::Data&& assetData,
		Rendering::TextureAsset&& textureAsset,
		EnumFlags<Platform::Type> platforms,
		CompileCallback&& callback
	)
	{
		Threading::JobBatch compileTextureJobBatch;
		struct TextureInfo
		{
			Threading::Atomic<bool> failedAnyTasks{false};
		};
		UniquePtr<TextureInfo> pTextureInfo{Memory::ConstructInPlace};
		Threading::Atomic<bool>& failedAnyTasks = pTextureInfo->failedAnyTasks;

		EnumFlags<Rendering::TextureAsset::BinaryType> binaryTypes;
		for (const Platform::Type platform : platforms)
		{
			binaryTypes |= Rendering::GetSupportedTextureBinaryTypes(platform);
		}

		Assert(Rendering::GetFormatInfo(static_cast<Rendering::Format>(uncompressedTexture.format()))
		         .m_flags.IsNotSet(Rendering::FormatFlags::Compressed));

		textureAsset.SetTypeGuid(TextureAssetType::AssetFormat.assetTypeGuid);

		textureAsset.SetArraySize(arrayElementCount);
		textureAsset.SetResolution({(uint32)uncompressedTexture.extent().x, (uint32)uncompressedTexture.extent().y});

		if (textureAsset.GetPreset() == Rendering::TexturePreset::Unknown)
		{
			const GenericTextureCompiler::AlphaChannelUsageType alphaChannelUsageType = [](const gli::texture& uncompressedTexture)
			{
				const gli::texture::swizzles_type swizzles = uncompressedTexture.swizzles();
				if (swizzles.r != gli::SWIZZLE_ALPHA && swizzles.g != gli::SWIZZLE_ALPHA && swizzles.b != gli::SWIZZLE_ALPHA && swizzles.a != gli::SWIZZLE_ALPHA)
				{
					return GenericTextureCompiler::AlphaChannelUsageType::None;
				}

				EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

				switch (uncompressedTexture.target())
				{
					case gli::target::TARGET_1D:
					case gli::target::TARGET_1D_ARRAY:
					case gli::target::TARGET_RECT:
					case gli::target::TARGET_RECT_ARRAY:
						ExpectUnreachable();

					case gli::target::TARGET_2D:
					{
						gli::fsampler2D sampler(gli::texture2d(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
						for (gli::texture::size_type levelIndex = 0; levelIndex < uncompressedTexture.levels(); ++levelIndex)
						{
							const gli::texture::extent_type dimensions = uncompressedTexture.extent(levelIndex);

							for (int j = 0; j < dimensions.y; ++j)
							{
								for (int i = 0; i < dimensions.x; ++i)
								{
									typename gli::texture2d::extent_type const texelCoordinate(gli::texture2d::extent_type(i, j));
									pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
								}
							}
						}
					}
					break;
					case gli::target::TARGET_2D_ARRAY:
					{
						gli::fsampler2DArray sampler(gli::texture2d_array(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
						for (gli::texture::size_type layerIndex = 0; layerIndex < uncompressedTexture.layers(); ++layerIndex)
						{
							for (gli::texture::size_type levelIndex = 0; levelIndex < uncompressedTexture.levels(); ++levelIndex)
							{
								const gli::texture::extent_type dimensions = uncompressedTexture.extent(levelIndex);

								for (int j = 0; j < dimensions.y; ++j)
								{
									for (int i = 0; i < dimensions.x; ++i)
									{
										typename gli::texture2d_array::extent_type const texelCoordinate(gli::texture2d_array::extent_type(i, j));
										pixelFlags =
											GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, layerIndex, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
									}
								}
							}
						}
					}
					break;
					case gli::target::TARGET_3D:
					{
						gli::fsampler3D sampler(gli::texture3d(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
						for (gli::texture::size_type levelIndex = 0; levelIndex < uncompressedTexture.levels(); ++levelIndex)
						{
							const gli::texture::extent_type dimensions = uncompressedTexture.extent(levelIndex);

							for (int k = 0; k < dimensions.z; ++k)
							{
								for (int j = 0; j < dimensions.y; ++j)
								{
									for (int i = 0; i < dimensions.x; ++i)
									{
										typename gli::texture3d::extent_type const texelCoordinate(gli::texture3d::extent_type(i, j, k));
										pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
									}
								}
							}
						}
					}
					break;
					case gli::target::TARGET_CUBE_ARRAY:
					{
						gli::fsamplerCubeArray sampler(gli::texture_cube_array(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
						for (gli::texture::size_type layerIndex = 0; layerIndex < uncompressedTexture.layers(); ++layerIndex)
						{
							for (gli::texture::size_type faceIndex = 0; faceIndex < uncompressedTexture.faces(); ++faceIndex)
							{
								for (gli::texture::size_type levelIndex = 0; levelIndex < uncompressedTexture.levels(); ++levelIndex)
								{
									const gli::texture::extent_type dimensions = uncompressedTexture.extent(levelIndex);

									for (int j = 0; j < dimensions.y; ++j)
									{
										for (int i = 0; i < dimensions.x; ++i)
										{
											typename gli::texture_cube_array::extent_type const texelCoordinate(gli::texture_cube_array::extent_type(i, j));
											pixelFlags = GetPixelFlags(
												pixelFlags,
												sampler.texel_fetch(texelCoordinate, faceIndex, layerIndex, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]
											);
										}
									}
								}
							}
						}
					}
					break;
					case gli::target::TARGET_CUBE:
					{
						gli::fsamplerCube sampler(gli::texture_cube(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
						for (gli::texture::size_type faceIndex = 0; faceIndex < uncompressedTexture.faces(); ++faceIndex)
						{
							for (gli::texture::size_type levelIndex = 0; levelIndex < uncompressedTexture.levels(); ++levelIndex)
							{
								const gli::texture::extent_type dimensions = uncompressedTexture.extent(levelIndex);

								for (int j = 0; j < dimensions.y; ++j)
								{
									for (int i = 0; i < dimensions.x; ++i)
									{
										typename gli::texture_cube::extent_type const texelCoordinate(gli::texture_cube::extent_type(i, j));
										pixelFlags =
											GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, faceIndex, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
									}
								}
							}
						}
					}
					break;
				}
				return GetAlphaUsageType(pixelFlags);
			}(uncompressedTexture);

			textureAsset.SetPreset(GetBestDefaultPreset(channelCount, bitsPerChannel, arrayElementCount, alphaChannelUsageType));
			if (UNLIKELY_ERROR(textureAsset.GetPreset() == Rendering::TexturePreset::Unknown))
			{
				LogError("Unknown texture preset");
				failedAnyTasks = true;
				return {};
			}
		}

		for (const Rendering::TextureAsset::BinaryType binaryType : binaryTypes)
		{
			Threading::JobBatch compileBinaryJobBatch =
				CompileUncompressedTextureBinaryType(uncompressedTexture, binaryType, textureAsset, failedAnyTasks);
			compileTextureJobBatch.QueueAfterStartStage(compileBinaryJobBatch);
		}

		compileTextureJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[assetData = Move(assetData),
		   textureAsset = Move(textureAsset),
		   callback = Move(callback),
		   pTextureInfo = Move(pTextureInfo)](Threading::JobRunnerThread&) mutable
			{
				Serialization::Serialize(assetData, textureAsset);
				callback(
					CompileFlags::Compiled * !pTextureInfo->failedAnyTasks,
					ArrayView<Asset::Asset>{textureAsset},
					ArrayView<const Serialization::Data>{assetData}
				);
			},
			Threading::JobPriority::AssetCompilation
		));
		return compileTextureJobBatch;
	}

	uint8 GetRequiredPrecompressionChannelCount(const uint8 numChannels)
	{
		switch (numChannels)
		{
			case 1:
				return 1;
			case 2:
				return 2;
			case 3:
			// Always load RGB data with alpha
			// We need it during the compression step
			case 4:
				return 4;
			default:
				ExpectUnreachable();
		}
	}

	bool GenericTextureCompiler::IsUpToDate(
		const Platform::Type platform,
		const Serialization::Data&,
		const Asset::Asset& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context
	)
	{
		const Time::Timestamp sourceFileTimestamp = sourceFilePath.GetLastModifiedTime();

		const EnumFlags<Rendering::TextureAsset::BinaryType> supportedBinaryTypes = Rendering::GetSupportedTextureBinaryTypes(platform);
		for (const Rendering::TextureAsset::BinaryType supportedBinaryType : supportedBinaryTypes)
		{
			IO::Path binaryFilePath = asset.GetBinaryFilePath(Rendering::TextureAsset::GetBinaryFileExtension(supportedBinaryType));
			const Time::Timestamp targetFileTimestamp = binaryFilePath.GetLastModifiedTime();

			// Disabled as it causes iOS textures to recompile every time
			/*const Time::Timestamp assetMetaDataTimestamp = asset.GetMetaDataFilePath().GetLastModifiedTime();
			if (assetMetaDataTimestamp > targetFileTimestamp)
			{
			  return false;
			}*/

			if (targetFileTimestamp < sourceFileTimestamp || !sourceFileTimestamp.IsValid())
			{
				return false;
			}

			IO::File binaryFile(binaryFilePath, IO::AccessModeFlags::ReadBinary);
			if (!binaryFile.IsValid())
			{
				return false;
			}

			if (binaryFile.GetSize() == 0)
			{
				return false;
			}
		}

		return true;
	}

	template<typename Callback>
	struct ExecuteCallbackAndAwaitJob final : public Threading::Job
	{
		ExecuteCallbackAndAwaitJob(Callback&& callback, const Priority priority)
			: Job(priority)
			, m_callback(Forward<Callback>(callback))
		{
		}
		virtual ~ExecuteCallbackAndAwaitJob() = default;

		virtual Result OnExecute(Threading::JobRunnerThread& thread) override
		{
			m_queuedJobBatch = m_callback(thread);
			return Result::AwaitExternalFinish;
		}

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override
		{
			if (m_queuedJobBatch.IsValid())
			{
				m_queuedJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[this](Threading::JobRunnerThread& thread)
					{
						SignalExecutionFinishedAndDestroying(thread);
						delete this;
					},
					GetPriority()
				));
				thread.Queue(m_queuedJobBatch);
			}
			else
			{
				SignalExecutionFinishedAndDestroying(thread);
				delete this;
			}
		}
	protected:
		Callback m_callback;
		Threading::JobBatch m_queuedJobBatch;
	};

	Threading::Job* GenericTextureCompiler::CompileStbi(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return new ExecuteCallbackAndAwaitJob(
			[sourceFilePath,
		   flags,
		   callback = Move(callback),
		   platforms,
		   assetData = Move(assetData),
		   asset = Move(asset)](Threading::JobRunnerThread&) mutable
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
				if (!sourceFile.IsValid())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				int width, height, textureChannelCountOnDisk;
				if (!stbi_info_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk))
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				Expect(width > 0);
				Expect(height > 0);
				Expect(textureChannelCountOnDisk > 0);

				const uint8 bufferChannelCount = GetRequiredPrecompressionChannelCount((uint8)textureChannelCountOnDisk);

				const bool is16bit = (bool)stbi_is_16_bit_from_file(static_cast<FILE*>(sourceFile.GetFile()));
				const Math::Vector2ui size = {(uint32)width, (uint32)height};
				gli::texture2d uncompressedTexture(
					static_cast<gli::format>(GetUncompressedTextureFormat(bufferChannelCount, 8 + is16bit * 8)),
					gli::extent2d{size.x, size.y},
					1
				);

				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));

				if (is16bit)
				{
					const size_t bufferSize = width * height * bufferChannelCount * sizeof(uint16);

					uint16* pImageData = stbi_load_from_file_16(
						static_cast<FILE*>(sourceFile.GetFile()),
						&width,
						&height,
						&textureChannelCountOnDisk,
						bufferChannelCount
					);
					Memory::CopyNonOverlappingElements(
						static_cast<unsigned char*>(uncompressedTexture.data()),
						reinterpret_cast<const unsigned char*>(pImageData),
						bufferSize
					);
					ngine::Memory::Deallocate(pImageData);

					return CompileUncompressedTexture(
						uncompressedTexture,
						(uint8)textureChannelCountOnDisk,
						sizeof(uint16) * 8,
						1,
						Move(assetData),
						Move(textureAsset),
						platforms,
						Move(callback)
					);
				}
				else
				{
					const size_t bufferSize = width * height * bufferChannelCount;

					uint8* const pImageData =
						stbi_load_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk, bufferChannelCount);
					Memory::CopyNonOverlappingElements(
						static_cast<unsigned char*>(uncompressedTexture.data()),
						reinterpret_cast<const unsigned char*>(pImageData),
						bufferSize
					);
					ngine::Memory::Deallocate(pImageData);

					return CompileUncompressedTexture(
						uncompressedTexture,
						(uint8)textureChannelCountOnDisk,
						sizeof(uint8) * 8,
						1,
						Move(assetData),
						Move(textureAsset),
						platforms,
						Move(callback)
					);
				}
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	[[nodiscard]] gli::texture2d DecompressImage(
		const gli::texture2d& sourceImage,
		const Rendering::TextureAsset::BinaryType sourceBinaryType,
		[[maybe_unused]] const Rendering::Format sourceFormat,
		[[maybe_unused]] const Rendering::FormatInfo& sourceFormatInfo,
		const Rendering::Format targetFormat,
		[[maybe_unused]] const Optional<Math::Ratiof> decompressionQuality
	)
	{
		switch (sourceBinaryType)
		{
			case Rendering::TextureAsset::BinaryType::BC:
			case Rendering::TextureAsset::BinaryType::Uncompressed:
			{
				return gli::convert(sourceImage, static_cast<gli::format>(targetFormat));
			}
			case Rendering::TextureAsset::BinaryType::ASTC:
			{
#if SUPPORT_ASTCENC
				astcenc_config astcConfig;
				const astcenc_profile colorProfile = GetAstcencProfile(sourceFormat);

				const uint32 astcFlags{ASTCENC_FLG_DECOMPRESS_ONLY};

				const float quality = decompressionQuality.IsValid() ? decompressionQuality.Get() * 100.f : ASTCENC_PRE_MEDIUM;

				const astcenc_error configError = astcenc_config_init(
					colorProfile,
					sourceFormatInfo.m_blockExtent.x,
					sourceFormatInfo.m_blockExtent.y,
					sourceFormatInfo.m_blockExtent.z,
					quality,
					astcFlags,
					&astcConfig
				);
				if (configError != ASTCENC_SUCCESS)
				{
					return {};
				}

				const uint32 jobCount = 1; // jobManager.GetJobThreads().GetSize();

				astcenc_context* pAstcContext;

				const astcenc_error contextError = astcenc_context_alloc(&astcConfig, jobCount, &pAstcContext);
				if (contextError != ASTCENC_SUCCESS)
				{
					return {};
				}

				const Math::Vector2i resolution{sourceImage.extent().x, sourceImage.extent().y};
				gli::texture2d exportedImage(static_cast<gli::texture::format_type>(targetFormat), sourceImage.extent(), 1);

				Array<void*, 1> targetTextureData{exportedImage.data()};
				astcenc_image
					targetImage{(uint32)resolution.x, (uint32)resolution.y, 1, GetAstcencSourceType(targetFormat), targetTextureData.GetData()};

				const astcenc_swizzle swizzle{ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};
				const astcenc_error decompressError = astcenc_decompress_image(
					pAstcContext,
					reinterpret_cast<const uint8*>(sourceImage.data()),
					sourceImage.size(),
					&targetImage,
					&swizzle,
					0
				);
				if (decompressError != ASTCENC_SUCCESS)
				{
					return {};
				}
				return Move(exportedImage);
#else
				return {};
#endif
			}
			case Rendering::TextureAsset::BinaryType::Count:
			case Rendering::TextureAsset::BinaryType::End:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	template<typename Callback>
	Optional<Threading::Job*> LoadTextureBinary(const Guid assetGuid, IO::Path&& binaryFilePath, Callback&& callback)
	{
		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		if (assetManager.HasAsset(assetGuid))
		{
			return assetManager.RequestAsyncLoadAssetPath(
				assetGuid,
				binaryFilePath,
				Threading::JobPriority::AssetCompilation,
				[callback = Forward<Callback>(callback)](const ConstByteView data)
				{
					callback(data);
				}
			);
		}
		else
		{
			FixedSizeVector<char, IO::FileView::SizeType> sourceFileContents;
			{
				IO::File binaryFile(binaryFilePath, IO::AccessModeFlags::ReadBinary);
				Assert(binaryFile.IsValid());
				if (UNLIKELY(!binaryFile.IsValid()))
				{
					callback({});
					return nullptr;
				}

				sourceFileContents =
					FixedSizeVector<char, IO::FileView::SizeType>(Memory::ConstructWithSize, Memory::Uninitialized, binaryFile.GetSize());
				if (UNLIKELY(!binaryFile.ReadIntoView(sourceFileContents.GetView())))
				{
					callback({});
					return nullptr;
				}
			}

			callback(sourceFileContents.GetView());
			return nullptr;
		}
	}

	Vector<ByteType> GenericTextureCompiler::ExportPNG(
		const Rendering::Format sourceFormat,
		const Rendering::TextureAsset::BinaryType binaryType,
		const Math::Vector2ui resolution,
		const Rendering::Format targetFormat,
		const Optional<Math::Ratiof> decompressionQuality,
		const ConstByteView data
	)
	{
		gli::texture2d
			existingImage(static_cast<gli::texture::format_type>(sourceFormat), gli::texture2d::extent_type{resolution.x, resolution.y}, 1);
		Assert((size)data.GetDataSize() >= existingImage.size());
		if (UNLIKELY((size)data.GetDataSize() < existingImage.size()))
		{
			return {};
		}

		if (UNLIKELY(!(ByteView{reinterpret_cast<ByteType*>(existingImage.data()), existingImage.size()}
		                 .CopyFrom(data.GetSubViewUpTo(existingImage.size())))))
		{
			return {};
		}

		const Rendering::FormatInfo& sourceFormatInfo = Rendering::GetFormatInfo(sourceFormat);
		const Rendering::FormatInfo& targetFormatInfo = Rendering::GetFormatInfo(targetFormat);
		gli::texture2d decompressedImage =
			DecompressImage(existingImage, binaryType, sourceFormat, sourceFormatInfo, targetFormat, decompressionQuality);

		const uint32 bitsPerPixel = targetFormatInfo.GetBitsPerPixel();
		const uint32 bytesPerPixel = bitsPerPixel / 8;

		Vector<ByteType> targetData;
		// Write the image data
		int result = stbi_write_png_to_func(
			[](void* context, void* pData, const int size)
			{
				Vector<ByteType>& data = *reinterpret_cast<Vector<ByteType>*>(context);

				data.CopyEmplaceRange(
					data.end(),
					Memory::Uninitialized,
					ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(pData), (uint32)size}
				);
			},
			&targetData,
			resolution.x,
			resolution.y,
			targetFormatInfo.m_componentCount,
			decompressedImage.data(),
			resolution.x * bytesPerPixel
		);
		if (result == 1)
		{
			return Move(targetData);
		}
		else
		{
			return {};
		}
	}

	Threading::Job* GenericTextureCompiler::
		ExportPNG(ExportedCallback&& callback, const EnumFlags<Platform::Type> platforms, Serialization::Data&& assetData, Asset::Asset&& asset, [[maybe_unused]] const IO::PathView targetFormatExtension, const Asset::Context&)
	{
		EnumFlags<Rendering::TextureAsset::BinaryType> binaryTypes;
		for (const Platform::Type platform : platforms)
		{
			binaryTypes |= Rendering::GetSupportedTextureBinaryTypes(platform);
		}

		Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
		const Math::Vector2ui resolution = textureAsset.GetResolution();

		Threading::Job& intermediateStage = Threading::CreateCallback(
			[](Threading::JobRunnerThread&)
			{
			},
			Threading::JobPriority::AssetCompilation
		);

		for (const Rendering::TextureAsset::BinaryType binaryType : binaryTypes)
		{
			IO::Path binaryFilePath = textureAsset.GetBinaryFilePath(binaryType);
			const Rendering::TextureAsset::BinaryInfo& binaryInfo = textureAsset.GetBinaryAssetInfo(binaryType);

			static constexpr Rendering::Format targetFormat = Rendering::Format::R8G8B8A8_UNORM_PACK8;

			IO::Path exportedFileExtension = IO::Path::Merge(Rendering::TextureAsset::GetBinaryFileExtension(binaryType), targetFormatExtension);

			Threading::Job* pJob = LoadTextureBinary(
				asset.GetGuid(),
				Move(binaryFilePath),
				[callback,
			   resolution,
			   arraySize = textureAsset.GetArraySize(),
			   binaryType,
			   compressionQuality = binaryInfo.GetCompressionQuality(),
			   sourceFormat = binaryInfo.GetFormat(),
			   exportedFileExtension = Move(exportedFileExtension)](const ConstByteView data)
				{
					switch (arraySize)
					{
						case 1:
						{
							Vector<ByteType> targetData = ExportPNG(sourceFormat, binaryType, resolution, targetFormat, compressionQuality, data);
							callback(targetData.GetView(), exportedFileExtension);
						}
						break;
						case 6:
							Assert(false, "TODO: Support cubemaps");
							break;
						default:
							Assert(false, "TODO: Support");
							break;
					}
				}
			);
			if (pJob != nullptr)
			{
				intermediateStage.AddSubsequentStage(*pJob);
			}
		}

		return &intermediateStage;
	}

#if SUPPORT_TIFF
	struct TIFFClient
	{
		Vector<ByteType, uint64> m_data;
		uint64 m_position = 0;

		static tsize_t Read(thandle_t handle, tdata_t _buffer, tsize_t _size)
		{
			TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			const ConstByteView dataView = tiffClient.m_data.GetView();
			if (tiffClient.m_position + _size > dataView.GetDataSize())
			{
				return 0;
			}

			const bool wasRead = dataView.GetSubView(tiffClient.m_position, (uint64)_size)
			                       .ReadIntoView(ArrayView<ByteType, uint64>{reinterpret_cast<ByteType*>(_buffer), (uint64)_size});
			tiffClient.m_position += _size;
			return wasRead ? _size : 0;
		}

		static tsize_t Write(thandle_t handle, tdata_t _buffer, tsize_t _size)
		{
			TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			if (tiffClient.m_position + _size > tiffClient.m_data.GetDataSize())
			{
				tiffClient.m_data.Resize(tiffClient.m_position + _size, Memory::Zeroed);
			}

			const ByteView dataView = tiffClient.m_data.GetView();
			const ConstByteView sourceView{reinterpret_cast<const ByteType*>(_buffer), _size};
			const ByteView targetView = dataView.GetSubView(tiffClient.m_position, (uint64)_size);
			const bool wasWritten = sourceView.ReadIntoView(ArrayView<ByteType, uint64>{targetView.GetData(), targetView.GetDataSize()});
			tiffClient.m_position += _size;
			return wasWritten ? _size : 0;
		}

		static toff_t Seek(thandle_t handle, toff_t _offset, int _origin)
		{
			TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			const ConstByteView dataView = tiffClient.m_data.GetView();
			switch (_origin)
			{
				case SEEK_CUR:
					Assert(tiffClient.m_position + _offset <= dataView.GetDataSize());
					tiffClient.m_position += _offset;
					break;
				case SEEK_END:
					Assert(dataView.GetDataSize() + _offset <= dataView.GetDataSize());
					tiffClient.m_position = dataView.GetDataSize() + _offset;
					break;
				case SEEK_SET:
					tiffClient.m_position = _offset;
					break;
				default:
					ExpectUnreachable();
			}
			return tiffClient.m_position;
		}

		static int Close(thandle_t handle)
		{
			[[maybe_unused]] TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			return 0;
		}

		static toff_t Size(thandle_t handle)
		{
			TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			const ConstByteView dataView = tiffClient.m_data.GetView();
			return dataView.GetDataSize();
		}

		static int Map(thandle_t handle, tdata_t* base, toff_t* psize)
		{
			TIFFClient& tiffClient = *reinterpret_cast<TIFFClient*>(handle);
			const ByteView dataView = tiffClient.m_data.GetView();
			*base = dataView.GetData();
			*psize = dataView.GetDataSize();
			return (1);
		}

		static void Unmap(thandle_t handle, tdata_t base, toff_t size)
		{
			UNUSED(handle);
			UNUSED(base);
			UNUSED(size);
		}
	};
#endif

	Threading::Job* GenericTextureCompiler::CompileTIFF(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
#if SUPPORT_TIFF
		return new ExecuteCallbackAndAwaitJob(
			[flags, callback, sourceFilePath, assetData = Move(assetData), asset = Move(asset), platforms](Threading::JobRunnerThread&) mutable
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
				if (!sourceFile.IsValid())
				{
					return Threading::JobBatch{};
				}

				TIFFClient tiffClient{Vector<ByteType, uint64>(Memory::ConstructWithSize, Memory::Uninitialized, (uint64)sourceFile.GetSize())};
				if (!sourceFile.ReadIntoView(tiffClient.m_data.GetView()))
				{
					return Threading::JobBatch{};
				}

				TIFF* const pTifFile = TIFFClientOpen(
					"MEMTIFF",
					"rb",
					(thandle_t)&tiffClient,
					TIFFClient::Read,
					TIFFClient::Write,
					TIFFClient::Seek,
					TIFFClient::Close,
					TIFFClient::Size,
					TIFFClient::Map,
					TIFFClient::Unmap
				);

				if (pTifFile == nullptr)
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				TIFFSetErrorHandler(
					[](const char* module, const char* message, va_list)
					{
						UNUSED(module);
						UNUSED(message);
					}
				);

				uint32 textureChannelCountOnDisk = 0;
				TIFFGetField(pTifFile, TIFFTAG_SAMPLESPERPIXEL, &textureChannelCountOnDisk);
				uint32 numBitsPerChannel = 0;
				TIFFGetField(pTifFile, TIFFTAG_BITSPERSAMPLE, &numBitsPerChannel);

				gli::texture2d uncompressedTexture;
				{
					Math::Vector2ui imageSize;
					TIFFGetField(pTifFile, TIFFTAG_IMAGEWIDTH, &imageSize.x);
					TIFFGetField(pTifFile, TIFFTAG_IMAGELENGTH, &imageSize.y);

					{
						uint32 photometric = 0, planarConfig = 0;
						TIFFGetField(pTifFile, TIFFTAG_PHOTOMETRIC, &photometric);
						TIFFGetField(pTifFile, TIFFTAG_PLANARCONFIG, &planarConfig);

						Assert(planarConfig != PLANARCONFIG_SEPARATE);
						Assert(photometric != PHOTOMETRIC_SEPARATED);
					}

					const uint8 bufferChannelCount = GetRequiredPrecompressionChannelCount((uint8)textureChannelCountOnDisk);
					uncompressedTexture = gli::texture2d(
						static_cast<gli::format>(GetUncompressedTextureFormat(bufferChannelCount, (uint8)numBitsPerChannel)),
						gli::extent2d{imageSize.x, imageSize.y},
						1
					);

					ArrayView<uint8, uint64> destination = {
						reinterpret_cast<uint8*>(uncompressedTexture.data(0, 0, 0)),
						(uint64)uncompressedTexture.size(0)
					};
					const uint64 scanlineSize = TIFFScanlineSize64(pTifFile);

					if (bufferChannelCount == textureChannelCountOnDisk)
					{
						Assert(scanlineSize * imageSize.y == (size)destination.GetDataSize());

						for (uint32 y = 0; y < imageSize.y; ++y)
						{
							[[maybe_unused]] const int result = TIFFReadScanline(pTifFile, destination.GetData(), y);
							Assert(result == 1);

							destination += scanlineSize;
						}
					}
					else
					{
						Assert(textureChannelCountOnDisk == 3 && bufferChannelCount == 4);
						for (uint32 y = 0; y < imageSize.y; ++y)
						{
							const ArrayView<uint8, uint64> scanlineView = destination.GetSubView(0, scanlineSize);
							const ArrayView<uint8, uint64> extendedScanlineView = destination.GetSubView(0, scanlineSize + imageSize.x);
							[[maybe_unused]] const int result = TIFFReadScanline(pTifFile, scanlineView.GetData(), y);
							Assert(result == 1);

							ArrayView<const Math::TVector3<uint8>, uint64> rgbSource = {
								reinterpret_cast<const Math::TVector3<uint8>*>(scanlineView.GetData()),
								scanlineSize / 3
							};

							Math::TVector4<uint8>* rgbaDestination = reinterpret_cast<Math::TVector4<uint8>*>(extendedScanlineView.end().Get()) - 1;
							for (const Math::TVector3<uint8>*element = rgbSource.end() - 1, *start = rgbSource.begin() - 1; element != start;
						       element--, rgbaDestination--)
							{
								*rgbaDestination = Math::TVector4<uint8>(element->x, element->y, element->z, (uint8)1);
							}

							destination += extendedScanlineView.GetSize();
						}
					}

					Assert(destination.IsEmpty());
				}

				TIFFClose(pTifFile);

				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
				return CompileUncompressedTexture(
					uncompressedTexture,
					(uint8)textureChannelCountOnDisk,
					(uint8)numBitsPerChannel,
					1,
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
#else
		callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
		return nullptr;
#endif
	}

	Threading::Job* GenericTextureCompiler::CompileDDS(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return new ExecuteCallbackAndAwaitJob(
			[flags, callback, sourceFilePath, assetData = Move(assetData), asset = Move(asset), platforms](Threading::JobRunnerThread&) mutable
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
				if (!sourceFile.IsValid())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
				if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				gli::texture texture = gli::load_dds(sourceFileContents.GetData(), sourceFileContents.GetSize());
				if (texture.empty())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				const Rendering::Format format = static_cast<Rendering::Format>(texture.format());

				// TODO: Decompress and process normally
				Assert(Rendering::GetFormatInfo(format).m_flags.IsNotSet(Rendering::FormatFlags::Compressed));
				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));

				return CompileUncompressedTexture(
					texture,
					(uint8)Rendering::GetFormatInfo(format).m_componentCount,
					GetUncompressedFormatBitsPerChannel(format),
					(uint8)texture.faces(),
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	Threading::Job* GenericTextureCompiler::
		ExportTIFF(ExportedCallback&& callback, [[maybe_unused]] const EnumFlags<Platform::Type> platforms, [[maybe_unused]] Serialization::Data&& assetData, [[maybe_unused]] Asset::Asset&& asset, [[maybe_unused]] const IO::PathView targetFormatExtension, const Asset::Context&)
	{
#if SUPPORT_TIFF
		EnumFlags<Rendering::TextureAsset::BinaryType> binaryTypes;
		for (const Platform::Type platform : platforms)
		{
			binaryTypes |= Rendering::GetSupportedTextureBinaryTypes(platform);
		}

		Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
		const Math::Vector2ui resolution = textureAsset.GetResolution();

		Threading::Job& intermediateStage = Threading::CreateCallback(
			[](Threading::JobRunnerThread&)
			{
			},
			Threading::JobPriority::AssetCompilation
		);

		for (const Rendering::TextureAsset::BinaryType binaryType : binaryTypes)
		{
			IO::Path binaryFilePath = textureAsset.GetBinaryFilePath(binaryType);
			const Rendering::TextureAsset::BinaryInfo& binaryInfo = textureAsset.GetBinaryAssetInfo(binaryType);
			const Rendering::FormatInfo& formatInfo = Rendering::GetFormatInfo(binaryInfo.GetFormat());

			const Rendering::Format targetFormat = GetBestExportFormat(formatInfo);
			const Rendering::FormatInfo& targetFormatInfo = Rendering::GetFormatInfo(targetFormat);

			IO::Path exportedFileExtension = IO::Path::Merge(Rendering::TextureAsset::GetBinaryFileExtension(binaryType), MAKE_PATH(".tiff"));

			Threading::Job* pJob = LoadTextureBinary(
				asset.GetGuid(),
				Move(binaryFilePath),
				[callback,
			   resolution,
			   arraySize = textureAsset.GetArraySize(),
			   binaryType,
			   compressionQuality = binaryInfo.GetCompressionQuality(),
			   sourceFormat = binaryInfo.GetFormat(),
			   exportedFileExtension = Move(exportedFileExtension),
			   &formatInfo,
			   targetFormat,
			   &targetFormatInfo](const ConstByteView data)
				{
					switch (arraySize)
					{
						case 1:
						{
							gli::texture2d existingImage(
								static_cast<gli::texture::format_type>(sourceFormat),
								gli::texture2d::extent_type{resolution.x, resolution.y},
								1
							);
							Assert((size)data.GetDataSize() >= existingImage.size());
							if (UNLIKELY((size)data.GetDataSize() < existingImage.size()))
							{
								callback({}, exportedFileExtension);
								return;
							}

							if (UNLIKELY(!(ByteView{reinterpret_cast<ByteType*>(existingImage.data()), existingImage.size()}
						                   .CopyFrom(data.GetSubViewUpTo(existingImage.size())))))
							{
								callback({}, exportedFileExtension);
								return;
							}

							TIFFClient tiffClient;
							TIFF* const pTifFile = TIFFClientOpen(
								"MEMTIFF",
								"wb",
								(thandle_t)&tiffClient,
								TIFFClient::Read,
								TIFFClient::Write,
								TIFFClient::Seek,
								TIFFClient::Close,
								TIFFClient::Size,
								TIFFClient::Map,
								TIFFClient::Unmap
							);
							if (pTifFile != nullptr)
							{
								TIFFSetField(pTifFile, TIFFTAG_IMAGEWIDTH, resolution.x);
								TIFFSetField(pTifFile, TIFFTAG_IMAGELENGTH, resolution.y);
								TIFFSetField(pTifFile, TIFFTAG_SAMPLESPERPIXEL, targetFormatInfo.m_componentCount);
								const uint32 bitsPerPixel = targetFormatInfo.GetBitsPerPixel();
								const uint32 bitsPerSample = bitsPerPixel / targetFormatInfo.m_componentCount;
								TIFFSetField(pTifFile, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
								TIFFSetField(pTifFile, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
								TIFFSetField(pTifFile, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
								TIFFSetField(pTifFile, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

								gli::texture2d decompressedImage =
									DecompressImage(existingImage, binaryType, sourceFormat, formatInfo, targetFormat, compressionQuality);

								const uint32 bytesPerPixel = bitsPerPixel / 8;

								// Write the image data
								ConstByteView imageData{reinterpret_cast<const ByteType*>(decompressedImage.data()), decompressedImage.size()};
								Assert(imageData.GetDataSize() == resolution.y * resolution.x * bytesPerPixel);
								for (uint32 row = 0; row < resolution.y; ++row)
								{
									Assert(imageData.GetDataSize() >= resolution.x * bytesPerPixel);
									if (TIFFWriteScanline(pTifFile, (void*)imageData.GetData(), row, 0) < 0)
									{
										TIFFClose(pTifFile);
										callback({}, exportedFileExtension);
										return;
									}
									imageData += resolution.x * bytesPerPixel;
								}
								Assert(imageData.IsEmpty());

								TIFFClose(pTifFile);
								callback(tiffClient.m_data.GetView(), exportedFileExtension);
							}
							else
							{
								callback({}, exportedFileExtension);
							}
							break;
						}
						case 6:
							Assert(false, "TODO: Support cubemaps");
							callback({}, exportedFileExtension);
							break;
						default:
							Assert(false, "TODO: Support");
							callback({}, exportedFileExtension);
							break;
					}
				}
			);
			if (pJob != nullptr)
			{
				intermediateStage.AddSubsequentStage(*pJob);
			}
		}

		return &intermediateStage;
#else
		callback({}, targetFormatExtension);
		return nullptr;
#endif
	}

	Threading::Job* GenericTextureCompiler::CompileKTX(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return new ExecuteCallbackAndAwaitJob(
			[flags, callback, sourceFilePath, assetData = Move(assetData), asset = Move(asset), platforms](Threading::JobRunnerThread&) mutable
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
				if (!sourceFile.IsValid())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
				if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				gli::texture texture = gli::load_ktx(sourceFileContents.GetData(), sourceFileContents.GetSize());

				// TODO: Decompress and process normally
				const Rendering::Format format = static_cast<Rendering::Format>(texture.format());
				Assert(Rendering::GetFormatInfo(format).m_flags.IsNotSet(Rendering::FormatFlags::Compressed));
				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));

				return CompileUncompressedTexture(
					texture,
					(uint8)Rendering::GetFormatInfo(format).m_componentCount,
					GetUncompressedFormatBitsPerChannel(format),
					(uint8)texture.faces(),
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	Threading::Job* GenericTextureCompiler::CompileKTX2(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
#if SUPPORT_LIBKTX
		return new ExecuteCallbackAndAwaitJob(
			[flags, callback, sourceFilePath, assetData = Move(assetData), asset = Move(asset), platforms](Threading::JobRunnerThread&) mutable
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
				if (!sourceFile.IsValid())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
				if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				ktxTexture2* textureIn;
				const KTX_error_code result = ktxTexture2_CreateFromMemory(
					reinterpret_cast<const ktx_uint8_t*>(sourceFileContents.GetData()),
					sourceFileContents.GetDataSize(),
					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
					&textureIn
				);
				if (result != KTX_SUCCESS)
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					ktxTexture_Destroy(ktxTexture(textureIn));
					return Threading::JobBatch{};
				}

				gli::texture texture;
				if (textureIn->isArray)
				{
					if (textureIn->isCubemap)
					{
						Assert(textureIn->baseDepth == 1);
						texture = gli::texture_cube_array(
							static_cast<gli::texture::format_type>(textureIn->vkFormat),
							gli::texture_cube::extent_type{textureIn->baseWidth, textureIn->baseHeight},
							textureIn->numLayers,
							textureIn->numLevels
						);
					}
					else
					{
						switch (textureIn->numDimensions)
						{
							case 1:
								texture = gli::texture1d_array(
									static_cast<gli::texture::format_type>(textureIn->vkFormat),
									gli::texture1d::extent_type{(int)textureIn->baseWidth},
									textureIn->numLayers,
									textureIn->numLevels
								);
								break;
							case 2:
								texture = gli::texture2d_array(
									static_cast<gli::texture::format_type>(textureIn->vkFormat),
									gli::texture2d::extent_type{textureIn->baseWidth, textureIn->baseHeight},
									textureIn->numLayers,
									textureIn->numLevels
								);
								break;
							case 3:
								Assert(false);
								callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
								ktxTexture_Destroy(ktxTexture(textureIn));
								return Threading::JobBatch{};
						}
					}
				}
				else if (textureIn->isCubemap)
				{
					Assert(textureIn->baseDepth == 1);
					texture = gli::texture_cube(
						static_cast<gli::texture::format_type>(textureIn->vkFormat),
						gli::texture_cube::extent_type{textureIn->baseWidth, textureIn->baseHeight},
						textureIn->numLevels
					);
				}
				else
				{
					switch (textureIn->numDimensions)
					{
						case 1:
							texture = gli::texture1d(
								static_cast<gli::texture::format_type>(textureIn->vkFormat),
								gli::texture1d::extent_type{(int)textureIn->baseWidth},
								textureIn->numLevels
							);
							break;
						case 2:
							texture = gli::texture2d(
								static_cast<gli::texture::format_type>(textureIn->vkFormat),
								gli::texture2d::extent_type{textureIn->baseWidth, textureIn->baseHeight},
								textureIn->numLevels
							);
							break;
						case 3:
							texture = gli::texture3d(
								static_cast<gli::texture::format_type>(textureIn->vkFormat),
								gli::texture3d::extent_type{textureIn->baseWidth, textureIn->baseHeight, textureIn->baseDepth},
								textureIn->numLevels
							);
							break;
					}
				}

				static auto copyImage = [](
																	[[maybe_unused]] const gli::texture& targetTexture,
																	ktxTexture2* refTexture,
																	gli::image targetImage,
																	ktxTexture2* texture,
																	const uint32 levelIndex,
																	const uint32 layerIndex = 0,
																	const uint32 faceIndex = 0
																)
				{
					size ktxOffset;
					// TODO: This breaks for the cubemaps.
					[[maybe_unused]] const KTX_error_code result =
						ktxTexture_GetImageOffset(ktxTexture(refTexture), levelIndex, layerIndex, faceIndex, &ktxOffset);
					Assert(result == KTX_SUCCESS);
					[[maybe_unused]] size imageSize = ktxTexture_GetImageSize(ktxTexture(refTexture), levelIndex);
					Assert(imageSize == targetImage.size());
					// const ptrdiff offset = static_cast<ptrdiff>(reinterpret_cast<uint8*>(targetImage.data()) - reinterpret_cast<const
				  // uint8*>(targetTexture.data()));
					Memory::CopyWithoutOverlap(
						targetImage.data(),
						texture->pData + ktxOffset,
						targetImage.size()
					); // Math::Min(imageSize, targetImage.size()));
				};

				ktxTextureCreateInfo createInfo;
				createInfo.baseWidth = textureIn->baseWidth;
				createInfo.baseHeight = textureIn->baseHeight;
				createInfo.baseDepth = textureIn->baseDepth;
				createInfo.generateMipmaps = false;
				createInfo.isArray = textureIn->isArray;
				createInfo.numDimensions = textureIn->numDimensions;
				createInfo.numFaces = textureIn->numFaces;
				createInfo.numLayers = textureIn->numLayers;
				createInfo.numLevels = textureIn->numLevels;
				createInfo.vkFormat = textureIn->vkFormat;
				ktxTexture2* refTexture;
				[[maybe_unused]] auto test = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_NO_STORAGE, &refTexture);
				Assert(test == 0);

				switch (texture.target())
				{
					case gli::target::TARGET_1D:
					{
						gli::texture1d textureImplementation(texture);
						copyImage(texture, refTexture, textureImplementation[0], textureIn, 0);
					}
					break;
					case gli::target::TARGET_1D_ARRAY:
					{
						gli::texture1d_array textureImplementation(texture);
						for (uint32 arrayLayerIndex = 0; arrayLayerIndex < textureIn->numLayers; ++arrayLayerIndex)
						{
							copyImage(texture, refTexture, textureImplementation[arrayLayerIndex][0], textureIn, 0, arrayLayerIndex);
						}
					}
					break;
					case gli::target::TARGET_2D:
					{
						gli::texture2d textureImplementation(texture);
						copyImage(texture, refTexture, textureImplementation[0], textureIn, 0);
					}
					break;
					case gli::target::TARGET_2D_ARRAY:
					{
						gli::texture2d_array textureImplementation(texture);
						for (uint32 arrayLayerIndex = 0; arrayLayerIndex < textureIn->numLayers; ++arrayLayerIndex)
						{
							copyImage(texture, refTexture, textureImplementation[arrayLayerIndex][0], textureIn, 0, arrayLayerIndex);
						}
					}
					break;
					case gli::target::TARGET_3D:
					{
						gli::texture3d textureImplementation(texture);
						copyImage(texture, refTexture, textureImplementation[0], textureIn, 0);
					}
					break;
					case gli::target::TARGET_CUBE:
					{
						gli::texture_cube textureImplementation(texture);
						for (uint32 faceIndex = 0; faceIndex < textureIn->numFaces; ++faceIndex)
						{
							copyImage(texture, refTexture, textureImplementation[faceIndex][0], textureIn, 0, 0, faceIndex);
						}
					}
					break;
					case gli::target::TARGET_CUBE_ARRAY:
					{
						gli::texture_cube_array textureImplementation(texture);
						for (uint32 arrayLayerIndex = 0; arrayLayerIndex < textureIn->numLayers; ++arrayLayerIndex)
						{
							for (uint32 faceIndex = 0; faceIndex < textureIn->numFaces; ++faceIndex)
							{
								copyImage(
									texture,
									refTexture,
									textureImplementation[arrayLayerIndex][faceIndex][0],
									textureIn,
									0,
									arrayLayerIndex,
									faceIndex
								);
							}
						}
					}
					break;
					default:
						Assert(false);
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						ktxTexture_Destroy(ktxTexture(textureIn));
						return Threading::JobBatch{};
				}

				ktxTexture_Destroy(ktxTexture(textureIn));

				// TODO: Decompress and process normally
				const Rendering::Format format = static_cast<Rendering::Format>(texture.format());
				Assert(Rendering::GetFormatInfo(format).m_flags.IsNotSet(Rendering::FormatFlags::Compressed));

				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));

				return CompileUncompressedTexture(
					texture,
					(uint8)Rendering::GetFormatInfo(format).m_componentCount,
					GetUncompressedFormatBitsPerChannel(format),
					(uint8)texture.faces(),
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
#else
		callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
		return nullptr;
#endif
	}

	Threading::Job* GenericTextureCompiler::CompileSVG(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
#if SUPPORT_LUNASVG
		return new ExecuteCallbackAndAwaitJob(
			[flags,
		   callback,
		   sourceFilePath,
		   assetData = Move(assetData),
		   asset = Move(asset),
		   metaDataFilePath = IO::Path(asset.GetMetaDataFilePath()),
		   platforms](Threading::JobRunnerThread&) mutable
			{
				std::unique_ptr<lunasvg::Document> svgDocument;
				{
					const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
					if (UNLIKELY(!sourceFile.IsValid()))
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return Threading::JobBatch{};
					}
					FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
					if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return Threading::JobBatch{};
					}

					svgDocument = lunasvg::Document::loadFromData(sourceFileContents.GetData(), sourceFileContents.GetDataSize());
				}

				const Math::Vector2ui sourceSize = {(uint32)svgDocument->width(), (uint32)svgDocument->height()};
				const uint32 maximumDimension = Math::Max(sourceSize.x, sourceSize.y);
				const uint8 maximumDimensionIndex = maximumDimension != sourceSize.x;
				const uint32 ratio = maximumDimension / sourceSize[!maximumDimensionIndex];

				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));

				Math::Vector2ui outputSize = textureAsset.GetResolution();
				if (textureAsset.GetResolution().IsZero())
				{
					// Default to 128x128 if not explicitly specified
					const uint32 highestMipSize = 128u;
					outputSize[maximumDimensionIndex] = highestMipSize;
					outputSize[!maximumDimensionIndex] = Math::NearestPowerOfTwo(highestMipSize / ratio);
				}

				const Rendering::MipMask mipMask = Rendering::MipMask::FromSizeAllToLargest(outputSize);
				const Rendering::MipMask::StoredType numMips = mipMask.GetSize();

				gli::texture2d uncompressedTexture =
					gli::texture2d(gli::format::FORMAT_RGBA8_UNORM_PACK8, gli::extent2d{outputSize.x, outputSize.y}, numMips);

				auto renderMipIntoTexture = [&document = *svgDocument, flags, &asset, &assetData, &callback](gli::image targetImage)
				{
					const Math::Vector2f scale = Math::Vector2f{(float)targetImage.extent().x, (float)targetImage.extent().y} /
				                               Math::Vector2f{(float)document.width(), (float)document.height()};
					lunasvg::Matrix matrix;
					matrix.identity();
					matrix.scale(scale.x, scale.y);
					document.setMatrix(matrix);

					lunasvg::Bitmap targetBitmap(
						static_cast<uint8*>(targetImage.data()),
						targetImage.extent().x,
						targetImage.extent().y,
						targetImage.extent().x * 4
					);
					if (!targetBitmap.valid())
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return;
					}
					document.render(targetBitmap);
					targetBitmap.convert(0, 1, 2, 3, true); // convert To RGBA unpremulitied
				};

				for (uint8 mipLevel = 0; mipLevel < numMips; ++mipLevel)
				{
					renderMipIntoTexture(uncompressedTexture[mipLevel]);
				}

				const Rendering::Format format = static_cast<Rendering::Format>(uncompressedTexture.format());
				Assert(Rendering::GetFormatInfo(format).m_flags.IsNotSet(Rendering::FormatFlags::Compressed));

				return CompileUncompressedTexture(
					uncompressedTexture,
					(uint8)Rendering::GetFormatInfo(format).m_componentCount,
					GetUncompressedFormatBitsPerChannel(format),
					(uint8)uncompressedTexture.faces(),
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
#else
		callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
		return nullptr;
#endif
	}

	void CompileHDRCubemap(
		const ConstByteView imageData,
		const Math::Vector2ui size,
		const EnumFlags<CompileFlags> flags,
		CompileCallback& callback,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data& cubemapAssetData,
		Asset::Asset& asset
	)
	{
		const uint8 bufferChannelCount = 4;
		const uint32 sampleCount = 1024;

		const uint32 cubemapSize = Math::Min(size.x, size.y);

		// TODO: Make sure this matches with what the PBR lighting stage uses
		const uint32 irradianceRenderTargetSize = 32u;
		const uint32 prefilteredRenderTargetSize = 256u;

		const Rendering::MipMask cubemapMipMask = Rendering::MipMask::FromSizeAllToLargest({cubemapSize, cubemapSize});
		const Rendering::MipMask::StoredType cubemapMipCount = cubemapMipMask.GetSize();

		const Rendering::MipMask irradianceMipMask =
			Rendering::MipMask::FromSizeAllToLargest({irradianceRenderTargetSize, irradianceRenderTargetSize});
		const Rendering::MipMask::StoredType irradianceMipCount = irradianceMipMask.GetSize();

		const Rendering::MipMask prefilteredMipMask =
			Rendering::MipMask::FromSizeAllToLargest({prefilteredRenderTargetSize, prefilteredRenderTargetSize});
		const Rendering::MipMask::StoredType prefilteredMipCount = prefilteredMipMask.GetSize();

		gli::texture_cube uncompressedCubemapTexture(
			static_cast<gli::format>(GetUncompressedTextureFormat(bufferChannelCount, 32)),
			gli::extent2d{cubemapSize, cubemapSize},
			cubemapMipCount
		);
		gli::texture_cube uncompressedSpecularCubemapTexture(
			static_cast<gli::format>(GetUncompressedTextureFormat(bufferChannelCount, 32)),
			gli::extent2d{prefilteredRenderTargetSize, prefilteredRenderTargetSize},
			prefilteredMipCount
		);
		gli::texture_cube uncompressedDiffuseCubemapTexture(
			static_cast<gli::format>(GetUncompressedTextureFormat(bufferChannelCount, 32)),
			gli::extent2d{irradianceRenderTargetSize, irradianceRenderTargetSize},
			irradianceMipCount
		);

		UniquePtr<RenderHelper> pRenderHelper = UniquePtr<RenderHelper>::Make(PROFILE_BUILD != 0);
		if (!pRenderHelper->IsValid())
		{
			LogError("Render helper is not valid");
			callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{cubemapAssetData});
			return;
		}

		RenderHelper& renderHelper = *pRenderHelper;

		Rendering::Image panoramaImage = renderHelper.uploadImage(imageData, size.x, size.y);
		if (!panoramaImage.IsValid())
		{
			LogError("Failed to upload image");
			callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{cubemapAssetData});
			return;
		}

		Rendering::RenderTexture cubemap =
			SampleCubemap(renderHelper, panoramaImage, {cubemapSize, cubemapSize}, cubemapMipMask, Rendering::Format::R32G32B32A32_SFLOAT);
		Assert(cubemap.IsValid());
		if (LIKELY(cubemap.IsValid()))
		{
			const bool specularSamplingResult = Sample(
				renderHelper,
				cubemap,
				uncompressedSpecularCubemapTexture,
				Distribution::GGX,
				prefilteredMipMask,
				sampleCount,
				Rendering::Format::R32G32B32A32_SFLOAT,
				1.f
			);

			const bool diffuseSamplingResult = Sample(
				renderHelper,
				cubemap,
				uncompressedDiffuseCubemapTexture,
				Distribution::Lambertian,
				irradianceMipMask,
				sampleCount,
				Rendering::Format::R32G32B32A32_SFLOAT,
				1.f
			);

			if (!specularSamplingResult || !diffuseSamplingResult)
			{
				LogError("Sample failed");
				callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{cubemapAssetData});
				return;
			}
		}
		else
		{
			LogError("SampleCubemap failed");
			callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{cubemapAssetData});
			return;
		}

		if (!DownloadCubemap(
					renderHelper,
					cubemap,
					Rendering::Format::R32G32B32A32_SFLOAT,
					{cubemapSize, cubemapSize},
					cubemapMipCount,
					Rendering::GetFormatInfo(Rendering::Format::R32G32B32A32_SFLOAT).m_blockDataSize,
					uncompressedCubemapTexture
				))
		{
			LogError("DownloadCubemap failed");
			callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{cubemapAssetData});
			return;
		}

		const IO::PathView assetFileName = asset.GetMetaDataFilePath().GetWithoutExtensions();
		Threading::JobRunnerThread& currentThread = *Threading::JobRunnerThread::GetCurrent();

		{
			IO::Path diffuseAssetPath =
				IO::Path::Merge(assetFileName, MAKE_PATH_LITERAL("Diffuse"), TextureAssetType::AssetFormat.metadataFileExtension);

			Serialization::Data diffuseTextureAssetData(diffuseAssetPath);
			Rendering::TextureAsset diffuseTextureAsset(diffuseTextureAssetData, Move(diffuseAssetPath));
			if (!diffuseTextureAsset.IsValid())
			{
				diffuseTextureAsset.RegenerateGuid();
			}

			diffuseTextureAsset.SetFlags(diffuseTextureAsset.GetFlags() | Rendering::ImageFlags::Cubemap);
			diffuseTextureAsset.SetUsageFlags(diffuseTextureAsset.GetUsageFlags() | Rendering::UsageFlags::TransferSource);

			Threading::JobBatch compileTextureJobBatch = CompileUncompressedTexture(
				uncompressedDiffuseCubemapTexture,
				(uint8)bufferChannelCount,
				32,
				6,
				Move(diffuseTextureAssetData),
				Move(diffuseTextureAsset),
				platforms,
				CompileCallback(callback)
			);
			currentThread.Queue(compileTextureJobBatch);
		}

		{
			IO::Path specularAssetPath =
				IO::Path::Merge(assetFileName, MAKE_PATH_LITERAL("Specular"), TextureAssetType::AssetFormat.metadataFileExtension);

			Serialization::Data specularTextureAssetData(specularAssetPath);
			Rendering::TextureAsset specularTextureAsset(specularTextureAssetData, Move(specularAssetPath));
			if (!specularTextureAsset.IsValid())
			{
				specularTextureAsset.RegenerateGuid();
			}

			specularTextureAsset.SetFlags(specularTextureAsset.GetFlags() | Rendering::ImageFlags::Cubemap);
			specularTextureAsset.SetUsageFlags(specularTextureAsset.GetUsageFlags() | Rendering::UsageFlags::TransferSource);

			Threading::JobBatch compileTextureJobBatch = CompileUncompressedTexture(
				uncompressedSpecularCubemapTexture,
				(uint8)bufferChannelCount,
				32,
				6,
				Move(specularTextureAssetData),
				Move(specularTextureAsset),
				platforms,
				CompileCallback(callback)
			);
			currentThread.Queue(compileTextureJobBatch);
		}

		{
			Rendering::TextureAsset cubemapTextureAsset(cubemapAssetData, IO::Path(asset.GetMetaDataFilePath()));
			if (!cubemapTextureAsset.IsValid())
			{
				cubemapTextureAsset.RegenerateGuid();
			}

			Threading::JobBatch compileTextureJobBatch = CompileUncompressedTexture(
				uncompressedCubemapTexture,
				(uint8)bufferChannelCount,
				32,
				6,
				Move(cubemapAssetData),
				Move(cubemapTextureAsset),
				platforms,
				Move(callback)
			);
			currentThread.Queue(compileTextureJobBatch);
		}

		cubemap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
		panoramaImage.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
	}

	Threading::Job* GenericTextureCompiler::CompileHDR(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread&,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return &Threading::CreateCallback(
			[sourceFilePath,
		   flags,
		   callback = Move(callback),
		   platforms,
		   specularAssetData = Move(assetData),
		   asset = Move(asset)](Threading::JobRunnerThread&) mutable
			{
				float* pImageData;
				Math::Vector2ui size;
				uint8 bufferChannelCount;
				{
					const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
					if (!sourceFile.IsValid())
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{specularAssetData});
						return;
					}

					int width, height, textureChannelCountOnDisk;
					stbi_info_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk);

					Expect(width > 0);
					Expect(height > 0);
					Expect(textureChannelCountOnDisk > 0);

					bufferChannelCount = GetRequiredPrecompressionChannelCount((uint8)textureChannelCountOnDisk);

					pImageData =
						stbi_loadf_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk, bufferChannelCount);
					size = {(uint32)width, (uint32)height};
				}

				CompileHDRCubemap(
					ConstByteView(pImageData, size.x * size.y * bufferChannelCount * sizeof(float)),
					size,
					flags,
					callback,
					platforms,
					specularAssetData,
					asset
				);
				ngine::Memory::Deallocate(pImageData);
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	Threading::Job* GenericTextureCompiler::CompileEXR(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread&,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return &Threading::CreateCallback(
			[sourceFilePath,
		   flags,
		   callback = Move(callback),
		   platforms,
		   specularAssetData = Move(assetData),
		   asset = Move(asset)](Threading::JobRunnerThread&) mutable
			{
				FixedSizeVector<unsigned char> imageData;

				{
					const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::Read | IO::AccessModeFlags::Binary);
					if (!sourceFile.IsValid())
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{specularAssetData});
						return;
					}

					imageData = FixedSizeVector<unsigned char>(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
					if (!sourceFile.ReadIntoView(imageData.GetView()))
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{specularAssetData});
						return;
					}
				}

				float* pImageData;
				Math::Vector2ui size;
				{
					int width, height;
					const char* error = nullptr;
					const int result = LoadEXRFromMemory(&pImageData, &width, &height, imageData.GetData(), imageData.GetDataSize(), &error);
					if (result != TINYEXR_SUCCESS)
					{
						FreeEXRErrorMessage(error);
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{specularAssetData});
						return;
					}
					size = {(uint32)width, (uint32)height};

					constexpr ConstStringView IntensityFactorName = "intensity_factor";
					const Serialization::Reader reader(specularAssetData);
					const float intensityScaling = reader.ReadWithDefaultValue<float>(IntensityFactorName, 1.f);
					// By default the cubemaps are very dark, so we add this internal factor on-top
				  // TODO: Investigate why we need to do this
					constexpr float internalIntensityFactor = 3.0f;
					const ArrayView<Math::Vector4f> pixels{reinterpret_cast<Math::Vector4f*>(pImageData), size.x * size.y};
					for (Math::Vector4f& __restrict pixel : pixels)
					{
						Math::Vector3f pixel3 = Math::Vector3f{pixel.x, pixel.y, pixel.z} * intensityScaling * internalIntensityFactor;
						pixel3 = Math::Min(pixel3, Math::Vector3f{1.f});
						pixel.x = pixel3.x;
						pixel.y = pixel3.y;
						pixel.z = pixel3.z;
					}

					Serialization::Writer writer(specularAssetData);
					writer.Serialize(IntensityFactorName, intensityScaling);
				}

				const uint8 bufferChannelCount = 4;

				CompileHDRCubemap(
					ConstByteView(pImageData, size.x * size.y * bufferChannelCount),
					size,
					flags,
					callback,
					platforms,
					specularAssetData,
					asset
				);
				ngine::Memory::Deallocate(pImageData);
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	Threading::Job* GenericTextureCompiler::CompileBRDF(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread&,
		const AssetCompiler::Plugin&,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context&,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return new ExecuteCallbackAndAwaitJob(
			[flags, callback = Move(callback), platforms, assetData = Move(assetData), asset = Move(asset)](Threading::JobRunnerThread&) mutable
			{
				constexpr uint8 brdfChannelCount = 2;
				constexpr uint32 brdfRenderTargetSize = 256u;
				constexpr uint32 sampleCount = 1024;
				gli::texture2d uncompressedBrdfTexture(
					static_cast<gli::format>(GetUncompressedTextureFormat(brdfChannelCount, 8)),
					gli::extent2d{brdfRenderTargetSize, brdfRenderTargetSize},
					1
				);

				UniquePtr<RenderHelper> pRenderHelper = UniquePtr<RenderHelper>::Make(PROFILE_BUILD != 0);
				if (!pRenderHelper->IsValid())
				{
					LogError("Render helper is not valid");
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				RenderHelper& renderHelper = *pRenderHelper;

				const bool brdfSamplingResult = SampleBRDF(renderHelper, uncompressedBrdfTexture, Distribution::GGX, sampleCount);

				if (!brdfSamplingResult)
				{
					LogError("SampleBRDF failed");
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return Threading::JobBatch{};
				}

				Rendering::TextureAsset textureAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
				if (!textureAsset.IsValid())
				{
					textureAsset.RegenerateGuid();
				}

				textureAsset.DisableMipGeneration();
				textureAsset.SetPreset(Rendering::TexturePreset::BRDF);
				textureAsset.SetUsageFlags(textureAsset.GetUsageFlags() | Rendering::UsageFlags::TransferSource);
				Serialization::Serialize(assetData, textureAsset);

				return CompileUncompressedTexture(
					uncompressedBrdfTexture,
					brdfChannelCount,
					8,
					1,
					Move(assetData),
					Move(textureAsset),
					platforms,
					Move(callback)
				);
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	[[nodiscard]] bool GenericTextureCompiler::HasAlphaChannel(const IO::Path& sourceFilePath)
	{
		const IO::PathView fileExtensions = sourceFilePath.GetAllExtensions();
		if (fileExtensions == MAKE_PATH(".jpg") || fileExtensions == MAKE_PATH(".png") || fileExtensions == MAKE_PATH(".psd") || fileExtensions == MAKE_PATH(".tga") || fileExtensions == MAKE_PATH(".bmp") || fileExtensions == MAKE_PATH(".gif") || fileExtensions == MAKE_PATH(".ptc") || fileExtensions == MAKE_PATH(".pnm"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return false;
			}

			int width, height, textureChannelCountOnDisk;
			if (!stbi_info_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk))
			{
				return false;
			}

			return textureChannelCountOnDisk == STBI_grey_alpha || textureChannelCountOnDisk == STBI_rgb_alpha;
		}
#if SUPPORT_TIFF
		else if (fileExtensions == MAKE_PATH(".tif") || fileExtensions == MAKE_PATH(".tiff"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return false;
			}

			TIFFClient tiffClient{Vector<ByteType, uint64>(Memory::ConstructWithSize, Memory::Uninitialized, sourceFile.GetSize())};
			if (!sourceFile.ReadIntoView(tiffClient.m_data.GetView()))
			{
				return false;
			}

			TIFF* const pTifFile = TIFFClientOpen(
				"MEMTIFF",
				"rb",
				(thandle_t)&tiffClient,
				TIFFClient::Read,
				TIFFClient::Write,
				TIFFClient::Seek,
				TIFFClient::Close,
				TIFFClient::Size,
				TIFFClient::Map,
				TIFFClient::Unmap
			);

			if (pTifFile == nullptr)
			{
				return false;
			}

			TIFFSetErrorHandler(
				[](const char* module, const char* message, va_list)
				{
					UNUSED(module);
					UNUSED(message);
				}
			);

			uint32 sampleInfo = 0;
			TIFFGetField(pTifFile, TIFFTAG_EXTRASAMPLES, &sampleInfo);
			return sampleInfo == EXTRASAMPLE_ASSOCALPHA || sampleInfo == EXTRASAMPLE_UNASSALPHA;
		}
#endif
		else if (fileExtensions == MAKE_PATH(".dds"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return false;
			}

			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return false;
			}

			gli::texture texture = gli::load_dds(sourceFileContents.GetData(), sourceFileContents.GetSize());
			return Rendering::GetFormatInfo(static_cast<Rendering::Format>(texture.format())).m_componentCount == 4;
		}
		else if (fileExtensions == MAKE_PATH(".ktx"))
		{

			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return false;
			}
			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return false;
			}

			gli::texture texture = gli::load_ktx(sourceFileContents.GetData(), sourceFileContents.GetSize());
			return Rendering::GetFormatInfo(static_cast<Rendering::Format>(texture.format())).m_componentCount == 4;
		}
		else if (fileExtensions == MAKE_PATH(".ktx2"))
		{
#if SUPPORT_LIBKTX
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return false;
			}

			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return false;
			}

			ktxTexture2* textureIn;
			const KTX_error_code result = ktxTexture2_CreateFromMemory(
				reinterpret_cast<const ktx_uint8_t*>(sourceFileContents.GetData()),
				sourceFileContents.GetDataSize(),
				KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
				&textureIn
			);
			if (result != KTX_SUCCESS)
			{
				return false;
			}

			const bool hasAlpha = Rendering::GetFormatInfo(static_cast<Rendering::Format>(textureIn->vkFormat)).m_componentCount == 4;
			ktxTexture_Destroy(ktxTexture(textureIn));
			return hasAlpha;
#endif
		}
		else if (fileExtensions == MAKE_PATH(".svg"))
		{
			return true;
		}

		return false;
	}

	GenericTextureCompiler::AlphaChannelUsageType GenericTextureCompiler::GetAlphaChannelUsageType(const IO::Path& sourceFilePath)
	{
		const IO::PathView fileExtensions = sourceFilePath.GetAllExtensions();
		if (fileExtensions == MAKE_PATH(".jpg") || fileExtensions == MAKE_PATH(".png") || fileExtensions == MAKE_PATH(".psd") || fileExtensions == MAKE_PATH(".tga") || fileExtensions == MAKE_PATH(".bmp") || fileExtensions == MAKE_PATH(".gif") || fileExtensions == MAKE_PATH(".ptc") || fileExtensions == MAKE_PATH(".pnm"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			int width, height, textureChannelCountOnDisk;
			if (!stbi_info_from_file(static_cast<FILE*>(sourceFile.GetFile()), &width, &height, &textureChannelCountOnDisk))
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			if (textureChannelCountOnDisk == STBI_grey_alpha || textureChannelCountOnDisk == STBI_rgb_alpha)
			{
				const bool is16bit = (bool)stbi_is_16_bit_from_file(static_cast<FILE*>(sourceFile.GetFile()));
				if (is16bit)
				{
					uint16* pImageData = stbi_load_from_file_16(
						static_cast<FILE*>(sourceFile.GetFile()),
						&width,
						&height,
						&textureChannelCountOnDisk,
						textureChannelCountOnDisk
					);

					EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

					const uint32 pixelCount = width * height;
					for (const Math::TColor<uint16>& pixel :
					     ArrayView<const Math::TColor<uint16>>{reinterpret_cast<const Math::TColor<uint16>*>(pImageData), pixelCount})
					{
						pixelFlags = GetPixelFlags(pixelFlags, pixel.a);
					}

					ngine::Memory::Deallocate(pImageData);
					return GetAlphaUsageType(pixelFlags);
				}
				else
				{
					uint8* const pImageData = stbi_load_from_file(
						static_cast<FILE*>(sourceFile.GetFile()),
						&width,
						&height,
						&textureChannelCountOnDisk,
						textureChannelCountOnDisk
					);

					EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

					const uint32 pixelCount = width * height;
					for (const Math::TColor<uint8>& pixel :
					     ArrayView<Math::TColor<uint8>>{reinterpret_cast<Math::TColor<uint8>*>(pImageData), pixelCount})
					{
						pixelFlags = GetPixelFlags(pixelFlags, pixel.a);
					}

					ngine::Memory::Deallocate(pImageData);
					return GetAlphaUsageType(pixelFlags);
				}
			}
		}
#if SUPPORT_TIFF
		else if (fileExtensions == MAKE_PATH(".tif") || fileExtensions == MAKE_PATH(".tiff"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			TIFFClient tiffClient{Vector<ByteType, uint64>(Memory::ConstructWithSize, Memory::Uninitialized, sourceFile.GetSize())};
			if (!sourceFile.ReadIntoView(tiffClient.m_data.GetView()))
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			};
			TIFF* const pTifFile = TIFFClientOpen(
				"MEMTIFF",
				"rb",
				(thandle_t)&tiffClient,
				TIFFClient::Read,
				TIFFClient::Write,
				TIFFClient::Seek,
				TIFFClient::Close,
				TIFFClient::Size,
				TIFFClient::Map,
				TIFFClient::Unmap
			);

			if (pTifFile == nullptr)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			TIFFSetErrorHandler(
				[](const char* module, const char* message, va_list)
				{
					UNUSED(module);
					UNUSED(message);
				}
			);

			uint32 sampleInfo = 0;
			TIFFGetField(pTifFile, TIFFTAG_EXTRASAMPLES, &sampleInfo);
			if (sampleInfo == EXTRASAMPLE_ASSOCALPHA || sampleInfo == EXTRASAMPLE_UNASSALPHA)
			{
				Math::Vector2ui imageSize;
				TIFFGetField(pTifFile, TIFFTAG_IMAGEWIDTH, &imageSize.x);
				TIFFGetField(pTifFile, TIFFTAG_IMAGELENGTH, &imageSize.y);

				uint32 textureChannelCountOnDisk = 0;
				TIFFGetField(pTifFile, TIFFTAG_SAMPLESPERPIXEL, &textureChannelCountOnDisk);
				uint32 numBitsPerChannel = 0;
				TIFFGetField(pTifFile, TIFFTAG_BITSPERSAMPLE, &numBitsPerChannel);

				const uint64 scanlineSize = TIFFScanlineSize64(pTifFile);
				FixedSizeVector<ByteType, uint64> scanlineData(Memory::ConstructWithSize, Memory::Uninitialized, scanlineSize);

				EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

				for (uint32 y = 0; y < imageSize.y; ++y)
				{
					[[maybe_unused]] const int result = TIFFReadScanline(pTifFile, scanlineData.GetData(), y);
					Assert(result == 1);

					switch (numBitsPerChannel)
					{
						case 8:
						{
							switch (textureChannelCountOnDisk)
							{
								case 2:
									for (const Array<uint8, 2>& pixel :
									     ArrayView<Array<uint8, 2>>{reinterpret_cast<Array<uint8, 2>*>(scanlineData.GetData()), imageSize.x})
									{
										pixelFlags = GetPixelFlags(pixelFlags, pixel[1]);
									}
									return GetAlphaUsageType(pixelFlags);
								case 4:
									for (const Array<uint8, 4>& pixel :
									     ArrayView<Array<uint8, 4>>{reinterpret_cast<Array<uint8, 4>*>(scanlineData.GetData()), imageSize.x})
									{
										pixelFlags = GetPixelFlags(pixelFlags, pixel[3]);
									}
									return GetAlphaUsageType(pixelFlags);
							}
						}
						break;
						case 16:
							switch (textureChannelCountOnDisk)
							{
								case 2:
									for (const Array<uint16, 2>& pixel :
									     ArrayView<Array<uint16, 2>>{reinterpret_cast<Array<uint16, 2>*>(scanlineData.GetData()), imageSize.x})
									{
										pixelFlags = GetPixelFlags(pixelFlags, pixel[1]);
									}
									return GetAlphaUsageType(pixelFlags);
								case 4:
									for (const Array<uint16, 4>& pixel :
									     ArrayView<Array<uint16, 4>>{reinterpret_cast<Array<uint16, 4>*>(scanlineData.GetData()), imageSize.x})
									{
										pixelFlags = GetPixelFlags(pixelFlags, pixel[3]);
									}
									return GetAlphaUsageType(pixelFlags);
							}
							break;
					}
				}

				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}
		}
#endif
		else if (fileExtensions == MAKE_PATH(".dds"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			gli::texture texture = gli::load_dds(sourceFileContents.GetData(), sourceFileContents.GetSize());
			const gli::texture::swizzles_type swizzles = texture.swizzles();
			if (swizzles.r != gli::SWIZZLE_ALPHA && swizzles.g != gli::SWIZZLE_ALPHA && swizzles.b != gli::SWIZZLE_ALPHA && swizzles.a != gli::SWIZZLE_ALPHA)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

			gli::fsampler2D sampler(gli::texture2d(texture), gli::WRAP_CLAMP_TO_EDGE);
			for (gli::texture::size_type layerIndex = 0; layerIndex < texture.layers(); ++layerIndex)
			{
				for (gli::texture::size_type faceIndex = 0; faceIndex < texture.faces(); ++faceIndex)
				{
					for (gli::texture::size_type levelIndex = 0; levelIndex < texture.levels(); ++levelIndex)
					{
						const gli::texture::extent_type dimensions = texture.extent(levelIndex);

						for (int j = 0; j < dimensions.y; ++j)
						{
							for (int i = 0; i < dimensions.x; ++i)
							{
								typename gli::texture2d::extent_type const texelCoordinate(gli::texture2d::extent_type(i, j));
								pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
							}
						}
					}
				}
			}
			return GetAlphaUsageType(pixelFlags);
		}
		else if (fileExtensions == MAKE_PATH(".ktx"))
		{
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}
			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			gli::texture texture = gli::load_ktx(sourceFileContents.GetData(), sourceFileContents.GetSize());

			const gli::texture::swizzles_type swizzles = texture.swizzles();
			if (swizzles.r != gli::SWIZZLE_ALPHA && swizzles.g != gli::SWIZZLE_ALPHA && swizzles.b != gli::SWIZZLE_ALPHA && swizzles.a != gli::SWIZZLE_ALPHA)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

			gli::fsampler2D sampler(gli::texture2d(texture), gli::WRAP_CLAMP_TO_EDGE);
			for (gli::texture::size_type layerIndex = 0; layerIndex < texture.layers(); ++layerIndex)
			{
				for (gli::texture::size_type faceIndex = 0; faceIndex < texture.faces(); ++faceIndex)
				{
					for (gli::texture::size_type levelIndex = 0; levelIndex < texture.levels(); ++levelIndex)
					{
						const gli::texture::extent_type dimensions = texture.extent(levelIndex);

						for (int j = 0; j < dimensions.y; ++j)
						{
							for (int i = 0; i < dimensions.x; ++i)
							{
								typename gli::texture2d::extent_type const texelCoordinate(gli::texture2d::extent_type(i, j));
								pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
							}
						}
					}
				}
			}
			return GetAlphaUsageType(pixelFlags);
		}
		else if (fileExtensions == MAKE_PATH(".ktx2"))
		{
#if SUPPORT_LIBKTX
			const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary);
			if (!sourceFile.IsValid())
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
			if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			ktxTexture2* textureIn;
			const KTX_error_code result = ktxTexture2_CreateFromMemory(
				reinterpret_cast<const ktx_uint8_t*>(sourceFileContents.GetData()),
				sourceFileContents.GetDataSize(),
				KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
				&textureIn
			);
			if (result != KTX_SUCCESS)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			const gli::texture::swizzles_type swizzles =
				gli::detail::get_format_info(static_cast<gli::texture::format_type>(textureIn->vkFormat)).Swizzles;
			if (swizzles.r != gli::SWIZZLE_ALPHA && swizzles.g != gli::SWIZZLE_ALPHA && swizzles.b != gli::SWIZZLE_ALPHA && swizzles.a != gli::SWIZZLE_ALPHA)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			gli::texture2d texture = gli::texture2d(
				static_cast<gli::texture::format_type>(textureIn->vkFormat),
				gli::texture2d::extent_type{textureIn->baseWidth, textureIn->baseHeight},
				textureIn->numLevels
			);

			Memory::CopyWithoutOverlap(texture.data(), textureIn->pData, textureIn->dataSize);

			EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

			gli::fsampler2D sampler(gli::texture2d(texture), gli::WRAP_CLAMP_TO_EDGE);
			for (gli::texture::size_type layerIndex = 0; layerIndex < texture.layers(); ++layerIndex)
			{
				for (gli::texture::size_type faceIndex = 0; faceIndex < texture.faces(); ++faceIndex)
				{
					for (gli::texture::size_type levelIndex = 0; levelIndex < texture.levels(); ++levelIndex)
					{
						const gli::texture2d::extent_type dimensions = texture.extent(levelIndex);

						for (int j = 0; j < dimensions.y; ++j)
						{
							for (int i = 0; i < dimensions.x; ++i)
							{
								typename gli::texture2d::extent_type const texelCoordinate(gli::texture2d::extent_type(i, j));
								pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, levelIndex)[swizzles[gli::SWIZZLE_ALPHA]]);
							}
						}
					}
				}
			}

			ktxTexture_Destroy(ktxTexture(textureIn));
			return GetAlphaUsageType(pixelFlags);
#endif
		}
		else if (fileExtensions == MAKE_PATH(".svg"))
		{
#if SUPPORT_LUNASVG
			std::unique_ptr<lunasvg::Document> svgDocument;
			{
				const IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
				if (UNLIKELY(!sourceFile.IsValid()))
				{
					return GenericTextureCompiler::AlphaChannelUsageType::None;
				}
				FixedSizeVector<char> sourceFileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)sourceFile.GetSize());
				if (!sourceFile.ReadIntoView(sourceFileContents.GetView()))
				{
					return GenericTextureCompiler::AlphaChannelUsageType::None;
				}

				svgDocument = lunasvg::Document::loadFromData(sourceFileContents.GetData(), sourceFileContents.GetDataSize());
			}

			gli::texture2d uncompressedTexture = gli::texture2d(
				gli::format::FORMAT_RGBA8_UNORM_PACK8,
				gli::extent2d{(uint32)svgDocument->width(), (uint32)svgDocument->height()},
				1
			);
			const gli::texture::swizzles_type swizzles = gli::detail::get_format_info(gli::format::FORMAT_RGBA8_UNORM_PACK8).Swizzles;
			if (swizzles.r != gli::SWIZZLE_ALPHA && swizzles.g != gli::SWIZZLE_ALPHA && swizzles.b != gli::SWIZZLE_ALPHA && swizzles.a != gli::SWIZZLE_ALPHA)
			{
				return GenericTextureCompiler::AlphaChannelUsageType::None;
			}

			lunasvg::Matrix matrix;
			matrix.identity();
			matrix.scale(1, 1);
			svgDocument->setMatrix(matrix);

			lunasvg::Bitmap targetBitmap(
				static_cast<uint8*>(uncompressedTexture.data()),
				uncompressedTexture.extent().x,
				uncompressedTexture.extent().y,
				uncompressedTexture.extent().x * 4
			);

			EnumFlags<PixelFlags> pixelFlags = PixelFlags::IsMasked | PixelFlags::AreAllPixelsOpaque | PixelFlags::AreAllPixelsInvisible;

			gli::fsampler2D sampler(gli::texture2d(uncompressedTexture), gli::WRAP_CLAMP_TO_EDGE);
			for (int j = 0; j < uncompressedTexture.extent().y; ++j)
			{
				for (int i = 0; i < uncompressedTexture.extent().x; ++i)
				{
					typename gli::texture2d::extent_type const texelCoordinate(gli::texture2d::extent_type(i, j));
					pixelFlags = GetPixelFlags(pixelFlags, sampler.texel_fetch(texelCoordinate, 0)[swizzles[gli::SWIZZLE_ALPHA]]);
				}
			}
			return GetAlphaUsageType(pixelFlags);
#endif
		}

		return GenericTextureCompiler::AlphaChannelUsageType::None;
	}
}
