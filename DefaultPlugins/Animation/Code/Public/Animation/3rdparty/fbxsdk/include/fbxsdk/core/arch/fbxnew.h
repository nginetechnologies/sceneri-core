/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

/** \file fbxnew.h
  * New operator override templates.
  *
  * Instead of overloading the operator new in the FBX SDK, we provide a set of templates
  * that are used internally to create objects. This mechanic allows the FBX SDK to call
  * a different memory allocator.
  * \see FbxSetMallocHandler FbxSetCallocHandler FbxSetReallocHandler FbxSetFreeHandler FbxSetMSizeHandler
  */
#ifndef _FBXSDK_CORE_ARCH_NEW_H_
#define _FBXSDK_CORE_ARCH_NEW_H_

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_def.h>

#include <new>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsbegin.h>

//Type traits for primitive types
template<typename T> struct FbxSimpleType { enum {value = 0}; };
template<typename T> struct FbxSimpleType<T*> { enum {value = 1}; };
template<typename T> struct FbxSimpleType<const T> { enum {value = FbxSimpleType<T>::value}; };
template<typename T, size_t n> struct FbxSimpleType<T[n]> { enum {value = FbxSimpleType<T>::value}; };

#define FBXSDK_DEFINE_SIMPLE_TYPE(T) template<> struct FbxSimpleType<T>{ union {T t;} catcherr; enum {value = 1};}

FBXSDK_DEFINE_SIMPLE_TYPE(bool);
FBXSDK_DEFINE_SIMPLE_TYPE(char);
FBXSDK_DEFINE_SIMPLE_TYPE(unsigned char);
FBXSDK_DEFINE_SIMPLE_TYPE(short);
FBXSDK_DEFINE_SIMPLE_TYPE(unsigned short);
FBXSDK_DEFINE_SIMPLE_TYPE(int);
FBXSDK_DEFINE_SIMPLE_TYPE(unsigned int);
FBXSDK_DEFINE_SIMPLE_TYPE(long);
FBXSDK_DEFINE_SIMPLE_TYPE(unsigned long);
FBXSDK_DEFINE_SIMPLE_TYPE(float);
FBXSDK_DEFINE_SIMPLE_TYPE(double);
FBXSDK_DEFINE_SIMPLE_TYPE(long double);
FBXSDK_DEFINE_SIMPLE_TYPE(long long);
FBXSDK_DEFINE_SIMPLE_TYPE(unsigned long long);

#define FBXSDK_IS_SIMPLE_TYPE(T) ((bool)FbxSimpleType<T>::value)

template<typename T> T* FbxNew()
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T();
}

template<typename T, typename T1> T* FbxNew(T1& p1)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1);
}

template<typename T, typename T1> T* FbxNew(const T1& p1)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1);
}

template<typename T, typename T1, typename T2> T* FbxNew(T1& p1, T2& p2)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2);
}

template<typename T, typename T1, typename T2> T* FbxNew(T1& p1, const T2& p2)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2);
}

template<typename T, typename T1, typename T2> T* FbxNew(const T1& p1, T2& p2)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2);
}

template<typename T, typename T1, typename T2> T* FbxNew(const T1& p1, const T2& p2)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(T1& p1, T2& p2, T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(T1& p1, T2& p2, const T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(T1& p1, const T2& p2, T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(T1& p1, const T2& p2, const T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(const T1& p1, T2& p2, T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(const T1& p1, T2& p2, const T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(const T1& p1, const T2& p2, T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3> T* FbxNew(const T1& p1, const T2& p2, const T3& p3)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, T2& p2, T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, T2& p2, T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, T2& p2, const T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, T2& p2, const T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, const T2& p2, T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, const T2& p2, T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, const T2& p2, const T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(T1& p1, const T2& p2, const T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, T2& p2, T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, T2& p2, const T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, T2& p2, const T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, const T2& p2, T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1, p2, p3, p4);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(T1& p1, T2& p2, T3& p3, T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, T2& p2, const T3& p3, T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, T2& p2, T3& p3, const T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4, const T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, T3& p3, const T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4, const T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4, const T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5,p6);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5,p6,p7);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7, const T8& p8)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5,p6,p7,p8);
}

template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9> T* FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7, const T8& p8, const T9& p9)
{
	T* p = (T*)FbxMalloc(sizeof(T));
	return new(p)T(p1,p2,p3,p4,p5,p6,p7,p8,p9);
}

template<typename T> void FbxDelete(T* p)
{
	if( p )
	{
		((T*)p)->~T();
		FbxFree(p);
	}
}

template<typename T> void FbxDelete(const T* p)
{
	if( p )
	{
		((T*)p)->~T();
		FbxFree(const_cast<T*>(p));
	}
}

#ifdef FBXSDK_CPU_32
#define MALLOC_HEADER_SIZE 8
#endif
#ifdef FBXSDK_CPU_64
#define MALLOC_HEADER_SIZE 16
#endif

template<typename T> T* FbxNewArray(const int n)
{
	const size_t lSize = FbxAllocSize((size_t)n, sizeof(T));
	if( FBXSDK_IS_SIMPLE_TYPE(T) )
	{
		return (T*)FbxMalloc(lSize);
	}
	else
	{
		// malloc usually provides 8-byte or 16-byte alignment on 32bit and 64bit architectures
		// respectively. By allocating 8 or 16 bytes for the header info, rather than sizeof(int),
		// we ensure this function maintains the same alignment behaviour as malloc.
		void* const pTmp = FbxMalloc(lSize + MALLOC_HEADER_SIZE);
		*static_cast<int*>(pTmp) = n;
		T* const p = reinterpret_cast<T*>(static_cast<char*>(pTmp) + MALLOC_HEADER_SIZE);

		for( int i = 0; i < n; ++i )
		{
			new(p+i)T; // in-place new, not allocating memory so it is safe.
		}
		return p;
	}
}

template<typename T> void FbxDeleteArray(T* p)
{
	if( p )
	{
		if( !FBXSDK_IS_SIMPLE_TYPE(T) )
		{
// When compiling on MacOS with libstdc++ we cannot use remove_const (it does not exist - not C++11)
#ifndef USING_LIBSTDCPP
			typedef typename std::remove_const<T>::type TMutable;
			TMutable* const pMutable = const_cast<TMutable*>(p);
			// FbxNewArray allocates MALLOC_HEADER_SIZE extra bytes as a header to store the array length
			void* const pTmp = reinterpret_cast<char*>(pMutable) - MALLOC_HEADER_SIZE;
            const int n = *static_cast<int*>(pTmp);
#else
            void* const pTmp = (char*)(p) - MALLOC_HEADER_SIZE;
            const int n = *(int*)(pTmp);
#endif			
			for( int i = 0; i < n; ++i )
			{
				p[i].~T();
			}
			FbxFree(pTmp);
		}
		else
		{
			FbxFree((void*)p);
		}
	}
}

#define FBXSDK_FRIEND_NEW()\
	template<typename T>\
	friend T* FBXSDK_NAMESPACE::FbxNew();\
	template<typename T, typename T1>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1);\
	template<typename T, typename T1>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1);\
	template<typename T, typename T1, typename T2>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2);\
	template<typename T, typename T1, typename T2>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2);\
	template<typename T, typename T1, typename T2>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2);\
	template<typename T, typename T1, typename T2>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, const T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, const T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, const T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3);\
	template<typename T, typename T1, typename T2, typename T3>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3);\
    \
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, const T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, const T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, const T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, const T2& p2, const T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, const T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, const T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3, const T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4);\
	template<typename T, typename T1, typename T2, typename T3, typename T4>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4);\
    \
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(T1& p1, T2& p2, T3& p3, T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, const T3& p3, T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3, const T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, T2& p2, T3& p3, T4& p4, const T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3, const T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, T3& p3, T4& p4, const T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, T4& p4, const T5& p5);\
    template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5>\
    friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5);\
    \
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6);\
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7);\
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7, const T8& p8);\
	template<typename T, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>\
	friend T* FBXSDK_NAMESPACE::FbxNew(const T1& p1, const T2& p2, const T3& p3, const T4& p4, const T5& p5, const T6& p6, const T7& p7, const T8& p8, const T9& p9);\
	template<typename T>\
	friend void FBXSDK_NAMESPACE::FbxDelete(T* p);\
	template<typename T>\
	friend void FBXSDK_NAMESPACE::FbxDelete(const T* p);\
	template<typename T>\
	friend T* FBXSDK_NAMESPACE::FbxNewArray(const int n);\
	template<typename T>\
	friend void FBXSDK_NAMESPACE::FbxDeleteArray(T* p);

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_CORE_ARCH_NEW_H_ */
