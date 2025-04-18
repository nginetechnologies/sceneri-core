#pragma once

#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetFormat.h>

namespace ngine::Rendering
{
	struct ShaderAsset : public Asset::Asset
	{
		inline static constexpr ngine::Asset::Format FragmentAssetFormat = {
			"{1661FFCA-AB01-4D30-B839-B14F29DCF489}"_guid, MAKE_PATH(".fragshader.nasset"), MAKE_PATH(".fragshader")
		};
		inline static constexpr ngine::Asset::Format VertexAssetFormat = {
			"{9C78C65F-A802-47AD-B6A2-9611A547D0D5}"_guid, MAKE_PATH(".vertshader.nasset"), MAKE_PATH(".vertshader")
		};
		inline static constexpr ngine::Asset::Format GeometryAssetFormat = {
			"{50D10DBF-54B3-49B7-B083-9698C5C738AF}"_guid, MAKE_PATH(".geomshader.nasset"), MAKE_PATH(".geomshader")
		};
		inline static constexpr ngine::Asset::Format ComputeAssetFormat = {
			"{ECA78F5C-9A68-4AA9-92C9-0FEE32A4EB7F}"_guid, MAKE_PATH(".compshader.nasset"), MAKE_PATH(".compshader")
		};
		inline static constexpr ngine::Asset::Format TessellationControlAssetFormat = {
			"f47d512d-6ed9-4835-88cb-5f29182a9224"_guid, MAKE_PATH(".tescshader.nasset"), MAKE_PATH(".tescshader")
		};
		inline static constexpr ngine::Asset::Format TessellationEvaluationAssetFormat = {
			"7b7486ca-fb79-4127-ab34-b4df2bc07c48"_guid, MAKE_PATH(".teseshader.nasset"), MAKE_PATH(".teseshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingGenerationAssetFormat = {
			"7d0161fb-1326-4343-b145-50759b09993a"_guid, MAKE_PATH(".rgenshader.nasset"), MAKE_PATH(".rgenshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingIntersectionAssetFormat = {
			"1b6918cd-41bf-4789-aafe-79e0563793fe"_guid, MAKE_PATH(".rintshader.nasset"), MAKE_PATH(".rintshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingAnyHitAssetFormat = {
			"eedb01f2-63a1-4c0f-ade8-e1910da0df7b"_guid, MAKE_PATH(".rahitshader.nasset"), MAKE_PATH(".rahitshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingClosestHitAssetFormat = {
			"776f9f70-2357-4a4d-8243-b0fecf20f0de"_guid, MAKE_PATH(".rchitshader.nasset"), MAKE_PATH(".rchitshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingMissAssetFormat = {
			"bb3e327d-9824-4ee5-bf3d-ac1eac17a2be"_guid, MAKE_PATH(".rmissshader.nasset"), MAKE_PATH(".rmissshader")
		};
		inline static constexpr ngine::Asset::Format RaytracingCallableAssetFormat = {
			"72bc539b-7364-4a38-9c25-7cbb5c74204b"_guid, MAKE_PATH(".rcallshader.nasset"), MAKE_PATH(".callshader")
		};

		inline static constexpr IO::PathView FragmentSourceFileExtension = MAKE_PATH(".frag");
		inline static constexpr IO::PathView VertexSourceFileExtension = MAKE_PATH(".vert");
		inline static constexpr IO::PathView GeometrySourceFileExtension = MAKE_PATH(".geom");
		inline static constexpr IO::PathView ComputeSourceFileExtension = MAKE_PATH(".comp");
		inline static constexpr IO::PathView TessellationControlSourceFileExtension = MAKE_PATH(".tesc");
		inline static constexpr IO::PathView TessellationEvaluationSourceFileExtension = MAKE_PATH(".tese");
		inline static constexpr IO::PathView RaytracingGenerationSourceFileExtension = MAKE_PATH(".rgen");
		inline static constexpr IO::PathView RaytracingIntersectionSourceFileExtension = MAKE_PATH(".rint");
		inline static constexpr IO::PathView RaytracingAnyHitSourceFileExtension = MAKE_PATH(".rahit");
		inline static constexpr IO::PathView RaytracingClosestHitSourceFileExtension = MAKE_PATH(".rchit");
		inline static constexpr IO::PathView RaytracingMissSourceFileExtension = MAKE_PATH(".rmiss");
		inline static constexpr IO::PathView RaytracingCallableSourceFileExtension = MAKE_PATH(".rcall");

		inline static constexpr IO::PathView GetBinaryFileExtensionFromSource(const IO::PathView sourceFileExtension)
		{
			if (sourceFileExtension == FragmentSourceFileExtension)
			{
				return FragmentAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == VertexSourceFileExtension)
			{
				return VertexAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == GeometrySourceFileExtension)
			{
				return GeometryAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == ComputeSourceFileExtension)
			{
				return ComputeAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == TessellationControlSourceFileExtension)
			{
				return TessellationControlAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == TessellationEvaluationSourceFileExtension)
			{
				return TessellationEvaluationAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingGenerationSourceFileExtension)
			{
				return RaytracingGenerationAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingIntersectionSourceFileExtension)
			{
				return RaytracingIntersectionAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingAnyHitSourceFileExtension)
			{
				return RaytracingAnyHitAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingClosestHitSourceFileExtension)
			{
				return RaytracingClosestHitAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingMissSourceFileExtension)
			{
				return RaytracingMissAssetFormat.binaryFileExtension;
			}
			else if (sourceFileExtension == RaytracingCallableSourceFileExtension)
			{
				return RaytracingCallableAssetFormat.binaryFileExtension;
			}

			Assert(false);
			return IO::PathView();
		}

		using Asset::Asset;
	};
}
