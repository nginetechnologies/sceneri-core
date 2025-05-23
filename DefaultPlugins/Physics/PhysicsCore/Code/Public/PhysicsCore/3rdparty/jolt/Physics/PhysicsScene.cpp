// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/PhysicsScene.h>
#include <Physics/PhysicsSystem.h>
#include <Physics/Body/BodyLockMulti.h>
#include <ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(PhysicsScene)
{
	JPH_ADD_ATTRIBUTE(PhysicsScene, mBodies)
	JPH_ADD_ATTRIBUTE(PhysicsScene, mConstraints)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(PhysicsScene::ConnectedConstraint)
{
	JPH_ADD_ATTRIBUTE(PhysicsScene::ConnectedConstraint, mSettings)
	JPH_ADD_ATTRIBUTE(PhysicsScene::ConnectedConstraint, mBody1)
	JPH_ADD_ATTRIBUTE(PhysicsScene::ConnectedConstraint, mBody2)
}

void PhysicsScene::AddBody(const BodyCreationSettings &inBody)
{
	mBodies.push_back(inBody);
}

void PhysicsScene::AddConstraint(const TwoBodyConstraintSettings *inConstraint, uint32 inBody1, uint32 inBody2)
{
	mConstraints.emplace_back(inConstraint, inBody1, inBody2);
}

bool PhysicsScene::FixInvalidScales()
{
	const Vec3 unit_scale = Vec3::sReplicate(1.0f);

	bool success = true;
	for (BodyCreationSettings &b : mBodies)
	{
		// Test if there is an invalid scale in the shape hierarchy
		const Shape *shape = b.GetShape();
		if (!shape->IsValidScale(unit_scale))
		{
			// Fix it up
			Shape::ShapeResult result = shape->ScaleShape(unit_scale);
			if (result.IsValid())
				b.SetShape(result.Get());
			else
				success = false;
		}
	}
	return success;
}

bool PhysicsScene::CreateBodies(PhysicsSystem *inSystem) const
{
	BodyInterface &bi = inSystem->GetBodyInterface();

	// Create bodies
	BodyIDVector body_ids;
	body_ids.reserve(mBodies.size());
	for (const BodyCreationSettings &b : mBodies)
	{
		const JPH::BodyID bodyID = bi.AcquireBodyIdentifier();
		if(bodyID.IsInvalid())
			break;
		[[maybe_unused]] const Body& body = bi.CreateBody(bodyID, b);
	}

	// Batch add bodies
	BodyIDVector temp_body_ids = body_ids; // Body ID's get shuffled by AddBodiesPrepare
	BodyInterface::AddState add_state = bi.AddBodiesPrepare(temp_body_ids.data(), (int)temp_body_ids.size());
	bi.AddBodiesFinalize(temp_body_ids.data(), (int)temp_body_ids.size(), add_state, EActivation::Activate);

	// If not all bodies are created, creating constraints will be unreliable
	if (body_ids.size() != mBodies.size())
		return false;

	// Create constraints
	for (const ConnectedConstraint &cc : mConstraints)
	{
		BodyID body1_id = cc.mBody1 == cFixedToWorld? BodyID() : body_ids[cc.mBody1];
		BodyID body2_id = cc.mBody2 == cFixedToWorld? BodyID() : body_ids[cc.mBody2];
		Constraint *c = bi.CreateConstraint(cc.mSettings, body1_id, body2_id);
		inSystem->AddConstraint(c);
	}

	// Everything was created
	return true;
}

void PhysicsScene::SaveBinaryState(StreamOut &inStream, bool inSaveShapes, bool inSaveGroupFilter) const
{
	BodyCreationSettings::ShapeToIDMap shape_to_id;
	BodyCreationSettings::MaterialToIDMap material_to_id;
	BodyCreationSettings::GroupFilterToIDMap group_filter_to_id;

	// Save bodies
	inStream.Write((uint32)mBodies.size());
	for (const BodyCreationSettings &b : mBodies)
		b.SaveWithChildren(inStream, inSaveShapes? &shape_to_id : nullptr, inSaveShapes? &material_to_id : nullptr, inSaveGroupFilter? &group_filter_to_id : nullptr);

	// Save constraints
	inStream.Write((uint32)mConstraints.size());
	for (const ConnectedConstraint &cc : mConstraints)
	{
		cc.mSettings->SaveBinaryState(inStream);
		inStream.Write(cc.mBody1);
		inStream.Write(cc.mBody2);
	}
}

PhysicsScene::PhysicsSceneResult PhysicsScene::sRestoreFromBinaryState(StreamIn &inStream)
{
	PhysicsSceneResult result;

	// Create scene
	Ref<PhysicsScene> scene = new PhysicsScene();

	BodyCreationSettings::IDToShapeMap id_to_shape;
	BodyCreationSettings::IDToMaterialMap id_to_material;
	BodyCreationSettings::IDToGroupFilterMap id_to_group_filter;

	// Reserve some memory to avoid frequent reallocations
	id_to_shape.reserve(1024);
	id_to_material.reserve(128);
	id_to_group_filter.reserve(128);

	// Read bodies
	uint32 len = 0;
	inStream.Read(len);
	scene->mBodies.resize(len);
	for (BodyCreationSettings &b : scene->mBodies)
	{
		// Read creation settings
		BodyCreationSettings::BCSResult bcs_result = BodyCreationSettings::sRestoreWithChildren(inStream, id_to_shape, id_to_material, id_to_group_filter);
		if (bcs_result.HasError())
		{
			result.SetError(bcs_result.GetError());
			return result;
		}
		b = bcs_result.Get();
	}

	// Read constraints
	len = 0;
	inStream.Read(len);
	scene->mConstraints.resize(len);
	for (ConnectedConstraint &cc : scene->mConstraints)
	{
		ConstraintSettings::ConstraintResult c_result = ConstraintSettings::sRestoreFromBinaryState(inStream);
		if (c_result.HasError())
		{
			result.SetError(c_result.GetError());
			return result;
		}
		cc.mSettings = static_cast<const TwoBodyConstraintSettings *>(c_result.Get().GetPtr());
		inStream.Read(cc.mBody1);
		inStream.Read(cc.mBody2);
	}

	result.Set(scene);
	return result;
}

void PhysicsScene::FromPhysicsSystem(const PhysicsSystem *inSystem)
{
	// This map will track where each body went in mBodies
	using BodyIDToIdxMap = UnorderedMap<BodyID, uint32, BodyID::Hash>;
	BodyIDToIdxMap body_id_to_idx;

	// Map invalid ID
	body_id_to_idx[BodyID()] = cFixedToWorld;

	// Get all bodies
	BodyIDVector body_ids;
	inSystem->GetBodies(body_ids);

	// Loop over all bodies
	const BodyLockInterface &bli = inSystem->GetBodyLockInterface();
	for (const BodyID &id : body_ids)
	{
		BodyLockRead lock(bli, id);
		if (lock.Succeeded())
		{
			// Store location of body
			body_id_to_idx[id] = (uint32)mBodies.size();

			// Convert to body creation settings
			AddBody(lock.GetBody().GetBodyCreationSettings());
		}
	}

	// Loop over all constraints
	Constraints constraints = inSystem->GetConstraints();
	for (const Constraint *c : constraints)
		if (c->GetType() == EConstraintType::TwoBodyConstraint)
		{
			// Cast to two body constraint
			const TwoBodyConstraint *tbc = static_cast<const TwoBodyConstraint *>(c);

			// Find the body indices
			BodyIDToIdxMap::const_iterator b1 = body_id_to_idx.find(tbc->GetBody1()->GetID());
			BodyIDToIdxMap::const_iterator b2 = body_id_to_idx.find(tbc->GetBody2()->GetID());
			JPH_ASSERT(b1 != body_id_to_idx.end() && b2 != body_id_to_idx.end());

			// Create constraint settings and add the constraint
			Ref<ConstraintSettings> settings = c->GetConstraintSettings();
			AddConstraint(static_cast<const TwoBodyConstraintSettings *>(settings.GetPtr()), b1->second, b2->second);
		}
}

JPH_NAMESPACE_END
