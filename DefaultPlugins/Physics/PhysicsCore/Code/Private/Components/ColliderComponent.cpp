#include "PhysicsCore/Components/ColliderComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/CharacterComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Material.h"
#include "PhysicsCore/MaterialAssetType.h"
#include "PhysicsCore/MaterialCache.h"
#include <PhysicsCore/DefaultMaterials.h>
#include "PhysicsCore/Plugin.h"

#include <Common/System/Query.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Asset/AssetManager.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Body/BodyCreationSettings.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>
#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ScaledShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ConvexShape.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Physics
{
	ColliderComponent::ColliderComponent(const ColliderComponent& templateComponent, ClonerInfo&& cloner)
		: Component3D(templateComponent, cloner)
		, m_material(templateComponent.m_material)
		, m_centerOfMassOffset(templateComponent.m_centerOfMassOffset)
		, m_pShape(WrapShape(Move(cloner.m_pShape)))
	{
		m_pShape->SetMaterial({}, &*m_material);
	}

	[[nodiscard]] inline const Material& GetOrLoadMaterial(Plugin& physicsManager, const Optional<Serialization::Reader> serializer)
	{
		Asset::Guid physicalMaterialGuid = serializer.IsValid()
		                                     ? serializer->ReadWithDefaultValue("physical_material", Asset::Guid(Materials::DefaultAssetGuid))
		                                     : Asset::Guid(Materials::DefaultAssetGuid);

		MaterialCache& materialCache = physicsManager.GetMaterialCache();
		MaterialIdentifier materialIdentifier = materialCache.FindOrRegisterAsset(physicalMaterialGuid);
		Optional<Material*> pMaterial = materialCache.FindMaterial(materialIdentifier);
		if (LIKELY(pMaterial.IsValid()))
		{
			Threading::JobBatch jobBatch = materialCache.TryLoadMaterial(materialIdentifier, System::Get<Asset::Manager>());
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadPhysicsMaterial);
				}
			}
			return *pMaterial;
		}

		return materialCache.GetDefaultMaterial();
	}

	ColliderComponent::ColliderComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_material(
				initializer.m_pPhysicalMaterial.IsValid() ? *initializer.m_pPhysicalMaterial
																									: System::FindPlugin<Plugin>()->GetMaterialCache().GetDefaultMaterial()
			)
		, m_pShape(WrapShape(Move(initializer.m_pShape)))
	{
		if (initializer.m_pTargetBody.IsInvalid() && !HasBody())
		{
			initializer.m_pTargetBody = CreateDataComponent<Data::Body>(
				initializer.GetSceneRegistry(),
				Data::Body::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, initializer.GetSceneRegistry()}}
			);
			Assert(initializer.m_pTargetBody.IsValid());
			initializer.m_pTargetBodyOwner = this;
		}

		Assert(m_pShape.IsValid());
		if (LIKELY(m_pShape.IsValid()))
		{
			m_pShape->SetMaterial({}, &*m_material);

			if (IsEnabled())
			{
				m_pAttachedBody = initializer.m_pTargetBody;
				m_pAttachedBodyOwner = initializer.m_pTargetBodyOwner;
			}
		}
	}

	ColliderComponent::ColliderComponent(DeserializerInfo&& deserializer)
		: ColliderComponent(
				Forward<DeserializerInfo>(deserializer),
				deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ColliderComponent>().ToString().GetView())
			)
	{
	}

	ColliderComponent::ColliderComponent(DeserializerInfo&& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: Component3D(deserializer)
		, m_material(GetOrLoadMaterial(*System::FindPlugin<Plugin>(), typeSerializer))
		, m_pShape(WrapShape(Move(deserializer.m_pShape)))
	{
		m_pShape->SetMaterial({}, &*m_material);
	}

	ColliderComponent::~ColliderComponent() = default;

	void ColliderComponent::OnCreated()
	{
		if (IsEnabled())
		{
			Optional<Data::Body*> pAttachedBody = m_pAttachedBody;
			Optional<Entity::Component3D*> pAttachedBodyOwner = m_pAttachedBodyOwner;

			if (pAttachedBody.IsInvalid())
			{
				if (DataComponentResult<Data::Body> result = GetNearestBody())
				{
					pAttachedBody = result.m_pDataComponent;
					pAttachedBodyOwner = result.m_pDataComponentOwner;
				}
			}

			// TODO: Store some reference to the targeted component in the serialized data
			// This way we can avoid traversing parents for it.
			if (pAttachedBody.IsValid())
			{
				if (LIKELY(pAttachedBody->AddCollider(*this)))
				{
					m_attachedBodyIdentifier = pAttachedBody->GetBodyIdentifier();
					m_pAttachedBody = pAttachedBody;
					m_pAttachedBodyOwner = pAttachedBodyOwner;
				}
			}
		}
	}

	void ColliderComponent::OnDestroying()
	{
		if (m_pAttachedBody.IsValid())
		{
			{
				Threading::SharedLock lock(m_pAttachedBody->m_colliderMutex);
				Assert(m_colliderIdentifier.IsValid());
				m_pAttachedBody->m_colliders[m_colliderIdentifier.GetFirstValidIndex()] = nullptr;
			}

			if (m_attachedBodyIdentifier.IsValid() & m_colliderIdentifier.IsValid())
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
				physicsScene.GetCommandStage().RemoveCollider(m_attachedBodyIdentifier, m_colliderIdentifier);
				m_attachedBodyIdentifier = JPH::BodyID{};
				m_colliderIdentifier = {};
				m_pAttachedBody = nullptr;
				m_pAttachedBodyOwner = nullptr;
			}
		}
	}

	void ColliderComponent::OnEnable()
	{
		Assert(m_pShape.IsValid());
		Assert(m_attachedBodyIdentifier.IsInvalid() && m_pAttachedBody.IsInvalid());
		if (LIKELY(m_pShape.IsValid()))
		{
			if (DataComponentResult<Data::Body> result = GetNearestBody())
			{
				if (LIKELY(result.m_pDataComponent->AddCollider(*this)))
				{
					m_attachedBodyIdentifier = result.m_pDataComponent->GetBodyIdentifier();
					m_pAttachedBody = result.m_pDataComponent;
					m_pAttachedBodyOwner = result.m_pDataComponentOwner;
				}
			}
		}
	}

	void ColliderComponent::OnDisable()
	{
		if (m_pAttachedBody.IsValid())
		{
			Threading::SharedLock lock(m_pAttachedBody->m_colliderMutex);
			Assert(m_colliderIdentifier.IsValid());
			m_pAttachedBody->m_colliders[m_colliderIdentifier.GetFirstValidIndex()] = nullptr;
		}

		if (m_attachedBodyIdentifier.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveCollider(m_attachedBodyIdentifier, m_colliderIdentifier);
			m_attachedBodyIdentifier = JPH::BodyID{};
			m_colliderIdentifier = {};
			m_pAttachedBody = nullptr;
			m_pAttachedBodyOwner = nullptr;
		}
	}

	void ColliderComponent::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		if (serializer.IsValid())
		{
			if (const Optional<Math::Massf> massOverride = serializer->Read<Math::Massf>("mass_override"))
			{
				m_massOrDensity = *massOverride;
				if (DataComponentResult<Data::Body> pBody = GetNearestBody())
				{
					Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
					pBody->RecalculateShapeMass(physicsScene);
				}
			}
			else if (const Optional<Math::Densityf> densityOverride = serializer->Read<Math::Densityf>("density_override"))
			{
				m_massOrDensity = *densityOverride;
				if (DataComponentResult<Data::Body> pBody = GetNearestBody())
				{
					Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
					pBody->RecalculateShapeMass(physicsScene);
				}
			}
		}
	}

	bool ColliderComponent::SerializeCustomData(Serialization::Writer serializer) const
	{
		bool wroteAny{false};
		wroteAny |= m_massOrDensity.Visit(
			[serializer](const Math::Massf mass) mutable
			{
				return serializer.Serialize("mass_override", mass);
			},
			[serializer](const Math::Densityf density) mutable
			{
				return serializer.Serialize("density_override", density);
			},
			[]()
			{
				return false;
			}
		);

		return wroteAny;
	}

	[[nodiscard]] JPH::ShapeRef ColliderComponent::WrapShape(JPH::ShapeRef&& pShapeIn)
	{
		Assert(pShapeIn.IsValid());
		if (LIKELY(pShapeIn.IsValid()))
		{
			Math::WorldScale worldScale = GetWorldScale();
			const Math::LocalTransform relativeTransform = GetRelativeTransform();
			const Math::Vector3f relativeLocation = relativeTransform.GetLocation() * worldScale;
			const Math::Quaternionf relativeRotation = relativeTransform.GetRotationQuaternion();

			if (pShapeIn->GetSubType() == JPH::EShapeSubType::Sphere)
			{
				worldScale = Math::WorldScale(Math::Max(worldScale.x, worldScale.y, worldScale.z));
			}

			JPH::OffsetCenterOfMassShapeSettings* comOffsetSettings = new JPH::OffsetCenterOfMassShapeSettings(m_centerOfMassOffset, pShapeIn);
			JPH::ScaledShapeSettings* scaleSettings = new JPH::ScaledShapeSettings(comOffsetSettings, {worldScale.x, worldScale.y, worldScale.z});
			JPH::RotatedTranslatedShapeSettings rotatedShapeSettings(
				{relativeLocation.x, relativeLocation.y, relativeLocation.z},
				{relativeRotation.x, relativeRotation.y, relativeRotation.z, relativeRotation.w},
				scaleSettings
			);
			return rotatedShapeSettings.Create().Get();
		}
		return {};
	}

	JPH::Shape* ColliderComponent::GetShape() const
	{
		if (m_pShape != nullptr)
		{
			JPH::Shape* actualShape = m_pShape;
			while (actualShape->GetType() == JPH::EShapeType::Decorated)
			{
				actualShape = const_cast<JPH::Shape*>(static_cast<JPH::DecoratedShape*>(actualShape)->GetInnerShape());
			}

			return actualShape;
		}
		return nullptr;
	}

	void ColliderComponent::SetShape(JPH::ShapeRef&& pShape)
	{
		JPH::ShapeRef pPreviousShape = m_pShape;
		Assert(pShape.GetPtr() != pPreviousShape);
		m_pShape = Forward<JPH::ShapeRef>(pShape);

		if (IsEnabled())
		{
			JPH::BodyID existingBodyIdentifier = m_attachedBodyIdentifier;
			if (m_pShape != nullptr)
			{
				m_pShape->SetMaterial({}, &*m_material);

				if (pPreviousShape != nullptr && existingBodyIdentifier.IsValid())
				{
					Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
					physicsScene.GetCommandStage().ReplaceCollider(existingBodyIdentifier, m_pShape, m_colliderIdentifier);
				}
				else if (DataComponentResult<Data::Body> result = GetNearestBody())
				{
					if (LIKELY(result.m_pDataComponent->AddCollider(*this)))
					{
						m_attachedBodyIdentifier = result.m_pDataComponent->GetBodyIdentifier();
						m_pAttachedBody = result.m_pDataComponent;
						m_pAttachedBodyOwner = result.m_pDataComponentOwner;
					}
				}
			}
			else if (existingBodyIdentifier.IsValid())
			{
				if (m_pAttachedBody.IsValid())
				{
					Threading::SharedLock lock(m_pAttachedBody->m_colliderMutex);
					Assert(m_colliderIdentifier.IsValid());
					m_pAttachedBody->m_colliders[m_colliderIdentifier.GetFirstValidIndex()] = nullptr;
				}

				Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				physicsScene.GetCommandStage().RemoveCollider(existingBodyIdentifier, m_colliderIdentifier);
				m_attachedBodyIdentifier = JPH::BodyID{};
				m_colliderIdentifier = {};
				m_pAttachedBody = nullptr;
				m_pAttachedBodyOwner = nullptr;
			}
		}
	}

	void ColliderComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (flags.IsSet(Entity::TransformChangeFlags::ChangedByPhysics))
		{
			return;
		}

		if (m_attachedBodyIdentifier.IsValid() & m_colliderIdentifier.IsValid() & m_pAttachedBody.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

			Math::WorldScale worldScale = GetWorldScale();
			const Math::WorldTransform bodyWorldTransform = m_pAttachedBodyOwner->GetWorldTransform();
			const Math::Vector3f relativeLocation = bodyWorldTransform.InverseTransformLocation(GetWorldLocation());
			const Math::Quaternionf rotation = bodyWorldTransform.GetRotationQuaternion().InverseTransformRotation(GetWorldRotation());

			if (GetShape()->GetSubType() == JPH::EShapeSubType::Sphere)
			{
				worldScale = Math::WorldScale(Math::Max(worldScale.x, worldScale.y, worldScale.z));
			}

			physicsScene.GetCommandStage()
				.SetColliderTransform(m_attachedBodyIdentifier, m_colliderIdentifier, Math::WorldTransform{rotation, relativeLocation, worldScale});
		}
	}

	ColliderComponent::DataComponentResult<Data::Body> ColliderComponent::GetNearestBody()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (Optional<Data::Body*> pBodyComponent = FindDataComponentOfType<Data::Body>(sceneRegistry))
		{
			return {pBodyComponent, this};
		}

		DataComponentResult<Data::Body> result{};
		HierarchyComponent::IterateDataComponentsOfTypeInParentsRecursive<Data::Body>(
			sceneRegistry,
			[&result](DataComponentResult<Data::Body> pair)
			{
				if (pair.IsValid())
				{
					if (pair.m_pDataComponentOwner->IsEnabled())
					{
						result = pair;
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		return result;
	}

	bool ColliderComponent::HasBody()
	{
		Optional<Component3D*> pComponent = this;
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		do
		{
			if (pComponent->HasDataComponentOfType<Data::Body>(sceneRegistry))
			{
				return true;
			}
			pComponent = pComponent->GetParentSafe();
		} while (pComponent.IsValid());

		return false;
	}

	void ColliderComponent::SetMaterial(const Asset::Picker asset)
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		MaterialIdentifier materialIdentifier = materialCache.FindOrRegisterAsset(asset.GetAssetGuid());
		Optional<Material*> pMaterial = materialCache.FindMaterial(materialIdentifier);
		if (LIKELY(pMaterial.IsValid()))
		{
			m_material = *pMaterial;
			// Remove any custom mass or density
			m_massOrDensity = {};

			m_pShape->SetMaterial({}, &*pMaterial);
			if (DataComponentResult<Data::Body> pBody = GetNearestBody())
			{
				Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				pBody->RecalculateShapeMass(physicsScene);
			}

			Threading::JobBatch jobBatch = materialCache.TryLoadMaterial(materialIdentifier, System::Get<Asset::Manager>());
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadPhysicsMaterial);
				}
			}
		}
	}

	Asset::Picker ColliderComponent::GetMaterialAsset() const
	{
		return {m_material->GetAssetGuid(), Physics::MaterialAssetType::AssetFormat.assetTypeGuid};
	}

	bool ColliderComponent::
		CanApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			return pAssetReference->GetTypeGuid() == Physics::MaterialAssetType::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool ColliderComponent::
		ApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == Physics::MaterialAssetType::AssetFormat.assetTypeGuid)
			{
				SetMaterial(*pAssetReference);
				return true;
			}
		}
		return false;
	}

	void ColliderComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		const Asset::Picker material = GetMaterialAsset();
		if (callback(material.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}
	}

	void ColliderComponent::OnAttachedToNewParent()
	{
		BaseType::OnAttachedToNewParent();

		Assert(m_attachedBodyIdentifier.IsInvalid() & m_pAttachedBody.IsInvalid());
		if (IsEnabled() && m_pShape.IsValid())
		{
			if (DataComponentResult<Data::Body> result = GetNearestBody())
			{
				if (LIKELY(result.m_pDataComponent->AddCollider(*this)))
				{
					m_attachedBodyIdentifier = result.m_pDataComponent->GetBodyIdentifier();
					m_pAttachedBody = result.m_pDataComponent;
					m_pAttachedBodyOwner = result.m_pDataComponentOwner;
				}
			}
		}
	}

	void ColliderComponent::OnBeforeDetachFromParent()
	{
		if (m_pAttachedBody.IsValid())
		{
			Threading::SharedLock lock(m_pAttachedBody->m_colliderMutex);
			Assert(m_colliderIdentifier.IsValid());
			m_pAttachedBody->m_colliders[m_colliderIdentifier.GetFirstValidIndex()] = nullptr;
		}

		if (m_attachedBodyIdentifier.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveCollider(m_attachedBodyIdentifier, m_colliderIdentifier);
			m_attachedBodyIdentifier = JPH::BodyID{};
			m_colliderIdentifier = {};
			m_pAttachedBody = nullptr;
			m_pAttachedBodyOwner = nullptr;
		}
	}

	void ColliderComponent::SetMassFromProperty(const Math::Massf mass)
	{
		m_massOrDensity = Math::Massf::FromGrams(Math::Max(mass.GetGrams(), 0.0001f));
		if (DataComponentResult<Data::Body> pBody = GetNearestBody())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			pBody->RecalculateShapeMass(physicsScene);
		}
	}
	Math::Massf ColliderComponent::GetMassFromProperty() const
	{
		return m_massOrDensity.Visit(
			[](const Math::Massf mass) -> Math::Massf
			{
				return mass;
			},
			[this](const Math::Densityf density) -> Math::Massf
			{
				if (m_pShape.IsValid())
				{
					return Math::Massf::FromKilograms(m_pShape->GetMassProperties(density.GetKilogramsCubed()).mMass);
				}
				else
				{
					return 0_kilograms;
				}
			},
			[this]() -> Math::Massf
			{
				if (m_pShape.IsValid())
				{
					return Math::Massf::FromKilograms(m_pShape->GetMassProperties().mMass);
				}
				else
				{
					return 0_kilograms;
				}
			}
		);
	}

	void ColliderComponent::SetDensityFromProperty(const Math::Densityf density)
	{
		m_massOrDensity = Math::Densityf::FromKilogramsCubed(Math::Max(density.GetKilogramsCubed(), 0.00001f));
		if (DataComponentResult<Data::Body> pBody = GetNearestBody())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			pBody->RecalculateShapeMass(physicsScene);
		}
	}
	Math::Densityf ColliderComponent::GetDensityFromProperty() const
	{
		return m_massOrDensity.Visit(
			[this](const Math::Massf mass) -> Math::Densityf
			{
				if (m_pShape.IsValid())
				{
					const float volume = m_pShape->GetVolume();
					return Math::Densityf::FromKilogramsCubed(mass.GetKilograms() / volume);
				}
				else
				{
					return 1000_kilograms_cubed;
				}
			},
			[](const Math::Densityf density) -> Math::Densityf
			{
				return density;
			},
			[this]() -> Math::Densityf
			{
				return m_material->GetDensity();
			}
		);
	}

	JPH::MassProperties ColliderComponent::GetMassProperties() const
	{
		return m_massOrDensity.Visit(
			[this](const Math::Massf mass) -> JPH::MassProperties
			{
				if (m_pShape.IsValid())
				{
					const float volume = m_pShape->GetVolume();
					const Math::Densityf density = Math::Densityf::FromKilogramsCubed(mass.GetKilograms() / volume);
					return m_pShape->GetMassProperties(density.GetKilogramsCubed());
				}
				else
				{
					return JPH::MassProperties{};
				}
			},
			[this](const Math::Densityf density) -> JPH::MassProperties
			{
				if (m_pShape.IsValid())
				{
					return m_pShape->GetMassProperties(density.GetKilogramsCubed());
				}
				else
				{
					return JPH::MassProperties{};
				}
			},
			[this]() -> JPH::MassProperties
			{
				if (m_pShape.IsValid())
				{
					return m_pShape->GetMassProperties();
				}
				else
				{
					return JPH::MassProperties{};
				}
			}
		);
	}
}
