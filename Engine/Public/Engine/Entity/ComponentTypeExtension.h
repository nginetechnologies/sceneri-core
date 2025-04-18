#pragma once

#include <Common/Asset/Guid.h>
#include <Common/EnumFlagOperators.h>
#include <Common/EnumFlags.h>
#include <Common/Guid.h>
#include <Common/Reflection/Extension.h>

namespace ngine::Entity
{
	enum class ComponentTypeFlags : uint8
	{
	};

	ENUM_FLAG_OPERATORS(ComponentTypeFlags);

	struct ComponentTypeExtension final : public Reflection::ExtensionInterface
	{
		inline static constexpr Guid TypeGuid = "{6939F95A-0B01-4594-A7B9-30331ECB4655}"_guid;

		constexpr ComponentTypeExtension(
			const ComponentTypeFlags typeFlags,
			const Asset::Guid iconAssetGuid,
			const Guid categoryGuid = {},
			const Asset::Guid assetOverrideGuid = {},
			const Guid assetTypeOverrideGuid = {}
		)
			: ExtensionInterface{TypeGuid}
			, m_typeFlags(typeFlags)
			, m_iconAssetGuid(iconAssetGuid)
			, m_categoryGuid(categoryGuid)
			, m_assetOverrideGuid(assetOverrideGuid)
			, m_assetTypeOverrideGuid(assetTypeOverrideGuid)
		{
		}

		constexpr ComponentTypeExtension(const Asset::Guid iconAssetGuid)
			: ExtensionInterface{TypeGuid}
			, m_iconAssetGuid(iconAssetGuid)
		{
		}

		EnumFlags<ComponentTypeFlags> m_typeFlags;
		Asset::Guid m_iconAssetGuid;
		Guid m_categoryGuid;
		Asset::Guid m_assetOverrideGuid;
		Guid m_assetTypeOverrideGuid;
	};
}

namespace ngine::Entity::ComponentCategory
{
	struct LightTag
	{
		static constexpr Guid TypeGuid = "1acb2307-b93b-4e66-b917-dd40212f0a98"_guid;
	};

	struct CameraTag
	{
		static constexpr Guid TypeGuid = "3ca2d65d-1e8b-472d-b419-34982867841f"_guid;
	};

	struct PrimitiveTag
	{
		static constexpr Guid TypeGuid = "9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid;
	};

	struct GravityTag
	{
		static constexpr Guid TypeGuid = "bffd01e3-e9f8-4f24-b1e2-077e6c6528f3"_guid;
	};

	struct PhysicsTag
	{
		static constexpr Guid TypeGuid = "5edc8044-ff05-4c39-b59a-29021095f002"_guid;
	};

	struct CharactersTag
	{
		static constexpr Guid TypeGuid = "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid;
	};

	struct SplineTag
	{
		static constexpr Guid TypeGuid = "5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid;
	};

	struct TextTag
	{
		static constexpr Guid TypeGuid = "cb5a6f40-b060-440e-b205-0f0942b3d93e"_guid;
	};

	struct VehicleTag
	{
		static constexpr Guid TypeGuid = "ed2cac99-a0bc-4793-bb03-f47dadecdcf9"_guid;
	};

	struct GameplayTag
	{
		static constexpr Guid TypeGuid = "ef85043b-303d-43ac-84da-8b920b61fb2b"_guid;
	};

	struct DataTag
	{
		static constexpr Guid TypeGuid = "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid;
	};

	struct AudioTag
	{
		static constexpr Guid TypeGuid = "93c90110-9374-420f-97f5-30b252e081a8"_guid;
	};

	struct AnimationTag
	{
		static constexpr Guid TypeGuid = "9D186C9A-3D74-4E92-A5DD-A0F16AC9C138"_guid;
	};

	struct WidgetTag
	{
		static constexpr Guid TypeGuid = "138411d8-ced1-431d-a5ef-ba44f2960dea"_guid;
	};

	struct NodeTag
	{
		static constexpr Guid TypeGuid = "56c32864-6c47-4335-93ba-34d67a2a93fb"_guid;
	};
	struct ControlNodeTag
	{
		static constexpr Guid TypeGuid = "bb2279c8-8d99-4bce-92d8-b744c2b0abab"_guid;
	};
	struct LogicNodeTag
	{
		static constexpr Guid TypeGuid = "7706497d-0774-4462-aad1-179569468fe5"_guid;
	};
	struct OperatorNodeTag
	{
		static constexpr Guid TypeGuid = "1d2ea054-03d5-49cd-b4f5-577d6ccb7984"_guid;
	};
	struct ConstantNodeTag
	{
		static constexpr Guid TypeGuid = "34b7e991-d4a2-44d1-8246-3b568561d888"_guid;
	};
	struct MathNodeTag
	{
		static constexpr Guid TypeGuid = "31859fc6-b6b8-4962-bd57-14451bd80445"_guid;
	};
}
