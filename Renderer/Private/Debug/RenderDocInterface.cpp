#include "Debug/RenderDocInterface.h"

#if ENABLE_RENDERDOC_API
#include <Common/IO/Library.h>
#include <Common/IO/Path.h>

#include "3rdparty/renderdoc/include/renderdoc_app.h"

namespace ngine::Rendering::RenderDoc
{
	static RENDERDOC_API_1_5_0* GetRenderDocAPIInternal()
	{
		RENDERDOC_API_1_5_0* renderDocAPI = nullptr;
		static IO::Library renderDocLibrary(IO::Path::Merge(IO::Library::FileNamePrefix, MAKE_PATH("renderdoc"), IO::Library::FileNamePostfix));
		if (renderDocLibrary.IsValid())
		{
			const pRENDERDOC_GetAPI getRenderDocAPI = renderDocLibrary.GetProcedureAddress<pRENDERDOC_GetAPI>("RENDERDOC_GetAPI");
			if (getRenderDocAPI != nullptr)
			{
				const int result = getRenderDocAPI(eRENDERDOC_API_Version_1_5_0, reinterpret_cast<void**>(&renderDocAPI));
				if (result == 1)
				{
					return renderDocAPI;
				}
			}
		}
		return nullptr;
	}
	static RENDERDOC_API_1_5_0* GetRenderDocAPI()
	{
		static RENDERDOC_API_1_5_0* renderDocAPI = GetRenderDocAPIInternal();
		return renderDocAPI;
	}

	extern void StartFrameCapture()
	{
		if (RENDERDOC_API_1_5_0* renderDocAPI = GetRenderDocAPI())
		{
			renderDocAPI->StartFrameCapture(nullptr, nullptr);
		}
	}

	extern void CancelFrameCapture()
	{
		if (RENDERDOC_API_1_5_0* renderDocAPI = GetRenderDocAPI())
		{
			renderDocAPI->DiscardFrameCapture(nullptr, nullptr);
		}
	}

	extern void EndFrameCapture()
	{
		if (RENDERDOC_API_1_5_0* renderDocAPI = GetRenderDocAPI())
		{
			renderDocAPI->EndFrameCapture(nullptr, nullptr);
		}
	}
}
#endif
