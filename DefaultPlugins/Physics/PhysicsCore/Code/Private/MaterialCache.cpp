#include "MaterialCache.h"
#include "Material.h"
#include "MaterialAsset.h"
#include "MaterialAssetType.h"
#include "DefaultMaterials.h"

#include <Engine/Asset/AssetType.inl>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>

#include <Common/Asset/VirtualAsset.h>
#include <Common/Serialization/Reader.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/IO/Log.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	MaterialCache::MaterialCache(Asset::Manager& assetManager)
	{
		auto registerVirtualMaterial = [](
																		 MaterialCache& materialCache,
																		 Asset::Manager& assetManager,
																		 const Asset::Guid assetGuid,
																		 const ConstUnicodeStringView name,
																		 const float friction,
																		 const float restitution,
																		 const Math::Densityf density
																	 ) -> MaterialIdentifier
		{
			assetManager.RegisterAsset(
				assetGuid,
				Asset::DatabaseEntry{
					Physics::MaterialAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(
						assetGuid.ToString().GetView(),
						Physics::MaterialAssetType::AssetFormat.metadataFileExtension,
						Asset::VirtualAsset::FileExtension
					),
					UnicodeString(name)
				},
				Asset::Identifier{}
			);

			const MaterialIdentifier materialIdentifier = materialCache.BaseType::RegisterAsset(
				assetGuid,
				[friction, restitution, density](const MaterialIdentifier, const Guid assetGuid)
				{
					return UniquePtr<Material>::Make(assetGuid, friction, restitution, density);
				}
			);
			return materialIdentifier;
		};
		// TODO: Type safety for friction and restitution
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::WaterAssetGuid,
			MAKE_UNICODE_LITERAL("Water"),
			0.01f,
			0.f,
			1000_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::GrassAssetGuid,
			MAKE_UNICODE_LITERAL("Grass"),
			0.35f,
			0.1f,
			500_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::CorkAssetGuid, MAKE_UNICODE_LITERAL("Cork"), 0.2f, 0.6f, 240_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::SteelAssetGuid,
			MAKE_UNICODE_LITERAL("Steel"),
			0.4f,
			0.1f,
			7850_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PolishedSteelAssetGuid,
			MAKE_UNICODE_LITERAL("Polished Steel"),
			0.2f,
			0.05f,
			7850_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RustySteelAssetGuid,
			MAKE_UNICODE_LITERAL("Rusty Steel"),
			0.6f,
			0.1f,
			7850_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::AluminumAssetGuid,
			MAKE_UNICODE_LITERAL("Aluminum"),
			0.3f,
			0.3f,
			2700_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RubberHardAssetGuid,
			MAKE_UNICODE_LITERAL("Rubber (Hard)"),
			1.0f,
			0.4f,
			1500_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RubberSoftAssetGuid,
			MAKE_UNICODE_LITERAL("Rubber (Soft)"),
			0.8f,
			0.9f,
			1200_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RubberVulcanizedAssetGuid,
			MAKE_UNICODE_LITERAL("Rubber (Vulcanized)"),
			1.0f,
			0.7f,
			1230_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::TiresDryAssetGuid,
			MAKE_UNICODE_LITERAL("Rubber (Tires, Dry)"),
			0.9f,
			0.5f,
			1100_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::TiresWetAssetGuid,
			MAKE_UNICODE_LITERAL("Rubber (Tires, Wet)"),
			0.7f,
			0.4f,
			1100_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::WoodOakAssetGuid,
			MAKE_UNICODE_LITERAL("Wood (Oak)"),
			0.3f,
			0.5f,
			710_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PolishedWoodAssetGuid,
			MAKE_UNICODE_LITERAL("Polished Wood"),
			0.2f,
			0.3f,
			700_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RoughWoodAssetGuid,
			MAKE_UNICODE_LITERAL("Rough Wood"),
			0.4f,
			0.3f,
			700_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::RottenWoodAssetGuid,
			MAKE_UNICODE_LITERAL("Rotten Wood"),
			0.5f,
			0.2f,
			500_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::ConcreteAssetGuid,
			MAKE_UNICODE_LITERAL("Concrete"),
			0.5f,
			0.1f,
			2400_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::GlassAssetGuid,
			MAKE_UNICODE_LITERAL("Glass"),
			0.4f,
			0.5f,
			2500_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::IceAssetGuid, MAKE_UNICODE_LITERAL("Ice"), 0.02f, 0.1f, 917_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::IceSmoothAssetGuid,
			MAKE_UNICODE_LITERAL("Ice (Smooth)"),
			0.02f,
			0.05f,
			917_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PlasticAssetGuid,
			MAKE_UNICODE_LITERAL("Plastic"),
			0.35f,
			0.3f,
			950_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::LeatherThinAssetGuid,
			MAKE_UNICODE_LITERAL("Leather (Thin)"),
			0.4f,
			0.3f,
			860_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::LeatherThickAssetGuid,
			MAKE_UNICODE_LITERAL("Leather (Thick)"),
			0.8f,
			0.3f,
			900_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::AsphaltAssetGuid,
			MAKE_UNICODE_LITERAL("Asphalt"),
			0.6f,
			0.1f,
			2300_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::SandpaperAssetGuid,
			MAKE_UNICODE_LITERAL("Sandpaper"),
			1.f,
			0.05f,
			2600_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CarpetAssetGuid,
			MAKE_UNICODE_LITERAL("Carpet"),
			0.5f,
			0.2f,
			450_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::FoamAssetGuid, MAKE_UNICODE_LITERAL("Foam"), 0.2f, 0.7f, 50_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::BrickAssetGuid,
			MAKE_UNICODE_LITERAL("Brick"),
			0.5f,
			0.2f,
			1800_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::GraniteAssetGuid,
			MAKE_UNICODE_LITERAL("Granite"),
			0.4f,
			0.2f,
			2750_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::MarbleAssetGuid,
			MAKE_UNICODE_LITERAL("Marble"),
			0.5f,
			0.3f,
			2710_kilograms_cubed
		);
		;
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CeramicAssetGuid,
			MAKE_UNICODE_LITERAL("Ceramic"),
			0.5f,
			0.3f,
			2400_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::NylonAssetGuid,
			MAKE_UNICODE_LITERAL("Nylon"),
			0.25f,
			0.4f,
			1150_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CopperAssetGuid,
			MAKE_UNICODE_LITERAL("Copper"),
			0.3f,
			0.3f,
			8960_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::LeadAssetGuid,
			MAKE_UNICODE_LITERAL("Lead"),
			0.8f,
			0.05f,
			11340_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::GoldAssetGuid, MAKE_UNICODE_LITERAL("Gold"), 0.3f, 0.4f, 19320_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::SilverAssetGuid,
			MAKE_UNICODE_LITERAL("Silver"),
			0.5f,
			0.35f,
			10490_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::BronzeAssetGuid,
			MAKE_UNICODE_LITERAL("Bronze"),
			0.3f,
			0.25f,
			8900_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::BrassAssetGuid,
			MAKE_UNICODE_LITERAL("Brass"),
			0.3f,
			0.25f,
			8500_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::ZincAssetGuid, MAKE_UNICODE_LITERAL("Zinc"), 0.3f, 0.4f, 7140_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::TitaniumAssetGuid,
			MAKE_UNICODE_LITERAL("Titanium"),
			0.3f,
			0.3f,
			4500_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::GraphiteAssetGuid,
			MAKE_UNICODE_LITERAL("Graphite"),
			0.05f,
			0.05f,
			2260_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::PaperAssetGuid, MAKE_UNICODE_LITERAL("Paper"), 0.3f, 0.6f, 800_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CardboardAssetGuid,
			MAKE_UNICODE_LITERAL("Cardboard"),
			0.3f,
			0.5f,
			700_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::FiberglassAssetGuid,
			MAKE_UNICODE_LITERAL("Fiberglass"),
			0.35f,
			0.2f,
			2550_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CarbonFiberAssetGuid,
			MAKE_UNICODE_LITERAL("Carbon Fiber"),
			0.4f,
			0.1f,
			1600_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::StyrofoamAssetGuid,
			MAKE_UNICODE_LITERAL("Styrofoam"),
			0.4f,
			0.6f,
			35_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::WoolAssetGuid, MAKE_UNICODE_LITERAL("Wool"), 0.4f, 0.5f, 1300_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::VelvetAssetGuid,
			MAKE_UNICODE_LITERAL("Velvet"),
			0.7f,
			0.3f,
			500_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::DenimAssetGuid, MAKE_UNICODE_LITERAL("Denim"), 0.6f, 0.4f, 800_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CottonAssetGuid,
			MAKE_UNICODE_LITERAL("Cotton"),
			0.45f,
			0.5f,
			1540_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PolyesterFabricAssetGuid,
			MAKE_UNICODE_LITERAL("Polyester Fabric"),
			0.4f,
			0.3f,
			1400_kilograms_cubed
		);
		registerVirtualMaterial(*this, assetManager, Materials::SilkAssetGuid, MAKE_UNICODE_LITERAL("Silk"), 0.3f, 0.2f, 1300_kilograms_cubed);
		registerVirtualMaterial(*this, assetManager, Materials::TarAssetGuid, MAKE_UNICODE_LITERAL("Tar"), 0.5f, 0.05f, 1200_kilograms_cubed);
		registerVirtualMaterial(*this, assetManager, Materials::MudAssetGuid, MAKE_UNICODE_LITERAL("Mud"), 0.7f, 0.1f, 1500_kilograms_cubed);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::DirtWetAssetGuid,
			MAKE_UNICODE_LITERAL("Mud (Wet)"),
			0.3f,
			0.05f,
			1600_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::FreshSnowAssetGuid,
			MAKE_UNICODE_LITERAL("Snow (Fresh)"),
			0.2f,
			0.1f,
			150_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PackedSnowAssetGuid,
			MAKE_UNICODE_LITERAL("Snow (Packed)"),
			0.3f,
			0.1f,
			300_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::DrySandAssetGuid,
			MAKE_UNICODE_LITERAL("Sand (Dry)"),
			0.4f,
			0.1f,
			1600_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::WetSandAssetGuid,
			MAKE_UNICODE_LITERAL("Sand (Wet)"),
			0.3f,
			0.05f,
			2000_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::AsbestosAssetGuid,
			MAKE_UNICODE_LITERAL("Asbestos"),
			0.5f,
			0.1f,
			2500_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::SlateAssetGuid,
			MAKE_UNICODE_LITERAL("Slate"),
			0.4f,
			0.15f,
			2700_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::CeramicTileAssetGuid,
			MAKE_UNICODE_LITERAL("Ceramic Tile"),
			0.3f,
			0.05f,
			2400_kilograms_cubed
		);
		registerVirtualMaterial(
			*this,
			assetManager,
			Materials::PorcelainTileAssetGuid,
			MAKE_UNICODE_LITERAL("Porcelain Tile"),
			0.4f,
			0.05f,
			2400_kilograms_cubed
		);
		Assert(Materials::DefaultAssetGuid == Materials::GravelAssetGuid);
		m_defaultMaterialIdentifier = registerVirtualMaterial(
			*this,
			assetManager,
			Materials::GravelAssetGuid,
			MAKE_UNICODE_LITERAL("Gravel"),
			0.6f,
			0.05f,
			1600_kilograms_cubed
		);

		RegisterAssetModifiedCallback(assetManager);
	}

	MaterialCache::~MaterialCache() = default;

	MaterialIdentifier MaterialCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[&defaultMaterial = *FindMaterial(m_defaultMaterialIdentifier)](const MaterialIdentifier, const Asset::Guid)
			{
				return UniquePtr<Material>::Make(defaultMaterial);
			}
		);
	}

	MaterialIdentifier MaterialCache::RegisterAsset(const Asset::Guid guid)
	{
		const MaterialIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[&defaultMaterial = *FindMaterial(m_defaultMaterialIdentifier)](const MaterialIdentifier, const Guid)
			{
				return UniquePtr<Material>::Make(defaultMaterial);
			}
		);
		return identifier;
	}

	Optional<Material*> MaterialCache::FindMaterial(const MaterialIdentifier identifier) const
	{
		return &*BaseType::GetAssetData(identifier);
	}

	const Material& MaterialCache::GetDefaultMaterial() const
	{
		return *BaseType::GetAssetData(m_defaultMaterialIdentifier);
	}

	Threading::JobBatch MaterialCache::TryLoadMaterial(const MaterialIdentifier identifier, Asset::Manager& assetManager)
	{
		if (m_loadedMaterials.Set(identifier))
		{
			const Guid assetGuid = GetAssetGuid(identifier);
			Material& material = *GetAssetData(identifier);

			Threading::Job* pMaterialLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
				assetGuid,
				Threading::JobPriority::LoadMeshCollider,
				[&material, assetGuid](const ConstByteView data)
				{
					if (UNLIKELY(!data.HasElements()))
					{
						LogWarning("Physics Material data was empty when loading asset {}!", assetGuid.ToString());
						return;
					}

					Serialization::Data materialData(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
					);
					if (UNLIKELY(!materialData.IsValid()))
					{
						LogWarning("Physics Material data was invalid when loading asset {}!", assetGuid.ToString());
						return;
					}

					const Serialization::Reader materialReader(materialData);
					[[maybe_unused]] const bool wasLoaded = materialReader.SerializeInPlace(material);
					material.m_assetGuid = assetGuid;

					LogWarningIf(!wasLoaded, "Material asset {} couldn't be loaded!", assetGuid.ToString());
				}
			);

			if (pMaterialLoadJob != nullptr)
			{
				Threading::JobBatch jobBatch(Threading::JobBatch::IntermediateStage);
				jobBatch.QueueAfterStartStage(*pMaterialLoadJob);
				return jobBatch;
			}
		}
		return {};
	}

#if DEVELOPMENT_BUILD
	void MaterialCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid,
		[[maybe_unused]] const IdentifierType identifier,
		[[maybe_unused]] const IO::PathView filePath
	)
	{
		// TODO: Implement
	}
#endif
}
