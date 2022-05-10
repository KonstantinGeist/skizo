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

#include "../Domain.h"
#include "../RuntimeHelpers.h"
#include "../ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_int_toString(int i)
{
    CDomain* domain = CDomain::ForCurrentThread();
	void* r = nullptr;
	SKIZO_GUARD_BEGIN
		Auto<const CString> source (CoreUtils::IntToString(i));
		r = domain->CreateString(source, false);
	SKIZO_GUARD_END
	return r;
}

int SKIZO_API _so_int_hashCode(int i)
{
    return i;
}

_so_bool SKIZO_API _so_int_equals(int i, void* otherObj)
{
    return BoxedEquals(&i, sizeof(int), otherObj, E_PRIMTYPE_INT);
}

_so_bool SKIZO_API _so_int_parseImpl(void* soStr, int* outp)
{
    SKIZO_NULL_CHECK(soStr);
    SKIZO_NULL_CHECK(outp);

    const CString* daStr = so_string_of(soStr);

    _so_bool r = false;
    SKIZO_GUARD_BEGIN
        r = daStr->TryParseInt(outp);
    SKIZO_GUARD_END

    return r;
}

}

} }
