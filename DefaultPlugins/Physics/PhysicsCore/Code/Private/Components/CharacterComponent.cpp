#include "PhysicsCore/Components/CharacterComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"
#include "PhysicsCore/Layer.h"
#include "PhysicsCore/Material.h"
#include "PhysicsCore/MaterialAssetType.h"
#include <PhysicsCore/DefaultMaterials.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Asset/AssetManager.h>

#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/CollideShape.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Physics
{
	CharacterComponent::CharacterComponent(const CharacterComponent& templateComponent, const Cloner& cloner)
		: BodyComponent(templateComponent, cloner)
		, m_material(templateComponent.m_material)
	{
	}

	CharacterComponent::CharacterComponent(const Deserializer& deserializer)
		: CharacterComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<CharacterComponent>().ToString().GetView())
			)
	{
	}

	[[nodiscard]] inline const Material& FindOrCreatePhysicsMaterial(Asset::Guid guid)
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		MaterialIdentifier materialIdentifier = materialCache.FindOrRegisterAsset(guid);
		return *materialCache.FindMaterial(materialIdentifier);
	}

	CharacterComponent::CharacterComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: BodyComponent(deserializer)
		, m_material(FindOrCreatePhysicsMaterial(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Guid>("physical_material", Guid(Materials::DefaultAssetGuid))
																 : Guid(Materials::DefaultAssetGuid)
			))
	{
	}

	CharacterComponent::CharacterComponent(Initializer&& initializer)
		: BodyComponent(Forward<Initializer>(initializer))
		, m_material(
				initializer.m_pPhysicalMaterial.IsValid() ? *initializer.m_pPhysicalMaterial
																									: System::FindPlugin<Plugin>()->GetMaterialCache().GetDefaultMaterial()
			)
	{
	}

	CharacterComponent::~CharacterComponent()
	{
	}

	void CharacterComponent::OnCreated()
	{
		Entity::ComponentTypeSceneData<CharacterComponent>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<CharacterComponent>&>(*GetTypeSceneData());
		if (IsEnabled())
		{
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void CharacterComponent::OnEnable()
	{
		Entity::ComponentTypeSceneData<CharacterComponent>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<CharacterComponent>&>(*GetTypeSceneData());
		sceneData.EnableFixedPhysicsUpdate(*this);
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void CharacterComponent::OnDisable()
	{
		Entity::ComponentTypeSceneData<CharacterComponent>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<CharacterComponent>&>(*GetTypeSceneData());
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	inline static constexpr float MinimumTimeForStateChange = 0.1f;

	CharacterComponent::GroundState CharacterComponent::GetGroundState() const
	{
		if (m_timeInAir >= MinimumTimeForStateChange)
		{
			return GroundState::InAir;
		}
		else
		{
			if (m_groundState == GroundState::Sliding)
			{
				return GroundState::Sliding;
			}

			return GroundState::OnGround;
		}
	}

	Math::Vector3f CharacterComponent::GetGroundLocation() const
	{
		Assert(IsOnGround());
		const JPH::Vec3 location = mGroundPosition;
		return {location.GetX(), location.GetY(), location.GetZ()};
	}

	Math::Vector3f CharacterComponent::GetGroundNormal() const
	{
		Assert(IsOnGround());
		const JPH::Vec3 normal = mGroundNormal;
		return {normal.GetX(), normal.GetY(), normal.GetZ()};
	}

	Math::Vector3f CharacterComponent::GetGroundVelocity() const
	{
		Assert(IsOnGround());

		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const JPH::BodyInterface& bodyInterface = physicsScene.m_physicsSystem.GetBodyInterface();
		const JPH::Vec3 velocity = bodyInterface.GetPointVelocity(mGroundBodyID, mGroundPosition);

		return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
	}

	Optional<Entity::Component3D*> CharacterComponent::GetGroundComponent() const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const JPH::BodyInterface& bodyInterface = physicsScene.m_physicsSystem.GetBodyInterface();
		return reinterpret_cast<Entity::Component3D*>(bodyInterface.GetUserData(mGroundBodyID));
	}

	Math::WorldCoordinate CharacterComponent::GetFootLocation() const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const JPH::MutableCompoundShape& shape = static_cast<const JPH::MutableCompoundShape&>(*body.GetShape());
			return GetWorldLocation() - Math::WorldCoordinate{0, 0, (Math::WorldCoordinateUnitType)shape.GetLocalBounds().GetExtent().GetZ()};
		}
		return Math::Zero;
	}

	void CharacterComponent::Jump(const Math::Vector3f acceleration)
	{
		AddAcceleration(acceleration);
		// Immediately switch to the air state
		m_timeInAir = Math::Min(MinimumTimeForStateChange, m_timeInAir);
	}

	void CharacterComponent::FixedPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const JPH::Vec3 joltCurrentVelocity = body.GetLinearVelocity();

			Math::Vector3f currentVelocity{joltCurrentVelocity.GetX(), joltCurrentVelocity.GetY(), joltCurrentVelocity.GetZ()};
			Math::Vector3f desiredVelocity = m_velocity;
			desiredVelocity.z = currentVelocity.z;
			Math::Vector3f newVelocity;

			if (m_groundState == GroundState::InAir)
			{
				newVelocity = 0.75f * currentVelocity + 0.25f * desiredVelocity;
			}
			else
			{
				newVelocity = desiredVelocity;
			}

			newVelocity += m_requestedImpulse;

			m_requestedImpulse = Math::Zero;
			m_velocity = Math::Zero;
			body.SetLinearVelocity({newVelocity.x, newVelocity.y, newVelocity.z});

			if (!body.IsActive())
			{
				lock.ReleaseLock();
				physicsScene.m_physicsSystem.GetBodyInterfaceNoLock().ActivateBody(GetBodyID());
			}
		}
	}

	inline static constexpr float CharacterCollisionTolerance = 0.05f;

	void CharacterComponent::AfterPhysicsUpdate()
	{
		const FrameTime frameTime{GetCurrentFrameTime()};
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();
			const JPH::BodyInterface& bodyInterface = physicsScene.m_physicsSystem.GetBodyInterface();

			const JPH::Vec3 gravity = body.GetGravity(
				physicsScene.m_physicsSystem,
				bodyLockInterface,
				JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Gravity)),
				static_cast<JPH::ObjectLayer>(Layer::Gravity)
			);

			// Collector that finds the hit with the normal that is the most 'up'
			class MyCollector final : public JPH::CollideShapeCollector
			{
			public:
				// Constructor
				explicit MyCollector(JPH::Vec3Arg inGravity, const float maximumGroundAngle)
					: mGravity(inGravity)
					, m_maximumGroundAngle(maximumGroundAngle)
				{
				}

				// See: CollectorType::AddHit
				virtual void AddHit(const JPH::CollideShapeResult& inResult) override
				{
					JPH::Vec3 normal = -inResult.mPenetrationAxis.Normalized();
					float dot = normal.Dot(-mGravity.Normalized());
					if (dot < mBestDot && dot > m_maximumGroundAngle) // Find the hit that is most opposite to the gravity
					{
						mGroundBodyID = inResult.mBodyID2;
						mGroundBodySubShapeID = inResult.mSubShapeID2;
						mGroundPosition = inResult.mContactPointOn2;
						mGroundNormal = normal;
						mBestDot = dot;
					}

					// Remove velocity in direction of hits
					/*const Math::Vector3f vNormal{normal.GetX(), normal.GetY(), normal.GetZ()};
					m_velocity -= vNormal * m_velocity.Dot(vNormal);*/
				}

				JPH::BodyID mGroundBodyID;
				JPH::SubShapeID mGroundBodySubShapeID;
				JPH::Vec3 mGroundPosition = JPH::Vec3::sZero();
				JPH::Vec3 mGroundNormal = JPH::Vec3::sZero();
			private:
				float mBestDot = Math::NumericLimits<float>::Max;
				JPH::Vec3 mGravity;
				float m_maximumGroundAngle;
			};

			auto checkCollision = [&physicsScene,
			                       &bodyInterface,
			                       bodyID = body.GetID(),
			                       layer = body.GetObjectLayer()](const JPH::Shape& inShape, MyCollector& ioCollector)
			{
				// Create query broadphase layer filter
				JPH::DefaultBroadPhaseLayerFilter broadphase_layer_filter = physicsScene.m_physicsSystem.GetDefaultBroadPhaseLayerFilter(layer);

				// Create query object layer filter
				JPH::DefaultObjectLayerFilter object_layer_filter = physicsScene.m_physicsSystem.GetDefaultLayerFilter(layer);

				// Ignore my own body
				JPH::IgnoreSingleBodyFilter body_filter(bodyID);

				// Determine position to test
				JPH::Vec3 position;
				JPH::Quat rotation;
				bodyInterface.GetPositionAndRotation(bodyID, position, rotation);
				JPH::Mat44 query_transform = JPH::Mat44::sRotationTranslation(rotation, position + rotation * inShape.GetCenterOfMass());

				// Settings for collide shape
				JPH::CollideShapeSettings settings;
				settings.mMaxSeparationDistance = CharacterCollisionTolerance;
				settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;
				settings.mActiveEdgeMovementDirection = bodyInterface.GetLinearVelocity(bodyID);
				settings.mBackFaceMode = JPH::EBackFaceMode::IgnoreBackFaces;

				const JPH::NarrowPhaseQuery& narrowPhaseQuery = physicsScene.m_physicsSystem.GetNarrowPhaseQuery();
				narrowPhaseQuery.CollideShape(
					&inShape,
					JPH::Vec3::sReplicate(1.0f),
					query_transform,
					settings,
					ioCollector,
					broadphase_layer_filter,
					object_layer_filter,
					body_filter
				);
			};

			// Collide shape
			MyCollector collector(gravity, m_maximumGroundAngle.Cos().GetRadians());
			lock.ReleaseLock();
			checkCollision(*body.GetShape(), collector);

			// Copy results
			mGroundBodyID = collector.mGroundBodyID;
			mGroundPosition = collector.mGroundPosition;
			mGroundNormal = collector.mGroundNormal;
			mGroundMaterial = bodyInterface.GetMaterial(collector.mGroundBodyID, collector.mGroundBodySubShapeID);

			if (mGroundBodyID.IsInvalid())
			{
				m_groundState = GroundState::InAir;
			}
			else
			{
				JPH::Vec3 up = -gravity.Normalized();
				if (mGroundNormal.Dot(up) > m_maximumWalkableAngle.Cos().GetRadians())
				{
					m_groundState = GroundState::OnGround;
				}
				else
				{
					m_groundState = GroundState::Sliding;
				}
			}
		}

		switch (m_groundState)
		{
			case GroundState::OnGround:
			{
				m_timeInAir = Math::Min(m_timeInAir - frameTime, 0.f);
			}
			break;
			case GroundState::Sliding:
			{
				const JPH::Vec3 groundNormal = mGroundNormal;
				Math::Vector3f currentGroundNormal{groundNormal.GetX(), groundNormal.GetY(), groundNormal.GetZ()};
				currentGroundNormal.z = 0.f;

				if (currentGroundNormal.Dot(m_velocity) <= 0.f)
				{
					m_velocity = Math::Zero;
				}

				m_timeInAir = Math::Min(m_timeInAir - frameTime, 0.f);
			}
			break;
			case GroundState::InAir:
			{
				m_timeInAir = Math::Max(m_timeInAir + frameTime, 0.f);
			}
			break;
		}
	}

	void CharacterComponent::SetMaterialAsset(const PhysicalMaterialPicker asset)
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		MaterialIdentifier materialIdentifier = materialCache.FindOrRegisterAsset(asset.GetAssetGuid());
		Optional<Material*> pMaterial = materialCache.FindMaterial(materialIdentifier);
		if (LIKELY(pMaterial.IsValid()))
		{
			m_material = *pMaterial;
			Threading::JobBatch jobBatch = materialCache.TryLoadMaterial(materialIdentifier, System::Get<Asset::Manager>());
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadSkeleton);
				}
			}
		}
	}

	CharacterComponent::PhysicalMaterialPicker CharacterComponent::GetMaterialAsset() const
	{
		return {m_material->GetAssetGuid(), Physics::MaterialAssetType::AssetFormat.assetTypeGuid};
	}

	bool CharacterComponent::
		CanApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			return pAssetReference->GetTypeGuid() == Physics::MaterialAssetType::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool CharacterComponent::
		ApplyAtPoint(const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == Physics::MaterialAssetType::AssetFormat.assetTypeGuid)
			{
				SetMaterialAsset(*pAssetReference);
				return true;
			}
		}
		return false;
	}

	void CharacterComponent::IterateAttachedItems(
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

	[[maybe_unused]] const bool wasCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CharacterComponent>>::Make());
	[[maybe_unused]] const bool wasCharacterTypeRegistered = Reflection::Registry::RegisterType<CharacterComponent>();
}
