#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Reflection/Property.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Time/Duration.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/Primitives/WorldLine.h>
#include <Common/Math/Transform.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsSystem.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyManager.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyActivationListener.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/ContactListener.h>
#include <PhysicsCore/3rdparty/jolt/Physics/StateRecorderImpl.h>
#include <PhysicsCore/Allocator.h>
#include <PhysicsCore/Layer.h>
#include <PhysicsCore/BroadPhaseLayer.h>
#include <Physics/Body/BodyID.h>

#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/Containers/InlineVector.h>

#include <PhysicsCore/ConstraintIdentifier.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Memory/Containers/CircularBuffer.h>
#include <Common/Time/Timestamp.h>
#include <Common/Time/FrameTime.h>
#include <Common/AtomicEnumFlags.h>

namespace ngine
{
	struct Scene3D;
	struct FrameTime;
}

namespace ngine::Physics
{
	struct Plugin;
	struct BodyComponent;
	struct ColliderComponent;
	struct CharacterBase;
	struct CharacterComponent;
	struct RigidbodyCharacter;
	struct RotatingCharacter;
	struct HandleComponent;
	struct RopeBodyComponent;
	struct KinematicBodyComponent;
	struct BodySettings;
	struct Vehicle;
	struct DirectionalGravityComponent;
	struct SphericalGravityComponent;
	struct SplineGravityComponent;

	namespace Components
	{
		struct PlaneConstraint;
	}
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Physics::Data
{
	namespace Internal
	{
		struct StateRecorder final : public JPH::StateRecorder
		{
			using BaseType = JPH::StateRecorder;

			StateRecorder() = default;
			StateRecorder(StateRecorder&& other) = default;
			StateRecorder(const StateRecorder& other) = delete;
			StateRecorder& operator=(StateRecorder&&) = default;
			StateRecorder& operator=(const StateRecorder&) = delete;

			virtual void WriteBytes(const void* inData, size_t inNumBytes) override;
			virtual void ReadBytes(void* outData, size_t inNumBytes) override;
			virtual bool IsEOF() const override;
			virtual bool IsFailed() const override;

			void Rewind()
			{
				m_position = 0;
			}
		protected:
			Vector<ByteType, size> m_data;
			size m_position{0};
		};
	}

	struct PhysicsCommandStage;
	struct Body;

	enum class SceneFlags : uint8
	{
		//! All bodies will start in sleep state by default
		BodiesSleepByDefault = 1 << 0,
		//! Whether the simulation is currently being rolled back
		IsRollingBack = 1 << 1,
	};

	ENUM_FLAG_OPERATORS(SceneFlags);

	// BroadPhaseLayerInterface implementation
	// This defines a mapping between object and broadphase layers.
	class BroadPhaseLayerInterfaceImplementation final : public JPH::BroadPhaseLayerInterface
	{
	public:
		BroadPhaseLayerInterfaceImplementation()
		{
			m_objectToBroadPhase[static_cast<uint8>(Layer::Dynamic)] = JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Dynamic));
			m_objectToBroadPhase[static_cast<uint8>(Layer::Static)] = JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Static));
			m_objectToBroadPhase[static_cast<uint8>(Layer::Queries)] = JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Queries));
			m_objectToBroadPhase[static_cast<uint8>(Layer::Triggers)] = JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Triggers));
			m_objectToBroadPhase[static_cast<uint8>(Layer::Gravity)] = JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Gravity));
		}

		virtual uint32 GetNumBroadPhaseLayers() const override
		{
			return m_objectToBroadPhase.GetSize();
		}

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer inLayer) const override
		{
			return m_objectToBroadPhase[(uint8)inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
		{
			switch (static_cast<BroadPhaseLayer>(inLayer))
			{
				case BroadPhaseLayer::Static:
					return "Static";
				case BroadPhaseLayer::Dynamic:
					return "Dynamic";
				case BroadPhaseLayer::Queries:
					return "Queries";
				case BroadPhaseLayer::Triggers:
					return "Triggers";
			}
			ExpectUnreachable();
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
	private:
		Array<JPH::BroadPhaseLayer, (uint8)Layer::Count> m_objectToBroadPhase;
	};

	struct Scene final : public Entity::Data::Component3D, public JPH::BodyActivationListener, public JPH::ContactListener
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint8, 3, 3>;

		using Initializer = Entity::Data::Component3D::DynamicInitializer;

		Scene(Initializer&& initializer);
		Scene(const Deserializer& deserializer);
		Scene(const Scene& templateComponent, const Cloner&);
		virtual ~Scene();

		void OnEnable();
		void OnDisable();

		void MarkBodyForRollback(const JPH::BodyID bodyIdentifier)
		{
			m_rolledBackBodies.Set(bodyIdentifier);
		}
		[[nodiscard]] bool RollBackAndTick(const Time::Timestamp time);

		using RollbackVisitCallback = Function<bool(), 24>;
		//! Rolls back history to the specified time, invokes the callback with physics at that state and then reverts to the previous (latest)
		//! time
		[[nodiscard]] bool RollbackAndVisit(const Time::Timestamp time, RollbackVisitCallback&& callback);

		[[nodiscard]] bool IsRollingBack() const
		{
			return m_flags.IsSet(SceneFlags::IsRollingBack);
		}

		[[nodiscard]] static Scene& Get(ngine::Scene3D& scene);

		// BodyActivationListener
		virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
		virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
		// ~BodyActivationListener

		// ContactListener
		virtual JPH::ValidateResult
		OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::CollideShapeResult& inCollisionResult) override;
		virtual void OnContactAdded(
			const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings
		) override;
		virtual void OnContactPersisted(
			const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings
		) override;
		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
		// ~ContactListener

		[[nodiscard]] JPH::EActivation GetDefaultBodyWakeState();

		[[nodiscard]] Threading::Job& GetPhysicsSimulationStage()
		{
			return m_physicsSimulationStage;
		}

		[[nodiscard]] inline bool ShouldBodiesSleepByDefault()
		{
			return m_flags.IsSet(SceneFlags::BodiesSleepByDefault);
		}

		void EnableBodiesSleepByDefault()
		{
			m_flags |= SceneFlags::BodiesSleepByDefault;
		}

		void DisableBodiesSleepByDefault()
		{
			m_flags &= ~SceneFlags::BodiesSleepByDefault;
		}

		void PutAllBodiesToSleep();
		void WakeAllBodiesFromSleep();

		//! Wakes bodies that don't have the property 'Sleeping' set
		void WakeBodiesFromSleep();

		void AddDynamicBody(JPH::BodyID bodyIdentifier)
		{
			m_dynamicBodies.Set(bodyIdentifier);
		}
		void RemoveDynamicBody(JPH::BodyID bodyIdentifier)
		{
			m_dynamicBodies.Clear(bodyIdentifier);
		}

		[[nodiscard]] JPH::BodyID RegisterBody();
		void DeregisterBody(JPH::BodyID bodyIdentifier);

		[[nodiscard]] ConstraintIdentifier RegisterConstraint();
		[[nodiscard]] JPH::Ref<JPH::Constraint> GetConstraint(const ConstraintIdentifier identifier);

		[[nodiscard]] PhysicsCommandStage& GetCommandStage()
		{
			return *m_physicsCommandStage;
		}

		struct RayCastResult
		{
			RayCastResult(
				const Math::Ratiof fraction,
				const Math::WorldLine line,
				const Math::Vector3f normal,
				const Optional<Data::Body*> pBody,
				const Optional<Entity::Component3D*> pComponent,
				const Optional<ColliderComponent*> pCollider
			)
				: m_fraction(fraction)
				, m_contactPosition(line.GetStart() + (float)fraction * (line.GetEnd() - line.GetStart()))
				, m_contactNormal(normal)
				, m_pBody(pBody)
				, m_pComponent(pComponent)
				, m_pCollider(pCollider)
			{
			}

			[[nodiscard]] Math::Ratiof GetFraction() const
			{
				return m_fraction;
			}

			[[nodiscard]] const Math::WorldCoordinate& GetContactPosition() const
			{
				return m_contactPosition;
			}

			[[nodiscard]] const Math::Vector3f& GetContactNormal() const
			{
				return m_contactNormal;
			}

			[[nodiscard]] Optional<Data::Body*> GetCollidingBody() const
			{
				return m_pBody;
			}
			[[nodiscard]] Optional<Entity::Component3D*> GetCollidingComponent() const
			{
				return m_pComponent;
			}
			[[nodiscard]] Optional<ColliderComponent*> GetCollidingCollider() const
			{
				return m_pCollider;
			}
		private:
			const Math::Ratiof m_fraction;
			const Math::WorldCoordinate m_contactPosition;
			const Math::Vector3f m_contactNormal;
			Optional<Data::Body*> m_pBody;
			Optional<Entity::Component3D*> m_pComponent;
			Optional<ColliderComponent*> m_pCollider;
		};

		struct ShapeCastResult
		{
			ShapeCastResult(
				const Math::Vector3f penetrationAxis,
				const Math::Vector3f contactLocation,
				const float penetrationDepth,
				const Optional<Data::Body*> pBody,
				const Optional<Entity::Component3D*> pComponent,
				const Optional<ColliderComponent*> pCollider
			)
				: m_penetrationAxis(penetrationAxis)
				, m_contactLocation(contactLocation)
				, m_penetrationDepth(penetrationDepth)
				, m_pBody(pBody)
				, m_pComponent(pComponent)
				, m_pCollider(pCollider)
			{
			}

			[[nodiscard]] Math::WorldCoordinate GetContactLocation() const
			{
				return m_contactLocation;
			}
			[[nodiscard]] Math::Vector3f GetContactNormal() const
			{
				return m_penetrationAxis.GetNormalized();
			}
			[[nodiscard]] Math::Vector3f GetPenetrationAxis() const
			{
				return m_penetrationAxis;
			}
			[[nodiscard]] float GetPenetrationDepth() const
			{
				return m_penetrationDepth;
			}
			[[nodiscard]] Optional<Data::Body*> GetCollidingBody() const
			{
				return m_pBody;
			}
			[[nodiscard]] Optional<Entity::Component3D*> GetCollidingComponent() const
			{
				return m_pComponent;
			}
			[[nodiscard]] Optional<ColliderComponent*> GetCollidingCollider() const
			{
				return m_pCollider;
			}
		private:
			const Math::Vector3f m_penetrationAxis;
			const Math::WorldCoordinate m_contactLocation;
			const float m_penetrationDepth;
			Optional<Data::Body*> m_pBody;
			Optional<Entity::Component3D*> m_pComponent;
			Optional<ColliderComponent*> m_pCollider;
		};

		using ShapeCastResults = InlineVector<ShapeCastResult, 1>;

		[[nodiscard]] ShapeCastResults SphereCast(
			const Math::WorldCoordinate& origin,
			const Math::Vector3f direction,
			const Math::Radiusf radius,
			const EnumFlags<BroadPhaseLayerMask> broadphaseLayers = BroadPhaseLayerMask::Static | BroadPhaseLayerMask::Dynamic,
			const EnumFlags<LayerMask> objectLayers = LayerMask::Static | LayerMask::Dynamic,
			const ArrayView<const JPH::BodyID> bodiesFilter = {}
		);
		[[nodiscard]] ShapeCastResults BoxCast(
			const Math::WorldCoordinate& origin,
			const Math::Vector3f direction,
			const Math::Vector3f halfExtends,
			const EnumFlags<BroadPhaseLayerMask> broadphaseLayers = BroadPhaseLayerMask::Static | BroadPhaseLayerMask::Dynamic,
			const EnumFlags<LayerMask> objectLayers = LayerMask::Static | LayerMask::Dynamic,
			const ArrayView<const JPH::BodyID> bodiesFilter = {}
		);
		[[nodiscard]] RayCastResult RayCast(
			const Math::WorldLine worldLine,
			const EnumFlags<BroadPhaseLayerMask> broadphaseLayers = BroadPhaseLayerMask::Static | BroadPhaseLayerMask::Dynamic,
			const EnumFlags<LayerMask> objectLayers = LayerMask::Static | LayerMask::Dynamic,
			const ArrayView<const JPH::BodyID> bodiesFilter = {}
		) const;

		void SetUpdateRate(const Math::Frequencyd updateRate)
		{
			m_updateRate = updateRate;
		}
		[[nodiscard]] Math::Frequencyd GetUpdateRate() const
		{
			return m_updateRate;
		}

		[[nodiscard]] FrameTime GetDeltaTime() const
		{
			return FrameTime{m_updateRate.GetDuration()};
		}

		[[nodiscard]] Time::Timestamp GetTickTime() const
		{
			return m_nextTickTime;
		}
	protected:
		void Initialize(ParentType& parent);
		void Step();
		void StepInternal();
		[[nodiscard]] ShapeCastResults ShapeCast(
			const Math::WorldCoordinate& origin,
			const Math::Vector3f direction,
			JPH::Shape* pShape,
			const EnumFlags<BroadPhaseLayerMask> broadphaseLayers = BroadPhaseLayerMask::Static | BroadPhaseLayerMask::Dynamic,
			const EnumFlags<LayerMask> objectLayers = LayerMask::Static | LayerMask::Dynamic,
			const ArrayView<const JPH::BodyID> bodiesFilter = {}
		);
	protected:
		friend struct Reflection::ReflectedType<Scene>;
		friend Body;
		friend BodyComponent;
		friend ColliderComponent;
		friend CharacterComponent;
		friend CharacterBase;
		friend RigidbodyCharacter;
		friend RotatingCharacter;
		friend HandleComponent;
		friend RopeBodyComponent;
		friend PhysicsCommandStage;
		friend KinematicBodyComponent;
		friend Vehicle;
		friend DirectionalGravityComponent;
		friend SphericalGravityComponent;
		friend SplineGravityComponent;

		// Constraints
		friend Components::PlaneConstraint;
		// ~Constraints

		Plugin& m_plugin;
		ngine::Scene3D& m_engineScene;
		Entity::ComponentTypeSceneData<Data::Body>& m_bodyComponentTypeSceneData;
		UniqueRef<PhysicsCommandStage> m_physicsCommandStage;
		Threading::Job& m_physicsSimulationStage;

		BroadPhaseLayerInterfaceImplementation m_broadPhaseLayerInterface;

		JPH::PhysicsSystem m_physicsSystem;
		Physics::Internal::TempAllocator m_tempAllocator;
		JPH::BodyIDVector m_bodies;

		Threading::Mutex m_stepMutex;

		Math::Frequencyd m_updateRate{60_hz};

		Time::Timestamp m_nextTickTime;

		AtomicEnumFlags<SceneFlags> m_flags;

		mutable Threading::SharedMutex m_initializationMutex;

		TSaltedIdentifierStorage<ConstraintIdentifier> m_constraintIdentifiers;
		TIdentifierArray<JPH::Ref<JPH::Constraint>, ConstraintIdentifier> m_constraints{Memory::Zeroed};
		JPH::AtomicBodyMask m_dynamicBodies;

		struct HistoryEntry
		{
			Time::Timestamp timestamp;
			//!  TODO: Custom state recorder so we manage the way it allocates
			Internal::StateRecorder stateRecorder;
		};
		// Store 60 ticks in history, which assuming 60 tick rate means we have a window of 1s
		inline static constexpr uint8 HistorySize = 60;
		//! Store the state of the physical scene in history
		//! Useful for multiplayer on both server and client
		FixedCircularBuffer<HistoryEntry, HistorySize> m_history;

		Internal::StateRecorder m_rollbackStateRecorder;

		JPH::AtomicBodyMask m_rolledBackBodies;

		struct RolledBackState
		{
			JPH::BodyIdentifier bodyIdentifier;
			Math::Vector3f position;
			Math::Vector3f velocity;
			Math::Vector3f angularVelocity;
			Math::Quaternionf rotation;
		};
		Vector<RolledBackState> m_rolledBackBodyStates;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Data::Scene>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Data::Scene>(
			"{91FA295B-F982-495D-88D8-E34B9CBEEB7D}"_guid,
			MAKE_UNICODE_LITERAL("Scene Physics"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
