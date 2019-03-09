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

#include "../RuntimeHelpers.h"
#include "../Class.h"
#include "../Domain.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

extern "C" {

// NOTE copypaste from _so_Type_getAttributeImpl; TODO?
void* SKIZO_API _so_Method_getAttributeImpl(void* methodHandle, void* pAttrName)
{
    SKIZO_NULL_CHECK(pAttrName);

    CDomain* domain = CDomain::ForCurrentThread();
    const CMethod* pMethod = ((CMethod*)methodHandle);
    const CString* attrName = so_string_of(pAttrName);

    const CArrayList<CAttribute*>* attrs = pMethod->Attributes();
    if(attrs) {
        for(int i = 0; i < attrs->Count(); i++) {
            const CAttribute* attr = attrs->Array()[i];

            if(attr->Name.Equals(attrName)) {
                Auto<const CString> value (attr->Value.ToString());
                return domain->CreateString(value, false);
            }
        }
    }

    return nullptr;
}

void* SKIZO_API _so_Method_nameImpl(void* methodHandle)
{
    const CMethod* pMethod = ((CMethod*)methodHandle);
    Auto<const CString> name (pMethod->Name().ToString());
    return CDomain::ForCurrentThread()->CreateString(name, true);
}

void* SKIZO_API _so_Method_invokeImpl(void* methodHandle, void* thisObj, void* args)
{
    const CMethod* pMethod = ((CMethod*)methodHandle);
    return pMethod->InvokeDynamic(thisObj, args);
}

/* The icalls for retrieving parameters are designed to generate zero garbage, the disadvantage is they're awkward. */
/* NOTE index "-1" will return info for returnParameter */
int SKIZO_API _so_Method_getParameterCount(void* methodHandle)
{
    const CMethod* pMethod = ((CMethod*)methodHandle);
    return pMethod->Signature().Params->Count();
}

void* SKIZO_API _so_Method_getParameterTypeHandle(void* methodHandle, int index)
{
    CMethod* pMethod = ((CMethod*)methodHandle);

    // Special case for returnParameter.
    if(index == -1) {
        if(pMethod->Signature().ReturnType.PrimType == E_PRIMTYPE_VOID) {
            return nullptr;
        } else {
            assert(pMethod->Signature().ReturnType.ResolvedClass);
            return pMethod->Signature().ReturnType.ResolvedClass;
        }
    } else {
        assert(index >= 0 && index < pMethod->Signature().Params->Count());

        CParam* param = pMethod->Signature().Params->Array()[index];
        assert(param->Type.ResolvedClass);
        return param->Type.ResolvedClass;
    }
}

void* SKIZO_API _so_Method_getParameterName(void* methodHandle, int index)
{
    const CMethod* pMethod = ((CMethod*)methodHandle);
    assert(index >= 0 && index < pMethod->Signature().Params->Count());

    const CParam* param = pMethod->Signature().Params->Array()[index];
    Auto<const CString> name (param->Name.ToString());

    return CDomain::ForCurrentThread()->CreateString(name, true);
}

int SKIZO_API _so_Method_getAccessModifierImpl(void* methodHandle)
{
    const CMethod* pMethod = ((CMethod*)methodHandle);

    return (int)pMethod->Access();
}

}

} }
