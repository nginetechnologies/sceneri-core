#pragma once

#include <Renderer/Assets/Material/MaterialIdentifier.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Assets/Material/DescriptorContentType.h>
#include <Renderer/Assets/Material/MaterialInstanceFlags.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Wrappers/AddressMode.h>
#include <Renderer/PushConstants/PushConstantsData.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/Identifier.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/IO/PathView.h>
#include <Common/Function/Event.h>
#include <Common/AtomicEnumFlags.h>

#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine
{
	struct Engine;

	namespace Asset
	{
		struct Guid;
	}

	namespace IO
	{
		struct Path;
	}

	namespace Threading
	{
		struct Job;
	}
}

namespace ngine::Rendering
{
	struct RuntimeDescriptorContent
	{
		using Type = DescriptorContentType;

		RuntimeDescriptorContent(const Rendering::TextureIdentifier textureIdentifier, const AddressMode addressMode)
			: m_type(Type::Texture)
		{
			m_textureData.m_textureIdentifier = textureIdentifier;
			m_textureData.m_addressMode = addressMode;
		}
		RuntimeDescriptorContent(const RuntimeDescriptorContent& other)
		{
			Memory::CopyNonOverlappingElement(*this, other);
		}
		RuntimeDescriptorContent(RuntimeDescriptorContent&& other)
		{
			Memory::CopyNonOverlappingElement(*this, static_cast<const RuntimeDescriptorContent&>(other));
		}
		RuntimeDescriptorContent& operator=(const RuntimeDescriptorContent& other)
		{
			Memory::CopyNonOverlappingElement(*this, other);
			return *this;
		}
		RuntimeDescriptorContent& operator=(RuntimeDescriptorContent&& other)
		{
			Memory::CopyNonOverlappingElement(*this, static_cast<const RuntimeDescriptorContent&>(other));
			return *this;
		}

		[[nodiscard]] Type GetType() const
		{
			return m_type;
		}

		Type m_type;
		union
		{
			struct
			{
				Rendering::TextureIdentifier m_textureIdentifier;
				AddressMode m_addressMode;
			} m_textureData;
		};
	};

	struct MaterialInstanceCache;

	struct RuntimeMaterialInstance
	{
		using Flags = MaterialInstanceFlags;

		using DescriptorContent = RuntimeDescriptorContent;
		using DescriptorContents = Vector<DescriptorContent, uint8>;

		RuntimeMaterialInstance(const MaterialInstanceIdentifier templateMaterialInstanceIdentifier, const EnumFlags<Flags> flags);
		RuntimeMaterialInstance(
			const MaterialIdentifier materialIdentifier,
			const MaterialInstanceIdentifier templateMaterialInstanceIdentifier,
			DescriptorContents&& descriptorContents,
			const EnumFlags<Flags> flags
		);
		RuntimeMaterialInstance(const RuntimeMaterialInstance&) = delete;
		RuntimeMaterialInstance(RuntimeMaterialInstance&&) = delete;
		RuntimeMaterialInstance& operator=(const RuntimeMaterialInstance& other)
		{
			m_materialIdentifier = other.m_materialIdentifier;
			m_descriptorContents = other.m_descriptorContents;
			m_pushConstantData = other.m_pushConstantData;
			return *this;
		}
		RuntimeMaterialInstance& operator=(RuntimeMaterialInstance&&) = delete;

		[[nodiscard]] PushConstantsData::ConstViewType GetPushConstantsData() const
		{
			Assert(IsValid());
			return m_pushConstantData.GetPushConstantsData();
		}

		[[nodiscard]] PushConstantsData::ViewType GetPushConstantsData()
		{
			Assert(IsValid());
			return m_pushConstantData.GetPushConstantsData();
		}

		[[nodiscard]] const PushConstantsData& GetPushConstants() const
		{
			Assert(IsValid());
			return m_pushConstantData;
		}

		[[nodiscard]] MaterialIdentifier GetMaterialIdentifier() const
		{
			return m_materialIdentifier;
		}
		[[nodiscard]] MaterialInstanceIdentifier GetTemplateMaterialInstanceIdentifier() const
		{
			return m_templateMaterialInstanceIdentifier;
		}
		[[nodiscard]] bool IsClone() const
		{
			return m_flags.IsSet(Flags::IsClone);
		}

		[[nodiscard]] ArrayView<const DescriptorContent, uint8> GetDescriptorContents() const
		{
			Assert(IsValid());
			return m_descriptorContents;
		}

		[[nodiscard]] bool HasFinishedLoading() const
		{
			return m_flags.AreAnySet(Flags::HasFinishedLoading);
		}
		[[nodiscard]] bool IsValid() const
		{
			return m_flags.IsSet(Flags::WasLoaded) & m_materialIdentifier.IsValid();
		}

		void SwitchParentMaterial(const MaterialInstanceIdentifier identifier, const MaterialIdentifier newMaterialIdentifier);
		void UpdateDescriptorContent(const uint8 index, DescriptorContent&& newContent);

		Event<void(void*, const DescriptorContent& previousData, uint8), 24> OnDescriptorContentChanged;
		Event<void(void*), 24> OnParentMaterialChanged;

		bool Serialize(const Serialization::Reader serializer, const MaterialIdentifier materialIdentifier);
		bool Serialize(Serialization::Writer serializer) const;
	protected:
		void OnLoadingFinished();
		void OnLoadingFailed();
	protected:
		friend MaterialInstanceCache;

		MaterialIdentifier m_materialIdentifier;
		MaterialInstanceIdentifier m_templateMaterialInstanceIdentifier;

		// Must match MaterialDefinition::m_descriptorBindings in size
		DescriptorContents m_descriptorContents;
		PushConstantsData m_pushConstantData;

		AtomicEnumFlags<Flags> m_flags;
	};
}
