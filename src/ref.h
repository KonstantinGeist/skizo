// *****************************************************************************
//
//  Copyright (c) Konstantin Geist. All rights reserved.
//
//  The use and distribution terms for this software are contained in the file
//  named License.txt, which can be found in the root of this distribution.
//  By using this software in any fashion, you are agreeing to be bound by the
//  terms of this license.
//
//  You must not remove this notice, or any other, from this software.
//
// *****************************************************************************

#ifndef SKIZO_REF_H_INCLUDED
#define SKIZO_REF_H_INCLUDED

#include "basedefs.h"
#include "Object.h"
#include "Variant.h"

namespace skizo { namespace collections {

// Special functions that can be used by containers.
// For CObjects, calls Ref() & Unref().
// For other types, a no-op.
//
// Supported primitive types: int, const char*, void*, CObject*, so_char16* & others.

    // **********
    //   Object
    // **********

bool SKIZO_EQUALS(const skizo::core::CObject* obj1, const skizo::core::CObject* obj2);

inline int SKIZO_HASHCODE(const skizo::core::CObject* obj)
{
    return obj->GetHashCode();
}

inline void SKIZO_REF(const skizo::core::CObject* obj)
{
    if(obj)
        obj->Ref();
}

inline void SKIZO_UNREF(const skizo::core::CObject* obj)
{
    if(obj)
        obj->Unref();
}

inline bool SKIZO_IS_NULL(const skizo::core::CObject* obj)
{
    return !obj;
}

    // *****
    //  int
    // *****

inline void SKIZO_REF(const int i) { }
inline void SKIZO_UNREF(const int i) { }

inline bool SKIZO_IS_NULL(const int i)
{
    return false;
}

inline bool SKIZO_EQUALS(const int a, const int b)
{
    return a == b;
}

inline int SKIZO_HASHCODE(const int i)
{
    return i;
}

    // ************
    //  so_uint32
    // ************

inline void SKIZO_REF(const so_uint32 i) { }
inline void SKIZO_UNREF(const so_uint32 i) { }

inline bool SKIZO_IS_NULL(const so_uint32 i)
{
    return false;
}

inline bool SKIZO_EQUALS(const so_uint32 a, const so_uint32 b)
{
    return a == b;
}

inline int SKIZO_HASHCODE(const so_uint32 i)
{
    return (int)i;
}

    // ************
    //  so_uint16
    // ************

inline void SKIZO_REF(const so_uint16 i) { }
inline void SKIZO_UNREF(const so_uint16 i) { }

inline bool SKIZO_IS_NULL(const so_uint16 i)
{
    return false;
}

inline bool SKIZO_EQUALS(const so_uint16 a, const so_uint16 b)
{
    return a == b;
}

inline int SKIZO_HASHCODE(const so_uint16 i)
{
    return (int)i;
}

    // *******
    //  void*
    // *******

inline void SKIZO_REF(const void* i) { }
inline void SKIZO_UNREF(const void* i) { }
inline bool SKIZO_EQUALS(const void* a, const void* b) { return a == b; }
inline bool SKIZO_IS_NULL(const void* i) { return !i; }

inline int SKIZO_HASHCODE(const void* i)
{
    // NOTE Shifts by 3 because pointers are often aligned and have 0's at the end;
    // focused on 64 bit platforms.
    return (size_t)(i) >> 3;
}

    // ***************
    //   const char*
    // ***************

inline void SKIZO_REF(const char* i) { }
inline void SKIZO_UNREF(const char* i) { }

bool SKIZO_EQUALS(const char* a, const char* b);
int SKIZO_HASHCODE(const char* i);

inline bool SKIZO_IS_NULL(const char* c)
{
    return !c;
}

    // **************
    //   so_char16*
    // **************

inline void SKIZO_REF(const so_char16* cs) { }
inline void SKIZO_UNREF(const so_char16* cs) { }

bool SKIZO_EQUALS(const so_char16* cs1, const so_char16* cs2);
int SKIZO_HASHCODE(const so_char16* cs);

inline bool SKIZO_IS_NULL(const so_char16* c)
{
    return !c;
}

    // ************
    //   SVariant
    // ************

inline void SKIZO_REF(const skizo::core::SVariant& v) { }
inline void SKIZO_UNREF(const skizo::core::SVariant& v) { }

inline bool SKIZO_EQUALS(const skizo::core::SVariant& v1, const skizo::core::SVariant& v2)
{
    return v1.Equals(v2);
}

inline int SKIZO_HASHCODE(const skizo::core::SVariant& v1)
{
    return v1.GetHashCode();
}

inline bool SKIZO_IS_NULL(const skizo::core::SVariant& v)
{
    return false;
}

    // ***********
    //   so_byte
    // ***********

inline void SKIZO_REF(const so_byte b) { }
inline void SKIZO_UNREF(const so_byte b) { }
inline bool SKIZO_EQUALS(const so_byte b1, const so_byte b2) { return b1 == b2; }
inline int SKIZO_HASHCODE(const so_byte b) { return b; }
inline bool SKIZO_IS_NULL(const so_byte b) { return false; }

    // *********
    //   float
    // *********

inline void SKIZO_REF(const float i) { }
inline void SKIZO_UNREF(const float i) { }

// NOTE Do not define SKIZO_EQUALS for floats, because using floats as dictionary
// keys is a bad idea.

inline int SKIZO_HASHCODE(const float i)
{
    int r;
    memcpy(&r, &i, sizeof(int) < sizeof(float)? sizeof(int): sizeof(float));
    return r;
}

inline bool SKIZO_IS_NULL(float i)
{
    return false;
}

    // ***********
    //   so_long
    // ***********

inline void SKIZO_REF(const so_long i) { }
inline void SKIZO_UNREF(const so_long i) { }
inline bool SKIZO_EQUALS(const so_long a, const so_long b) { return a == b; }
inline bool SKIZO_IS_NULL(const so_long i) { return false; }
inline int SKIZO_HASHCODE(const so_long i) { return (int)i; } // TODO ?

} }

#endif // SKIZO_REF_H_INCLUDED
