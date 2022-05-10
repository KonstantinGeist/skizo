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

#ifndef MEMBER_H_INCLUDED
#define MEMBER_H_INCLUDED

#include "Object.h"

namespace skizo { namespace script {

/**
 * Allowed members in a class.
 * @note Events and properties are expanded into fields and methods.
 */
enum EMemberKind
{
    E_RUNTIMEOBJECTKIND_FIELD,
    E_RUNTIMEOBJECTKIND_METHOD, // Includes constructors.
    E_RUNTIMEOBJECTKIND_CONST
};

class CMember: public skizo::core::CObject
{
public:
    // For bump pointer allocation.
    void* operator new(size_t size);
    void operator delete (void *p);

    // For faster type check.
    virtual EMemberKind MemberKind() const = 0;
};

} }

#endif // MEMBER_H_INCLUDED
