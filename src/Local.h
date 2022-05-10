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

#ifndef LOCAL_H_INCLUDED
#define LOCAL_H_INCLUDED

#include "MetadataSource.h"
#include "StringSlice.h"
#include "TypeRef.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

/**
 * Describes a local variable.
 */
class CLocal: public skizo::core::CObject
{
public:
    /**
     * Remembers where the local was declared for nicer errors.
     */
    SMetadataSource Source;

    SStringSlice Name;
    STypeRef Type;

    /**
     * Each local remembers the method it was declared in (for implementing closures).
     */
    class CMethod* DeclaringMethod;

    /**
     * Was this local 'captured', i.e. referenced from a nested method (closure)?
     */
    bool IsCaptured;

    CLocal();
    CLocal(const SStringSlice& name, const STypeRef& type, class CMethod* declaringMethod);
    CLocal* Clone() const;
};

// A param is hardly different from a local.
typedef CLocal CParam;

} }

#endif // LOCAL_H_INCLUDED
