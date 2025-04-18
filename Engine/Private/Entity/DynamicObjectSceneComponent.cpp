#include "Entity/DynamicObjectSceneComponent.h"

#include <Common/System/Query.h>

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobManager.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Reflection/DynamicTypeDefinition.h>
#include <Common/IO/Log.h>

namespace ngine::Entity
{
	DynamicObjectSceneComponent::DynamicObjectSceneComponent(const DynamicObjectSceneComponent& templateComponent, const Cloner& cloner)
		: SceneComponent(templateComponent, cloner)
		, m_pDynamicType(UniquePtr<Reflection::DynamicTypeDefinition>::Make(*templateComponent.m_pDynamicType))
		, m_dynamicObject(templateComponent.m_dynamicObject)
	{
	}

	DynamicObjectSceneComponent::DynamicObjectSceneComponent(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
	}

	DynamicObjectSceneComponent::DynamicObjectSceneComponent(Initializer&& initializer)
		: SceneComponent(Forward<Initializer>(initializer))
	{
	}

	DynamicObjectSceneComponent::~DynamicObjectSceneComponent() = default;

	void DynamicObjectSceneComponent::SetDynamicObjectType(const ObjectTypePicker asset)
	{
		// TODO: Destroy object
		Assert(!m_dynamicObject.IsValid());
		Assert(!m_pDynamicType.IsValid());

		Threading::Job* pJob = System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
			asset.GetAssetGuid(),
			Threading::JobPriority::LoadScriptObject,
			[this, asset](const ConstByteView data)
			{
				if (UNLIKELY(!data.HasElements()))
				{
					LogWarning("Asset data was empty when loading asset {0}!", asset.GetAssetGuid().ToString());
					return;
				}

				Serialization::Data typeSerializationData(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
				);
				if (LIKELY(typeSerializationData.IsValid()))
				{
					m_pDynamicType.CreateInPlace();
					const bool wasRead = Serialization::Deserialize(typeSerializationData, *m_pDynamicType);
					if (LIKELY(wasRead))
					{
						m_dynamicObject = Any(Memory::DefaultConstruct, *m_pDynamicType);
					}
				}
				else
				{
					LogWarning("Asset data was invalid when loading asset {0}!", asset.GetAssetGuid().ToString());
				}
			}
		);
		pJob->Queue(System::Get<Threading::JobManager>());
	}

	Threading::JobBatch DynamicObjectSceneComponent::SetDeserializedDynamicObjectType(
		const ObjectTypePicker asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		Assert(!m_dynamicObject.IsValid());
		Assert(!m_pDynamicType.IsValid());

		return System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
			asset.GetAssetGuid(),
			Threading::JobPriority::LoadScriptObject,
			[this, asset](const ConstByteView data)
			{
				if (UNLIKELY(!data.HasElements()))
				{
					LogWarning("Asset data was empty when loading asset {0}!", asset.GetAssetGuid().ToString());
					return;
				}

				Serialization::Data typeSerializationData(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
				);
				if (UNLIKELY(!typeSerializationData.IsValid()))
				{
					LogWarning("Asset data was invalid when loading asset {0}!", asset.GetAssetGuid().ToString());
					return;
				}

				m_pDynamicType.CreateInPlace();
				if (Serialization::Deserialize(typeSerializationData, *m_pDynamicType))
				{
					m_dynamicObject = Any(Memory::DefaultConstruct, *m_pDynamicType);
				}
				else
				{
					LogWarning("Failed to read dynamic type when loading asset {0}!", asset.GetAssetGuid().ToString());
				}
			}
		);
	}

	DynamicObjectSceneComponent::ObjectTypePicker DynamicObjectSceneComponent::GetDynamicObjectType() const
	{
		if (m_pDynamicType != nullptr)
		{
			return ObjectTypePicker{m_pDynamicType->GetGuid(), Reflection::DynamicTypeDefinition::AssetFormat.assetTypeGuid};
		}
		return Asset::Types{Reflection::DynamicTypeDefinition::AssetFormat.assetTypeGuid};
	}

	bool DynamicObjectSceneComponent::CanApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	) const
	{
		if (SceneComponent::CanApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			return pAssetReference->GetTypeGuid() == Reflection::DynamicTypeDefinition::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool DynamicObjectSceneComponent::ApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	)
	{
		if (SceneComponent::ApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == Reflection::DynamicTypeDefinition::AssetFormat.assetTypeGuid)
			{
				SetDynamicObjectType(*pAssetReference);
				return true;
			}
		}

		return false;
	}

	void DynamicObjectSceneComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Picker mesh = GetDynamicObjectType();
		if (callback(mesh.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}

		SceneComponent::IterateAttachedItems(allowedTypes, callback);
	}

	[[maybe_unused]] const bool wasDynamicObjectRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<DynamicObjectSceneComponent>>::Make());
	[[maybe_unused]] const bool wasDynamicObjectTypeRegistered = Reflection::Registry::RegisterType<DynamicObjectSceneComponent>();
}
