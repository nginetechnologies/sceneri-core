#pragma once

#include "SceneFramegraph.h"

#include <Widgets/Documents/SceneDocument3D.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

namespace ngine
{
	struct ProjectInfo;
}

namespace ngine::App::UI::Document
{
	struct Scene final : public Widgets::Document::Scene3D
	{
		using BaseType = Widgets::Document::Scene3D;

		Scene(Initializer&& initializer);
		~Scene();

		void OnCreated();

		// Widget
		virtual void OnFramegraphInvalidated() override;
		virtual void BuildFramegraph(Rendering::FramegraphBuilder&) override;
		virtual void OnFramegraphBuilt() override;
		// ~Widget

		[[nodiscard]] Network::ClientIdentifier GetClientIdentifier() const
		{
			return m_clientIdentifier;
		}
	protected:
		virtual void OnSceneChanged() override;

		// Widgets::Document::Scene3D
		[[nodiscard]] virtual Asset::Reference GetDocumentAssetReference() const override;
		virtual ArrayView<const Guid, uint16> GetSupportedDocumentAssetTypeGuids() const override;

		[[nodiscard]] virtual Threading::JobBatch OpenDocumentAssetInternal(
			const Widgets::DocumentData& document,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags
		) override;
		virtual void CloseDocument() override;
		// ~Widgets::Document::Scene3D

		[[nodiscard]] Threading::JobBatch OpenProjectDocumentInternal(
			const Asset::Guid assetGuid,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags
		);
		[[nodiscard]] Threading::JobBatch
		OpenProjectDocumentInternal(ProjectInfo&& projectInfo, const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags);
	protected:
		SceneFramegraphBuilder m_framegraphBuilder;

		Network::ClientIdentifier m_clientIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<App::UI::Document::Scene>
	{
		inline static constexpr auto Type = Reflection::Reflect<App::UI::Document::Scene>(
			"{D105097C-7FA8-4387-A4C5-982F64F5A955}"_guid,
			MAKE_UNICODE_LITERAL("Scene Document Widget"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization
		);
	};
}
