// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/RTTI.h>

JPH_NAMESPACE_BEGIN

/// Helper functions to get the underlying RTTI type of a type (so e.g. Array<sometype> will return sometype)
template <class T>				
const RTTI *GetPrimitiveTypeOfType(T *)
{ 
	return GetRTTIOfType((T *)nullptr);
}

template <class T>				
const RTTI *GetPrimitiveTypeOfType(T **)
{ 
	return GetRTTIOfType((T *)nullptr);
}

template <class T>				
const RTTI *GetPrimitiveTypeOfType(Ref<T> *)
{ 
	return GetRTTIOfType((T *)nullptr);
}

template <class T>				
const RTTI *GetPrimitiveTypeOfType(RefConst<T> *)
{ 
	return GetRTTIOfType((T *)nullptr);
}

template <class T>				
const RTTI *GetPrimitiveTypeOfType(Array<T> *)
{ 
	return GetPrimitiveTypeOfType((T *)nullptr);
}

template <class T, uint N>				
const RTTI *GetPrimitiveTypeOfType(StaticArray<T, N> *)
{ 
	return GetPrimitiveTypeOfType((T *)nullptr);
}

template <class T, uint N>				
const RTTI *GetPrimitiveTypeOfType(T (*)[N])
{ 
	return GetPrimitiveTypeOfType((T *)nullptr);
}

JPH_NAMESPACE_END
