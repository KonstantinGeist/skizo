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

#ifndef FIELD_H_INCLUDED
#define FIELD_H_INCLUDED

#include "AccessModifier.h"
#include "ArrayList.h"
#include "Attribute.h"
#include "Member.h"
#include "MetadataSource.h"
#include "StringSlice.h"
#include "TypeRef.h"

namespace skizo { namespace script {

/**
 * Describes fields, both instance and static.
 */
class CField: public CMember
{
public:
    /**
     * Remembers where the field was declared for nicer errors.
     */
    SMetadataSource Source;

    CClass* DeclaringClass;
    SStringSlice Name;
    STypeRef Type;

    /**
     * @note Fields if declared in user code are currently always private, but they can be public if compiler-generated.
     */
    EAccessModifier Access;

    bool IsStatic;
    int Offset;

    skizo::core::Auto<skizo::collections::CArrayList<CAttribute*> > Attributes;

    CField();

    virtual EMemberKind MemberKind() const override;
};

} }

#endif // FIELD_H_INCLUDED
