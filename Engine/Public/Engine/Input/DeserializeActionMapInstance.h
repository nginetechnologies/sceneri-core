#pragma once

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/Manager.h>
#include <Engine/Reflection/Registry.h>
#include <Common/System/Query.h>
#include <Common/Memory/Optional.h>
#include <Common/Function/Function.h>

#include <Engine/Input/ActionMap.h>

#include <Common/Serialization/Deserialize.h>

namespace ngine::Input
{
	[[nodiscard]] inline Threading::Job*
	DeserializeInstanceAsync(const Asset::Guid assetGuid, ActionMonitor& actionMonitor, Function<void(UniquePtr<ActionMap>&&), 24>&& callback)
	{

		return System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
			assetGuid,
			Threading::JobPriority::DeserializeComponent,
			[&actionMonitor, callback = Forward<decltype(callback)>(callback)](const ConstByteView data)
			{
				if (UNLIKELY(!data.HasElements()))
				{
					callback(nullptr);
					return;
				}

				Serialization::RootReader serializer = Serialization::GetReaderFromBuffer(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
				);

				Assert(serializer.GetData().IsValid());
				if (LIKELY(serializer.GetData().IsValid()))
				{
					auto pActionMap = UniquePtr<Input::ActionMap>::Make(actionMonitor, serializer);
					callback(Move(pActionMap));
				}
				else
				{
					callback(nullptr);
				}
			}
		);
	}
}
