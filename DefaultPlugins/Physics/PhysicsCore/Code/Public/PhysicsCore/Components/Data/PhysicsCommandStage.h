#pragma once

#include <PhysicsCore/Components/ColliderIdentifier.h>
#include <PhysicsCore/ConstraintIdentifier.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>
#include <3rdparty/jolt/Physics/Body/BodyCreationSettings.h>

#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/SixDOFConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Vehicle/VehicleConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/PointConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/SliderConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/DistanceConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/HingeConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/FixedConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/ConeConstraint.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/PathConstraint.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Math/Transform.h>

#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/FlatVector.h>

namespace JPH
{
	class MutableCompoundShape;
	class Character;

	class SixDOFConstraintSettings;
	class SwingTwistConstraintSettings;
	class PointConstraintSettings;
	class SliderConstraintSettings;
	class DistanceConstraintSettings;
	class HingeConstraintSettings;
	class FixedConstraintSettings;
	class ConeConstraintSettings;
	class PathConstraintSettings;
	class VehicleConstraintSettings;
}

namespace ngine::Physics
{
	struct BodyComponent;
	struct ColliderComponent;
	struct CharacterComponent;
	struct BodySettings;
}

namespace ngine::Physics::Data
{
	struct Scene;
	struct Body;

	struct PhysicsCommandStage final : public Threading::Job
	{
		PhysicsCommandStage(Scene& scene);

		void CreateBody(const JPH::BodyID bodyIdentifier, const JPH::BodyCreationSettings& bodyCreationSettings, uint64 pUserData);
		void CloneBody(
			const JPH::BodyID bodyIdentifier,
			const Scene& clonedBodyScene,
			const JPH::BodyID clonedBodyIdentifier,
			uint64 pUserData,
			uint64 pTemplateUserData
		);
		void AddBody(const JPH::BodyID bodyIdentifier);
		void RemoveBody(const JPH::BodyID bodyIdentifier);
		void DestroyBody(const JPH::BodyID bodyIdentifier);
		void SetBodyLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position);
		void SetBodyRotation(const JPH::BodyID bodyIdentifier, const JPH::Quat rotation);
		void SetBodyTransform(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position, const JPH::Quat rotation);
		void MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position);
		void MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Quat rotation);
		void MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position, const JPH::Quat rotation);
		void SetBodyVelocity(const JPH::BodyID bodyIdentifier, const JPH::Vec3 velocity);
		void SetBodyMotionType(const JPH::BodyID bodyIdentifier, const JPH::EMotionType motionType, const JPH::EActivation activationMode);
		void SetBodyLayer(const JPH::BodyID bodyIdentifier, const JPH::ObjectLayer objectLayer);

		void AddCollider(const JPH::BodyID bodyIdentifier, JPH::RefConst<JPH::Shape>&& pShape, const ColliderIdentifier colliderIdentifier);
		void ReplaceCollider(const JPH::BodyID bodyIdentifier, JPH::RefConst<JPH::Shape>&& pShape, const ColliderIdentifier colliderIdentifier);
		void RemoveCollider(const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier);
		void SetColliderTransform(
			const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier, const Math::WorldTransform transform
		);

		void AddImpulse(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse);
		void AddImpulseAtLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse, const JPH::Vec3 location);
		void AddForce(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse);
		void AddForceAtLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse, const JPH::Vec3 location);
		void AddTorque(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse);
		void AddAngularImpulse(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse);

		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SixDOFConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SwingTwistConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::PointConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SliderConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::DistanceConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::HingeConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::FixedConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::ConeConstraintSettings>&& settings
		);
		void AddConstraint(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::PathConstraintSettings>&& settings
		);
		void AddVehicleConstraint(
			const ConstraintIdentifier constraintIdentifier, const JPH::BodyID bodyIdentifier, JPH::Ref<JPH::VehicleConstraintSettings>&& settings
		);
		void RemoveConstraint(const ConstraintIdentifier constraintIdentifier);

		void WakeBodyFromSleep(const JPH::BodyID bodyIdentifier);
		void WakeAllBodiesFromSleep();
		void PutBodyToSleep(const JPH::BodyID bodyIdentifier);
		void PutAllBodiesToSleep();

		void FlushCommandQueue();

		enum class Flags : uint16
		{
			IsProcessingSleepQueue0 = 1 << 0,
			IsProcessingSleepQueue1 = 1 << 1,
			PutAllBodiesToSleep = 1 << 2,

			IsProcessingWakeQueue0 = 1 << 3,
			IsProcessingWakeQueue1 = 1 << 4,
			WakeAllBodiesFromSleep = 1 << 5,
		};
	protected:
		[[nodiscard]] bool ShouldQueueCommands();

		void CloneBodyInternal(
			JPH::BodyInterface& bodyInterface,
			const JPH::BodyID bodyIdentifier,
			const Scene& clonedBodyScene,
			const JPH::BodyID clonedBodyIdentifier,
			const uint64 pUserData,
			const uint64 pTemplateUserData
		);
		void AddColliderInternal(
			const JPH::BodyLockInterface& bodyLockInterface,
			const JPH::BodyID bodyIdentifier,
			JPH::RefConst<JPH::Shape>&& pShape,
			const ColliderIdentifier colliderIdentifier
		);
		void ReplaceColliderInternal(
			const JPH::BodyLockInterface& bodyLockInterface,
			const JPH::BodyID bodyIdentifier,
			JPH::RefConst<JPH::Shape>&& pShape,
			const ColliderIdentifier colliderIdentifier
		);
		void RemoveColliderInternal(
			const JPH::BodyLockInterface& bodyLockInterface, const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier
		);
		void SetColliderTransformInternal(
			const JPH::BodyLockInterface& bodyLockInterface,
			const JPH::BodyID bodyIdentifier,
			const ColliderIdentifier colliderIdentifier,
			const Math::WorldTransform worldTransform
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SixDOFConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SwingTwistConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::PointConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::SliderConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::DistanceConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::HingeConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::FixedConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::ConeConstraintSettings>&& settings
		);
		void AddConstraintInternal(
			const ConstraintIdentifier constraintIdentifier,
			const FixedArrayView<const JPH::BodyID, 2> bodies,
			JPH::Ref<JPH::PathConstraintSettings>&& settings
		);
		void AddVehicleConstraintInternal(
			const ConstraintIdentifier constraintIdentifier, const JPH::BodyID bodyIdentifier, JPH::Ref<JPH::VehicleConstraintSettings>&& settings
		);
		void UpdateBodyMotionProperties(Data::Body& bodyComponent, JPH::Body& body);

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Physics Command Stage";
		}
#endif
		virtual Result OnExecute(Threading::JobRunnerThread& thread) override;

		struct BodyCommandBase
		{
			JPH::BodyID m_bodyId;
		};
		struct CreateBodyCommand
		{
			JPH::BodyID m_bodyId;
			JPH::BodyCreationSettings m_creationSettings;
			uint64 m_pUserData;
		};
		struct CloneBodyCommand
		{
			JPH::BodyID m_bodyId;
			const Scene& m_clonedBodyScene;
			JPH::BodyID m_clonedBodyId;
			uint64 m_pUserData;
			uint64 m_pTemplateUserData;
		};
		struct DestroyBodyCommand : public BodyCommandBase
		{
		};
		struct AddBodyCommand : public BodyCommandBase
		{
		};
		struct RemoveBodyCommand : public BodyCommandBase
		{
		};
		struct AddColliderCommand
		{
			JPH::BodyID m_bodyId;
			JPH::RefConst<JPH::Shape> m_pShape;
			ColliderIdentifier m_colliderIdentifier;
		};
		struct ReplaceColliderCommand : public AddColliderCommand
		{
		};
		struct RemoveColliderCommand
		{
			JPH::BodyID m_bodyId;
			ColliderIdentifier m_colliderIdentifier;
		};
		struct SetColliderTransformCommand
		{
			JPH::BodyID m_bodyId;
			ColliderIdentifier m_colliderIdentifier;
			Math::WorldTransform m_newTransform;
		};
		struct SetBodyMotionTypeCommand
		{
			JPH::BodyID m_bodyId;
			JPH::EMotionType m_motionType;
			JPH::EActivation m_activationMode;
		};
		struct SetBodyLayerCommand
		{
			JPH::BodyID m_bodyId;
			JPH::ObjectLayer m_objectLayer;
		};
		struct SetBodyLocationCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Vec3 m_position;
		};
		struct SetBodyRotationCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Quat m_rotation;
		};
		struct SetBodyTransformCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Vec3 m_position;
			JPH::Quat m_rotation;
		};
		struct SetBodyVelocityCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Vec3 m_velocity;
		};
		struct MoveKinematicBodyLocationCommand : public SetBodyLocationCommand
		{
		};
		struct MoveKinematicBodyRotationCommand : public SetBodyRotationCommand
		{
		};
		struct MoveKinematicBodyTransformCommand : public SetBodyTransformCommand
		{
		};
		struct ConstraintCommand
		{
			ConstraintIdentifier m_constraintIdentifier;
			Array<JPH::BodyID, 2> m_bodies;
		};
		struct AddSixDegreesOfFreedomConstraintCommand : public ConstraintCommand
		{
			JPH::Ref<JPH::SixDOFConstraintSettings> m_settings;
		};
		struct AddSwingTwistConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::SwingTwistConstraintSettings> m_settings;
		};
		struct AddPointConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::PointConstraintSettings> m_settings;
		};
		struct AddSliderConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::SliderConstraintSettings> m_settings;
		};
		struct AddDistanceConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::DistanceConstraintSettings> m_settings;
		};
		struct AddHingeConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::HingeConstraintSettings> m_settings;
		};
		struct AddFixedConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::FixedConstraintSettings> m_settings;
		};
		struct AddConeConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::ConeConstraintSettings> m_settings;
		};
		struct AddPathConstraint : public ConstraintCommand
		{
			JPH::Ref<JPH::PathConstraintSettings> m_settings;
		};

		struct AddVehicleConstraintCommand
		{
			ConstraintIdentifier m_constraintIdentifier;
			JPH::BodyID m_bodyId;
			JPH::Ref<JPH::VehicleConstraintSettings> m_settings;
		};

		struct RemoveConstraintCommand
		{
			ConstraintIdentifier m_constraintIdentifier;
		};

		struct AddImpulseCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Vec3 m_impulse;
		};
		struct AddImpulseAtLocationCommand
		{
			JPH::BodyID m_bodyId;
			JPH::Vec3 m_impulse;
			JPH::Vec3 m_location;
		};
		struct AddForceCommand : public AddImpulseCommand
		{
		};
		struct AddForceAtLocationCommand : public AddImpulseAtLocationCommand
		{
		};
		struct AddTorqueCommand : public AddImpulseCommand
		{
		};
		struct AddAngularImpulseCommand : public AddImpulseCommand
		{
		};

		using Command = Variant<
			CreateBodyCommand,
			CloneBodyCommand,
			DestroyBodyCommand,
			AddBodyCommand,
			RemoveBodyCommand,
			SetBodyLocationCommand,
			SetBodyRotationCommand,
			SetBodyTransformCommand,
			MoveKinematicBodyLocationCommand,
			MoveKinematicBodyRotationCommand,
			MoveKinematicBodyTransformCommand,
			SetBodyVelocityCommand,
			SetBodyMotionTypeCommand,
			SetBodyLayerCommand,
			AddColliderCommand,
			ReplaceColliderCommand,
			RemoveColliderCommand,
			SetColliderTransformCommand,
			AddSixDegreesOfFreedomConstraintCommand,
			AddSwingTwistConstraint,
			AddPointConstraint,
			AddSliderConstraint,
			AddDistanceConstraint,
			AddHingeConstraint,
			AddFixedConstraint,
			AddConeConstraint,
			AddPathConstraint,
			AddVehicleConstraintCommand,
			RemoveConstraintCommand,
			AddImpulseCommand,
			AddImpulseAtLocationCommand,
			AddForceCommand,
			AddForceAtLocationCommand,
			AddTorqueCommand,
			AddAngularImpulseCommand>;

		template<typename Type>
		using CommandQueue = Vector<Type>;

		struct DoubleBufferedData
		{
			mutable Threading::Mutex m_sleepBodiesQueueMutex;
			CommandQueue<JPH::BodyID> m_sleepBodiesQueue;

			mutable Threading::Mutex m_wakeBodiesQueueMutex;
			CommandQueue<JPH::BodyID> m_wakeBodiesQueue;
		};

		void Execute(CommandQueue<Command>& queue, JPH::BodyInterface& bodyInterface, const JPH::BodyLockInterface& bodyLockInterface);
		void ProcessWakeBodiesFromSleep(DoubleBufferedData& queue, JPH::BodyInterface& bodyInterface);
		void ProcessPutBodiesToSleep(DoubleBufferedData& queue, JPH::BodyInterface& bodyInterface);

		bool IsBodyValid(const JPH::BodyID bodyIdentifier) const;

		template<typename... Commands>
		void QueueCommands(Commands&&... command);
	protected:
		AtomicEnumFlags<Flags> m_flags;

		Threading::Mutex m_flushQueueMutex;
		Array<DoubleBufferedData, 2> m_doubleBufferedData;

		mutable Threading::Mutex m_queuedCommandsMutex;
		CommandQueue<Command> m_queuedCommands;

		Scene& m_scene;
	};

	ENUM_FLAG_OPERATORS(PhysicsCommandStage::Flags);
}
