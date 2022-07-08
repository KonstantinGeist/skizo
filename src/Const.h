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

#ifndef CONST_H_INCLUDED
#define CONST_H_INCLUDED

#include "AccessModifier.h"
#include "Member.h"
#include "MetadataSource.h"
#include "StringSlice.h"
#include "TypeRef.h"
#include "Variant.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

/**
 * Represents a const value.
 */
class CConst: public CMember
{
public:
    /**
     * Remembers where the const was declared for nicer errors.
     */
    SMetadataSource Source;

    SStringSlice Name;
    STypeRef Type;
    EAccessModifier Access;

    class CClass* DeclaringClass;

    /**
     * @see SkizoMethodPrivate::m_declaringExtClass
     */
    class CClass* DeclaringExtClass;

    /** @note 
     * Null literals map to Variant::NOTHING.
     * Char literals map to Variant::INT (just like int literals).
     * String literals map to Variant::BLOB (because they're opaque pointers in the emitted code).
     */
    skizo::core::SVariant Value;

    CConst();

    virtual EMemberKind MemberKind() const override;

    /**
      * Checks if a const is accessible from a given method.
      * Consts have an exception for closures: closures are allowed to access private consts of enclosing
      * classes.
      */
    bool IsAccessibleFromMethod(const class CMethod* other) const;
};

} }

#endif // CONST_H_INCLUDED
