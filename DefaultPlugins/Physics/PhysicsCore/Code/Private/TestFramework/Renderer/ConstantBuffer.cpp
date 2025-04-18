// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include "Plugin.h"
#if ENABLE_JOLT_DEBUG_RENDERER

#include <TestFramework/TestFramework.h>

#include <TestFramework/Renderer/ConstantBuffer.h>
#include <TestFramework/Renderer/Renderer.h>
#include <TestFramework/Renderer/FatalErrorIfFailed.h>

namespace JPH::DebugRendering
{
	ConstantBuffer::ConstantBuffer(Renderer* inRenderer, uint64 inBufferSize)
		: mRenderer(inRenderer)
	{
		mBuffer = mRenderer->CreateD3DResourceOnUploadHeap(inBufferSize);
		mBufferSize = inBufferSize;
	}

	ConstantBuffer::~ConstantBuffer()
	{
		if (mBuffer != nullptr)
			mRenderer->RecycleD3DResourceOnUploadHeap(mBuffer.Get(), mBufferSize);
	}

	void* ConstantBuffer::MapInternal()
	{
		void* mapped_resource;
		D3D12_RANGE range = {0, 0}; // We're not going to read
		FatalErrorIfFailed(mBuffer->Map(0, &range, &mapped_resource));
		return mapped_resource;
	}

	void ConstantBuffer::Unmap()
	{
		mBuffer->Unmap(0, nullptr);
	}

	void ConstantBuffer::Bind(int inSlot)
	{
		mRenderer->GetCommandList()->SetGraphicsRootConstantBufferView(inSlot, mBuffer->GetGPUVirtualAddress());
	}
}

#endif
