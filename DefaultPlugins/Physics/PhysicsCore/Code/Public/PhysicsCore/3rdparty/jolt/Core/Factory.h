// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/RTTI.h>
#include <Core/UnorderedMap.h>

JPH_NAMESPACE_BEGIN

/// Factory, to create RTTI objects
class Factory
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Create an object
	void *						CreateObject(const char *inName);

	/// Find type info for a specific class by name
	const RTTI *				Find(const char *inName);

	/// Find type info for a specific class by hash
	const RTTI *				Find(uint32 inHash);

	/// Register an object with the factory. Returns false on failure.
	bool						Register(const RTTI *inRTTI);

	/// Register a list of objects with the factory. Returns false on failure.
	bool						Register(const RTTI **inRTTIs, uint inNumber);

	/// Unregisters all types
	void						Clear();

	/// Get all registered classes
	Array<const RTTI *>			GetAllClasses() const;

	/// Singleton factory instance
	static Factory *			sInstance;

private:
	using ClassNameMap = UnorderedMap<string_view, const RTTI *>;

	using ClassHashMap = UnorderedMap<uint32, const RTTI *>;

	/// Map of class names to type info
	ClassNameMap				mClassNameMap;

	// Map of class hash to type info
	ClassHashMap				mClassHashMap;
};

JPH_NAMESPACE_END
