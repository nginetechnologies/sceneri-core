#include "Assets/Material/RuntimeMaterialInstance.h"
#include "Assets/Material/MaterialInstanceAssetType.h"

#include <Common/System/Query.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Math/Color.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Stages/Serialization/RenderItemStageMask.h>

namespace ngine::Rendering
{
	RuntimeMaterialInstance::RuntimeMaterialInstance(
		const MaterialIdentifier materialIdentifier,
		const MaterialInstanceIdentifier templateMaterialInstanceIdentifier,
		DescriptorContents&& descriptorContents,
		const EnumFlags<Flags> flags
	)
		: m_materialIdentifier(materialIdentifier)
		, m_templateMaterialInstanceIdentifier(templateMaterialInstanceIdentifier)
		, m_descriptorContents(Move(descriptorContents))
		, m_flags(flags & Flags::IsClone)
	{
		Assert(materialIdentifier.IsValid());
	}

	RuntimeMaterialInstance::RuntimeMaterialInstance(
		const MaterialInstanceIdentifier templateMaterialInstanceIdentifier, const EnumFlags<Flags> flags
	)
		: m_templateMaterialInstanceIdentifier(templateMaterialInstanceIdentifier)
		, m_flags(flags & Flags::IsClone)
	{
	}

	bool RuntimeMaterialInstance::Serialize(const Serialization::Reader serializer, const MaterialIdentifier materialIdentifier)
	{
		m_materialIdentifier = materialIdentifier;

		const RuntimeMaterial& __restrict material = *System::Get<Rendering::Renderer>().GetMaterialCache().GetMaterial(materialIdentifier);
		Assert(material.IsValid());
		const Rendering::MaterialAsset& materialAsset = *material.GetAsset();

		RestrictedArrayView<const MaterialAsset::DescriptorBinding, uint8> descriptorBindings = materialAsset.GetDescriptorBindings();
		m_descriptorContents.Reserve(descriptorBindings.GetSize());

		if (const Optional<Serialization::Reader> descriptorContentsReader = serializer.FindSerializer("descriptor_contents"))
		{
			for (const Serialization::Reader contentSerializer : descriptorContentsReader->GetArrayView())
			{
				Assert(!descriptorBindings.IsEmpty());
				if (UNLIKELY_ERROR(descriptorBindings.IsEmpty()))
				{
					continue;
				}

				const MaterialAsset::DescriptorBinding& binding = descriptorBindings[0];

				switch (binding.m_type)
				{
					case DescriptorContentType::Invalid:
						ExpectUnreachable();
					case DescriptorContentType::Texture:
					{
						Asset::Guid textureAssetGuid = binding.m_samplerInfo.m_defaultTextureGuid;
						contentSerializer.Serialize("texture", textureAssetGuid);

						Rendering::AddressMode addressMode = binding.m_samplerInfo.m_defaultAddressMode;
						contentSerializer.Serialize("address_mode", addressMode);

						const TextureIdentifier textureIdentifier =
							System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(textureAssetGuid);

						m_descriptorContents.EmplaceBack(textureIdentifier, addressMode);
					}
					break;
				}

				descriptorBindings++;
			}
		}

		Assert(
			materialAsset.GetDescriptorBindings().IsEmpty() || descriptorBindings.GetSize() < materialAsset.GetDescriptorBindings().GetSize()
		);

		const ArrayView pushConstantDefinitions = materialAsset.GetPushConstants();
		if (!serializer.Serialize("push_constants", m_pushConstantData, pushConstantDefinitions))
		{
			m_pushConstantData.ResetToDefaults(pushConstantDefinitions);
		}
		return true;
	}

	bool RuntimeMaterialInstance::Serialize(Serialization::Writer serializer) const
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		MaterialCache& materialCache = renderer.GetMaterialCache();
		if (!serializer.Serialize("material", materialCache.GetAssetGuid(m_materialIdentifier)))
		{
			return false;
		}

		const RuntimeMaterial& __restrict material = *materialCache.GetMaterial(m_materialIdentifier);
		Assert(material.IsValid());
		const Rendering::MaterialAsset& materialAsset = *material.GetAsset();

		if (m_descriptorContents.HasElements())
		{
			TextureCache& textureCache = renderer.GetTextureCache();

			RestrictedArrayView<const RuntimeDescriptorContent, uint8> descriptorContents = m_descriptorContents.GetView();

			if (!serializer.SerializeArrayWithCallback(
						"descriptor_contents",
						[&textureCache, descriptorContents](Serialization::Writer writer, const uint8 index)
						{
							const RuntimeDescriptorContent& __restrict descriptorContent = descriptorContents[index];
							switch (descriptorContent.GetType())
							{
								case DescriptorContentType::Texture:
								{
									const Asset::Guid textureAssetGuid = textureCache.GetAssetGuid(descriptorContent.m_textureData.m_textureIdentifier);
									return writer.SerializeInPlace(MaterialInstanceDescriptorContent{
										DescriptorContentType::Texture,
										textureAssetGuid,
										descriptorContent.m_textureData.m_addressMode
									});
								}
								case DescriptorContentType::Invalid:
									Assert(false);
									return false;
							}
							ExpectUnreachable();
						},
						descriptorContents.GetSize()
					))
			{
				return false;
			}
		}

		if (m_pushConstantData.GetPushConstantsData().HasElements())
		{
			const ArrayView<const PushConstantDefinition, uint8> pushConstantDefinitions = materialAsset.GetPushConstants();
			if (!serializer.Serialize("push_constants", m_pushConstantData, pushConstantDefinitions))
			{
				return false;
			}
		}

		return true;
	}

	void RuntimeMaterialInstance::OnLoadingFinished()
	{
		Assert(m_materialIdentifier.IsValid());
		[[maybe_unused]] const bool wasSet = m_flags.TrySetFlags(Flags::WasLoaded);
		Assert(wasSet);
	}

	void RuntimeMaterialInstance::OnLoadingFailed()
	{
		[[maybe_unused]] const bool wasSet = m_flags.TrySetFlags(Flags::FailedLoading);
		Assert(wasSet);
	}

	void
	RuntimeMaterialInstance::SwitchParentMaterial(const MaterialInstanceIdentifier identifier, const MaterialIdentifier newMaterialIdentifier)
	{
		Assert(identifier.IsValid());
		Assert(newMaterialIdentifier.IsValid());

		MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		RuntimeMaterial& previousMaterial = *materialCache.GetAssetData(m_materialIdentifier).m_pMaterial;
		const Rendering::MaterialAsset& previousMaterialAsset = *previousMaterial.GetAsset();

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		const RuntimeMaterial& newMaterial = *materialCache.GetMaterial(newMaterialIdentifier);
		const Rendering::MaterialAsset& newMaterialAsset = *newMaterial.GetAsset();
		DescriptorContents newDescriptorContents(Memory::Reserve, newMaterialAsset.GetDescriptorBindings().GetSize());

		const PushConstantsData previousPushConstantsData = Move(m_pushConstantData);
		m_pushConstantData.ResetToDefaults(newMaterialAsset.GetPushConstants());
		TByteView<ByteType, size> pushConstantDataView = m_pushConstantData.GetPushConstantsData();

		// Copy over data with identical push constant presets
		for (const PushConstantDefinition& pushConstantDefinition : newMaterialAsset.GetPushConstants())
		{
			const OptionalIterator<const PushConstantDefinition> previousPushConstantDefinition = previousMaterialAsset.GetPushConstants().FindIf(
				[pushConstantPreset = pushConstantDefinition.m_preset](const PushConstantDefinition& previousPushConstantDefinition)
				{
					return previousPushConstantDefinition.m_preset == pushConstantPreset;
				}
			);
			if (previousPushConstantDefinition.IsValid())
			{
				TByteView<const ByteType, size> previousPushConstantDataView = previousPushConstantsData.GetPushConstantsData();
				for (auto previousIt = previousMaterialAsset.GetPushConstants().begin(); previousIt != previousPushConstantDefinition; ++previousIt)
				{
					previousPushConstantDataView += previousIt->m_size;
				}

				Assert(previousPushConstantDefinition->m_size == pushConstantDefinition.m_size);
				pushConstantDataView.CopyFrom(previousPushConstantDataView.GetSubView((size)0, (size)previousPushConstantDefinition->m_size));
			}

			pushConstantDataView += pushConstantDefinition.m_size;
		}

		// Copy over texutres with identical texture presets
		for (const MaterialAsset::DescriptorBinding& descriptorBinding : newMaterialAsset.GetDescriptorBindings())
		{
			const OptionalIterator<const MaterialAsset::DescriptorBinding> previousBinding = previousMaterialAsset.GetDescriptorBindings().FindIf(
				[texturePreset = descriptorBinding.m_samplerInfo.m_texturePreset](const MaterialAsset::DescriptorBinding& previousDescriptorBinding)
				{
					return previousDescriptorBinding.m_samplerInfo.m_texturePreset == texturePreset;
				}
			);
			if (previousBinding.IsValid())
			{
				const uint8 previousDescriptorIndex = previousMaterialAsset.GetDescriptorBindings().GetIteratorIndex(previousBinding);
				newDescriptorContents.EmplaceBack(m_descriptorContents[previousDescriptorIndex]);
			}
			else
			{
				newDescriptorContents.EmplaceBack(
					textureCache.FindOrRegisterAsset(descriptorBinding.m_samplerInfo.m_defaultTextureGuid),
					descriptorBinding.m_samplerInfo.m_defaultAddressMode
				);
			}
		}

		m_descriptorContents = Move(newDescriptorContents);

		const Rendering::MaterialIdentifier previousMaterialIdentifier = m_materialIdentifier;
		m_materialIdentifier = newMaterialIdentifier;

		OnParentMaterialChanged();

		previousMaterial.OnMaterialInstanceParentChanged(identifier, previousMaterialIdentifier, newMaterialIdentifier);
	}

	void RuntimeMaterialInstance::UpdateDescriptorContent(const uint8 index, DescriptorContent&& newContent)
	{
		Assert(index < m_descriptorContents.GetSize());
		const DescriptorContent previousData = Move(m_descriptorContents[index]);
		m_descriptorContents[index] = Forward<DescriptorContent>(newContent);
		OnDescriptorContentChanged(previousData, index);
	}

	[[maybe_unused]] const bool wasMaterialInstanceTypeRegistered = Reflection::Registry::RegisterType<MaterialInstanceAssetType>();
}
