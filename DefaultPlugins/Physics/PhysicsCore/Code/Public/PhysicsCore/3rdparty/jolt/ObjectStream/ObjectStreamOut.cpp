// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <ObjectStream/ObjectStreamOut.h>
#include <ObjectStream/ObjectStreamTextOut.h>
#include <ObjectStream/ObjectStreamBinaryOut.h>
#include <ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

ObjectStreamOut::ObjectStreamOut(ostream &inStream) :
	mStream(inStream)
{
// Add all primitives to the class set
#define JPH_DECLARE_PRIMITIVE(name)	mClassSet.insert(JPH_RTTI(name));
#include <ObjectStream/ObjectStreamTypes.h>
}

ObjectStreamOut *ObjectStreamOut::Open(EStreamType inType, ostream &inStream)
{
	switch (inType) 
	{
	case EStreamType::Text:		return new ObjectStreamTextOut(inStream);
	case EStreamType::Binary:	return new ObjectStreamBinaryOut(inStream);
	default:					JPH_ASSERT(false);
	}
	return nullptr;
}

bool ObjectStreamOut::Write(const void *inObject, const RTTI *inRTTI)
{
	// Assign a new identifier to the object and write it
	mIdentifierMap.try_emplace(inObject, mNextIdentifier, inRTTI);
	mNextIdentifier++;
	WriteObject(inObject);

	// Write all linked objects
	while (!mObjectQueue.empty() && !mStream.fail()) 
	{
		const void *linked_object = mObjectQueue.front();
		WriteObject(linked_object);
		mObjectQueue.pop();
	}
	return !mStream.fail();
}

void ObjectStreamOut::WriteObject(const void *inObject)
{
	// Find object identifier
	IdentifierMap::iterator i = mIdentifierMap.find(inObject);
	JPH_ASSERT(i != mIdentifierMap.end());

	// Write class description and associated descriptions
	QueueRTTI(i->second.mRTTI);
	while (!mClassQueue.empty() && !mStream.fail()) 
	{
		WriteRTTI(mClassQueue.front());
		mClassQueue.pop();
	}

	HintNextItem();
	HintNextItem();

	// Write object header.
	WriteDataType(EOSDataType::Object);
	WriteName(i->second.mRTTI->GetName());
	WriteIdentifier(i->second.mIdentifier);

	// Write attribute data
	WriteClassData(i->second.mRTTI, inObject);
}

void ObjectStreamOut::QueueRTTI(const RTTI *inRTTI)
{
	ClassSet::const_iterator i = mClassSet.find(inRTTI);
	if (i == mClassSet.end()) 
	{
		mClassSet.insert(inRTTI);
		mClassQueue.push(inRTTI);
	}
}

void ObjectStreamOut::WriteRTTI(const RTTI *inRTTI)
{
	HintNextItem();
	HintNextItem();

	// Write class header. E.g. in text mode: "class <name> <attr-count>"
	WriteDataType(EOSDataType::Declare);
	WriteName(inRTTI->GetName());
	WriteCount(inRTTI->GetAttributeCount());

	// Write class attribute info
	HintIndentUp();
	for (int attr_index = 0; attr_index < inRTTI->GetAttributeCount(); ++attr_index) 
	{
		// Get attribute
		const SerializableAttribute &attr = inRTTI->GetAttribute(attr_index);

		// Write definition of attribute class if undefined
		const RTTI *rtti = attr.GetMemberPrimitiveType();
		if (rtti != nullptr)
			QueueRTTI(rtti);

		HintNextItem();

		// Write attribute information.
		WriteName(attr.GetName());
		attr.WriteDataType(*this);
	}
	HintIndentDown();
}

void ObjectStreamOut::WriteClassData(const RTTI *inRTTI, const void *inInstance)
{
	JPH_ASSERT(inInstance);

	// Write attributes
	HintIndentUp();
	for (int attr_index = 0; attr_index < inRTTI->GetAttributeCount(); ++attr_index) 
	{
		// Get attribute
		const SerializableAttribute &attr = inRTTI->GetAttribute(attr_index);
		attr.WriteData(*this, inInstance);
	}
	HintIndentDown();
}

void ObjectStreamOut::WritePointerData(const RTTI *inRTTI, const void *inPointer)
{
	Identifier identifier;

	if (inPointer) 
	{
		// Check if this object has an identifier
		IdentifierMap::iterator i = mIdentifierMap.find(inPointer);
		if (i != mIdentifierMap.end()) 
		{
			// Object already has an identifier
			identifier = i->second.mIdentifier;
		} 
		else 
		{
			// Assign a new identifier to this object and queue it for serialization
			identifier = mNextIdentifier++;
			mIdentifierMap.try_emplace(inPointer, identifier, inRTTI);
			mObjectQueue.push(inPointer);
		}
	} 
	else 
	{
		// Write nullptr pointer
		identifier = sNullIdentifier;
	}

	// Write the identifier
	HintNextItem();
	WriteIdentifier(identifier);
}

JPH_NAMESPACE_END
