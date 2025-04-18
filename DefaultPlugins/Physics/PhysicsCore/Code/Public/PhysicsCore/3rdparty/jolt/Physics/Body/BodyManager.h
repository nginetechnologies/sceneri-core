// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Body/Body.h>
#include <Core/Mutex.h>
#include <Core/MutexArray.h>

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Memory/Containers/Array.h>

JPH_NAMESPACE_BEGIN

// Classes
class BodyCreationSettings;
class BodyActivationListener;
struct PhysicsSettings;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
class BodyDrawFilter;
#endif // JPH_DEBUG_RENDERER

/// Array of bodies
using BodyVector = Array<Body*>;

/// Array of body ID's
using BodyIDVector = Array<BodyID>;

using ConstBodyView = ngine::IdentifierArrayView<Body* const, BodyIdentifier>;
using BodyView = ngine::IdentifierArrayView<Body*, BodyIdentifier>;
using BodyMask = ngine::IdentifierMask<BodyIdentifier>;
using AtomicBodyMask = ngine::Threading::AtomicIdentifierMask<BodyIdentifier>;

/// Class that contains all bodies
class BodyManager : public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Destructor
	~BodyManager();

	/// Initialize the manager
	void Init(uint inNumBodyMutexes, const BroadPhaseLayerInterface& inLayerInterface);

	/// Gets the current amount of bodies that are in the body manager
	uint GetNumBodies() const;

	/// Gets the max bodies that we can support
	uint GetMaxBodies() const
	{
		return BodyIdentifier::MaximumCount;
	}

	/// Helper struct that counts the number of bodies of each type
	struct BodyStats
	{
		uint mNumBodies = 0; ///< Total number of bodies in the body manager
		uint mMaxBodies = 0; ///< Max allowed number of bodies in the body manager (as configured in Init(...))

		uint mNumBodiesStatic = 0; ///< Number of static bodies

		uint mNumBodiesDynamic = 0;       ///< Number of dynamic bodies
		uint mNumActiveBodiesDynamic = 0; ///< Number of dynamic bodies that are currently active

		uint mNumBodiesKinematic = 0;       ///< Number of kinematic bodies
		uint mNumActiveBodiesKinematic = 0; ///< Number of kinematic bodies that are currently active
	};

	/// Get stats about the bodies in the body manager (slow, iterates through all bodies)
	BodyStats GetBodyStats() const;

	[[nodiscard]] BodyID AcquireBodyIdentifier()
	{
		return m_identifierStorage.AcquireIdentifier();
	}

	void ReturnBodyIdentifier(const BodyID identifier)
	{
		m_identifierStorage.ReturnIdentifier(identifier);
	}

	[[nodiscard]] BodyID::IndexType GetMaximumUsedBodyCount() const
	{
		return m_identifierStorage.GetMaximumUsedElementCount();
	}

	/// Create a body.
	/// This is a thread safe function. Can return null if there are no more bodies available.
	Body& CreateBody(const BodyID identifier, const BodyCreationSettings& inBodyCreationSettings);

	/// Mark a list of bodies for destruction and remove it from this manager.
	/// This is a thread safe function since the body is not deleted until the next PhysicsSystem::Update() (which will take all locks)
	void DestroyBodies(const BodyID* inBodyIDs, int inNumber);

	/// Activate a list of bodies.
	/// This function should only be called when an exclusive lock for the bodies are held.
	void ActivateBodies(const BodyID* inBodyIDs, int inNumber);

	/// Deactivate a list of bodies.
	/// This function should only be called when an exclusive lock for the bodies are held.
	void DeactivateBodies(const BodyID* inBodyIDs, int inNumber);

	/// Get copy of the list of active bodies under protection of a lock.
	void GetActiveBodies(BodyIDVector& outBodyIDs) const;

	/// Get the list of active bodies. Note: Not thread safe. The active bodies list can change at any moment.
	const BodyID* GetActiveBodiesUnsafe() const
	{
		return m_activeBodies.GetData();
	}

	/// Get the number of active bodies.
	uint32 GetNumActiveBodies() const
	{
		return mNumActiveBodies;
	}

	/// Get the number of active bodies that are using continuous collision detection
	uint32 GetNumActiveCCDBodies() const
	{
		return mNumActiveCCDBodies;
	}

	/// Listener that is notified whenever a body is activated/deactivated
	void SetBodyActivationListener(BodyActivationListener* inListener);
	BodyActivationListener* GetBodyActivationListener() const
	{
		return mActivationListener;
	}

	/// Check if this is a valid body pointer. When a body is freed the memory that the pointer occupies is reused to store a freelist.
	static inline bool sIsValidBodyPointer(const Body* inBody)
	{
		return inBody != nullptr;
	}

	/// Get all bodies. Note that this can contain invalid body pointers, call sIsValidBodyPointer to check.
	ConstBodyView GetBodies() const
	{
		return m_identifierStorage.GetValidElementView(m_bodies.GetView());
	}

	/// Get all bodies. Note that this can contain invalid body pointers, call sIsValidBodyPointer to check.
	BodyView GetBodies()
	{
		return m_identifierStorage.GetValidElementView(m_bodies.GetView());
	}

	/// Get all body IDs under the protection of a lock
	void GetBodyIDs(BodyIDVector& outBodies) const;

	/// Access a body (not protected by lock)
	const Body& GetBody(const BodyID& inID) const
	{
		return *m_bodies[inID];
	}

	/// Access a body (not protected by lock)
	Body& GetBody(const BodyID& inID)
	{
		return *m_bodies[inID];
	}

	/// Access a body, will return a nullptr if the body ID is no longer valid (not protected by lock)
	const Body* TryGetBody(const BodyID& inID) const
	{
		const Body* body = m_bodies[inID];
		return sIsValidBodyPointer(body) && body->GetID() == inID ? body : nullptr;
	}

	/// Access a body, will return a nullptr if the body ID is no longer valid (not protected by lock)
	Body* TryGetBody(const BodyID& inID)
	{
		Body* body = m_bodies[inID];
		return sIsValidBodyPointer(body) && body->GetID() == inID ? body : nullptr;
	}

	/// Access the mutex for a single body
	SharedMutex& GetMutexForBody(const BodyID& inID) const
	{
		return mBodyMutexes.GetMutexByObjectIndex(inID.GetIndex());
	}

	/// Bodies are protected using an array of mutexes (so a fixed number, not 1 per body). Each bit in this mask indicates a locked mutex.
	using MutexMask = uint64;

	///@name Batch body mutex access (do not use directly)
	///@{
	MutexMask GetAllBodiesMutexMask() const
	{
		return mBodyMutexes.GetNumMutexes() == sizeof(MutexMask) * 8 ? ~MutexMask(0) : (MutexMask(1) << mBodyMutexes.GetNumMutexes()) - 1;
	}
	MutexMask GetMutexMask(const BodyID* inBodies, int inNumber) const;
	MutexMask GetMutexMask(const BodyMask& bodies) const;
	void LockRead(MutexMask inMutexMask) const;
	void UnlockRead(MutexMask inMutexMask) const;
	void LockWrite(MutexMask inMutexMask) const;
	void UnlockWrite(MutexMask inMutexMask) const;
	///@}

	/// Lock all bodies. This should only be done during PhysicsSystem::Update().
	void LockAllBodies() const;

	/// Unlock all bodies. This should only be done during PhysicsSystem::Update().
	void UnlockAllBodies() const;

	/// Function to update body's layer (should only be called by the BodyInterface since it also requires updating the broadphase)
	inline void SetBodyObjectLayerInternal(Body& ioBody, ObjectLayer inLayer) const
	{
		ioBody.mObjectLayer = inLayer;
		ioBody.mBroadPhaseLayer = mBroadPhaseLayerInterface->GetBroadPhaseLayer(inLayer);
	}

	/// Set the Body::EFlags::InvalidateContactCache flag for the specified body. This means that the collision cache is invalid for any body
	/// pair involving that body until the next physics step.
	void InvalidateContactCacheForBody(Body& ioBody);

	/// Reset the Body::EFlags::InvalidateContactCache flag for all bodies. All contact pairs in the contact cache will now by valid again.
	void ValidateContactCacheForAllBodies();

	/// Saving state for replay
	void SaveState(StateRecorder& inStream) const;

	/// Restoring state for replay. Returns false if failed.
	bool RestoreState(StateRecorder& inStream);

	enum class EShapeColor
	{
		InstanceColor,   ///< Random color per instance
		ShapeTypeColor,  ///< Convex = green, scaled = yellow, compound = orange, mesh = red
		MotionTypeColor, ///< Static = grey, keyframed = green, dynamic = random color per instance
		SleepColor,      ///< Static = grey, keyframed = green, dynamic = yellow, sleeping = red
		IslandColor,     ///< Static = grey, active = random color per island, sleeping = light grey
		MaterialColor,   ///< Color as defined by the PhysicsMaterial of the shape
	};

#ifdef JPH_DEBUG_RENDERER
	/// Draw settings
	struct DrawSettings
	{
		bool mDrawGetSupportFunction = false; ///< Draw the GetSupport() function, used for convex collision detection
		bool mDrawSupportDirection = false; ///< When drawing the support function, also draw which direction mapped to a specific support point
		bool mDrawGetSupportingFace = false; ///< Draw the faces that were found colliding during collision detection
		bool mDrawShape = true;              ///< Draw the shapes of all bodies
		bool mDrawShapeWireframe = false; ///< When mDrawShape is true and this is true, the shapes will be drawn in wireframe instead of solid.
		EShapeColor mDrawShapeColor = EShapeColor::MotionTypeColor; ///< Coloring scheme to use for shapes
		bool mDrawBoundingBox = false;                              ///< Draw a bounding box per body
		bool mDrawCenterOfMassTransform = false;                    ///< Draw the center of mass for each body
		bool mDrawWorldTransform = false; ///< Draw the world transform (which can be different than the center of mass) for each body
		bool mDrawVelocity = false;       ///< Draw the velocity vector for each body
		bool mDrawMassAndInertia = false; ///< Draw the mass and inertia (as the box equivalent) for each body
		bool mDrawSleepStats = false;     ///< Draw stats regarding the sleeping algorithm of each body
	};

	/// Draw the state of the bodies (debugging purposes)
	void Draw(
		const DrawSettings& inSettings,
		const PhysicsSettings& inPhysicsSettings,
		DebugRenderer* inRenderer,
		const BodyDrawFilter* inBodyFilter = nullptr
	);
#endif // JPH_DEBUG_RENDERER

#ifdef JPH_ENABLE_ASSERTS
	/// Lock the active body list, asserts when Activate/DeactivateBody is called.
	void SetActiveBodiesLocked(bool inLocked)
	{
		mActiveBodiesLocked = inLocked;
	}

	/// Per thread override of the locked state, to be used by the PhysicsSystem only!
	class GrantActiveBodiesAccess
	{
	public:
		inline GrantActiveBodiesAccess(bool inAllowActivation, bool inAllowDeactivation)
		{
			JPH_ASSERT(!sOverrideAllowActivation);
			sOverrideAllowActivation = inAllowActivation;

			JPH_ASSERT(!sOverrideAllowDeactivation);
			sOverrideAllowDeactivation = inAllowDeactivation;
		}

		inline ~GrantActiveBodiesAccess()
		{
			sOverrideAllowActivation = false;
			sOverrideAllowDeactivation = false;
		}
	};
#endif

#if DEBUG_BUILD
	/// Validate if the cached bounding boxes are correct for all active bodies
	void ValidateActiveBodyBounds();
#endif // DEBUG_BUILD
private:
	/// Increment and get the sequence number of the body
#ifdef JPH_COMPILER_CLANG
	__attribute__((no_sanitize("implicit-conversion"))) // We intentionally overflow the uint8 sequence number
#endif

	/// Helper function to delete a body (which could actually be a BodyWithMotionProperties)
	inline static void
	sDeleteBody(Body* inBody);

	/// Current number of allocated bodies
	atomic<BodyIdentifier::IndexType> mNumBodies = 0;

	ngine::TSaltedIdentifierStorage<BodyIdentifier> m_identifierStorage;

	ngine::TIdentifierArray<JPH::Body*, BodyIdentifier> m_bodies{ngine::Memory::Zeroed};

	AtomicBodyMask m_bodiesMask;

	/// Protects mBodies array (but not the bodies it points to), mNumBodies and mBodyIDFreeListStart
	mutable Mutex mBodiesMutex;

	/// An array of mutexes protecting the bodies in the mBodies array
	using BodyMutexes = MutexArray<SharedMutex>;
	mutable BodyMutexes mBodyMutexes;

	/// Mutex that protects the mActiveBodies array
	mutable Mutex mActiveBodiesMutex;

	/// List of all active dynamic bodies (size is equal to max amount of bodies)
	ngine::Array<BodyID, BodyIdentifier::MaximumCount> m_activeBodies;

	/// How many bodies there are in the list of active bodies
	atomic<BodyIdentifier::IndexType> mNumActiveBodies = 0;

	/// How many of the active bodies have continuous collision detection enabled
	uint32 mNumActiveCCDBodies = 0;

	/// Mutex that protects the mBodiesCacheInvalid array
	mutable Mutex mBodiesCacheInvalidMutex;

	/// List of all bodies that should have their cache invalidated
	BodyIDVector mBodiesCacheInvalid;

	/// Listener that is notified whenever a body is activated/deactivated
	BodyActivationListener* mActivationListener = nullptr;

	/// Cached broadphase layer interface
	const BroadPhaseLayerInterface* mBroadPhaseLayerInterface = nullptr;

#ifdef JPH_ENABLE_ASSERTS
	/// Debug system that tries to limit changes to active bodies during the PhysicsSystem::Update()
	bool mActiveBodiesLocked = false;
	static thread_local bool sOverrideAllowActivation;
	static thread_local bool sOverrideAllowDeactivation;
#endif
};

JPH_NAMESPACE_END
