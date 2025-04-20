#include "Components/TextComponent.h"
#include "Font.h"
#include "Manager.h"
#include "Stages/DrawTextStage.h"

#include <Common/System/Query.h>
#include "Engine/Engine.h"
#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"

#include <Common/Memory/Move.h>
#include <Common/Memory/Forward.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>
#include <Renderer/Stages/MaterialsStage.h>
#include <Renderer/Renderer.h>

namespace ngine::Font
{
	TextComponent::TextComponent(const TextComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
		, m_text(templateComponent.m_text)
		, m_color(templateComponent.m_color)
		, m_fontSize(templateComponent.m_fontSize)
		, m_fontIdentifier(templateComponent.m_fontIdentifier)
	{
		CloneMaterialInstance();

		if (m_fontIdentifier.IsInvalid())
		{
			Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
			m_fontIdentifier = fontManager.GetCache().FindOrRegisterAsset("73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset);
		}
	}

	[[nodiscard]] Rendering::RenderItemStageMask GetDefaultStageMask(Rendering::StageCache& stageCache)
	{
		Rendering::RenderItemStageMask stageMask;
		const Rendering::SceneRenderStageIdentifier drawTextStageIdentifier = stageCache.FindOrRegisterAsset(DrawTextStage::TypeGuid);
		const Rendering::SceneRenderStageIdentifier materialsStageIdentifier =
			stageCache.FindOrRegisterAsset(Rendering::MaterialsStage::TypeGuid);
		stageMask.Set(drawTextStageIdentifier);
		stageMask.Set(materialsStageIdentifier);
		return stageMask;
	}

	TextComponent::TextComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{
					Initializer(initializer), GetDefaultStageMask(System::Get<Rendering::Renderer>().GetStageCache()) | initializer.m_stageMask
				},
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(Primitives::PlaneMeshAssetGuid),
				System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(
					"E656BCDE-78CE-461B-9CAB-3D17C41BB56D"_asset
				)
			})
		, m_text(Forward<UnicodeString>(initializer.m_text))
		, m_color(initializer.m_color)
		, m_fontSize(initializer.m_fontSize)
		, m_fontIdentifier(initializer.m_fontIdentifier)
	{
		CloneMaterialInstance();

		if (m_fontIdentifier.IsInvalid())
		{
			Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
			m_fontIdentifier = fontManager.GetCache().FindOrRegisterAsset("73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset);
		}
	}

	TextComponent::TextComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: StaticMeshComponent(deserializer)
		, m_text(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<UnicodeString>("text", MAKE_UNICODE_LITERAL("My Text"))
																			: MAKE_UNICODE_LITERAL("My Text")
			)
	{
		CloneMaterialInstance();

		if (m_fontIdentifier.IsInvalid())
		{
			Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
			m_fontIdentifier = fontManager.GetCache().FindOrRegisterAsset("73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset);
		}
	}

	TextComponent::TextComponent(const Deserializer& deserializer)
		: TextComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<TextComponent>().ToString().GetView()))
	{
	}

	TextComponent::~TextComponent()
	{
	}

	void TextComponent::SetFontAsset(const FontPicker asset)
	{
		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();
		SetFont(fontCache.FindOrRegisterAsset(asset.GetAssetGuid()));
	}

	TextComponent::FontPicker TextComponent::GetFont() const
	{
		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();
		return {fontCache.GetAssetGuid(m_fontIdentifier), Font::AssetFormat.assetTypeGuid};
	}

	void TextComponent::OnTextPropertiesChanged()
	{
		ResetStages(GetStageMask());
	}

	bool TextComponent::
		CanApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			return pAssetReference->GetTypeGuid() == Font::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool TextComponent::
		ApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == Font::AssetFormat.assetTypeGuid)
			{
				SetFontAsset(*pAssetReference);
				return true;
			}
		}
		return false;
	}

	void TextComponent::IterateAttachedItems(
		[[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes,
		const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		FontPicker font = GetFont();
		if (callback(font) == Memory::CallbackResult::Break)
		{
			return;
		}
	}

	[[maybe_unused]] const bool wasTextComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<TextComponent>>::Make());
	[[maybe_unused]] const bool wasTextComponentTypeRegistered = Reflection::Registry::RegisterType<TextComponent>();
}
