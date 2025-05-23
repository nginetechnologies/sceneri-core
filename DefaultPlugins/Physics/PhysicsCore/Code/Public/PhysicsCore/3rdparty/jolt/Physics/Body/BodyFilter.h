// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/NonCopyable.h>
#include <Physics/Body/BodyID.h>

JPH_NAMESPACE_BEGIN

class Body;

/// Class function to filter out bodies, returns true if test should collide with body
class BodyFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~BodyFilter() = default;

	/// Filter function. Returns true if we should collide with inBodyID
	virtual bool			ShouldCollide(const BodyID &inBodyID) const
	{
		return true;
	}

	/// Filter function. Returns true if we should collide with inBody (this is called after the body is locked and makes it possible to filter based on body members)
	virtual bool			ShouldCollideLocked(const Body &inBody) const
	{
		return true;
	}
};

/// A simple body filter implementation that ignores a single, specified body
class IgnoreSingleBodyFilter : public BodyFilter
{
public:
	/// Constructor, pass the body you want to ignore
	explicit				IgnoreSingleBodyFilter(const BodyID &inBodyID) : 
		mBodyID(inBodyID)
	{
	}

	/// Filter function. Returns true if we should collide with inBodyID
	virtual bool			ShouldCollide(const BodyID &inBodyID) const override
	{
		return mBodyID != inBodyID;
	}
		
private:
	BodyID					mBodyID;
};

/// A simple body filter implementation that ignores multiple, specified bodies
class IgnoreMultipleBodiesFilter : public BodyFilter
{
public:
	/// Remove all bodies from the filter
	void					Clear()
	{
		mBodyIDs.clear();
	}

	/// Reserve space for inSize body ID's
	void					Reserve(uint inSize)
	{
		mBodyIDs.reserve(inSize);
	}

	/// Add a body to be ignored
	void					IgnoreBody(const BodyID &inBodyID)
	{
		mBodyIDs.push_back(inBodyID);
	}
		
	/// Filter function. Returns true if we should collide with inBodyID
	virtual bool			ShouldCollide(const BodyID &inBodyID) const override
	{
		return find(mBodyIDs.begin(), mBodyIDs.end(), inBodyID) == mBodyIDs.end();
	}

private:
	Array<BodyID>			mBodyIDs;
};

#ifdef JPH_DEBUG_RENDERER
/// Class function to filter out bodies for debug rendering, returns true if body should be rendered
class BodyDrawFilter : public NonCopyable
{
public:
	/// Destructor
	virtual					~BodyDrawFilter() = default;

	/// Filter function. Returns true if inBody should be rendered
	virtual bool			ShouldDraw(const Body& inBody) const
	{
		return true;
	}
};
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_END
