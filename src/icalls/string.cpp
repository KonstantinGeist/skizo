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
#include "../NativeHeaders.h"
#include "../RuntimeHelpers.h"

// ************************************************************************************
//   General information on the way string literals are constructed.
//
//  There are two things to consider:
//  a) vtables are emitted in TCC and registered with internal metadata using
//     _soX_regvtable
//  b) string literals that appear in the code should be pre-allocated and their
//     pointers hardcoded into the emitted TCC code.
//
//  THE PROBLEM: TCC wants a preallocated string so that it was able to hardcode
//  its pointer to the output code, but we can't allocate strings before TCC
//  defines vtables in that compiled code!
//
//  THE SOLUTION: String literals are allocated on a separate GC heap (see
//  SMemoryManager::StringLiterals) before TCC code is compiled, as expected.
//  Since TCC wasn't yet compiled, the vtable for strings isn't ready, so we allocate
//  string literals _with zero vtables_.
//
//  In the prolog code, right after _soX_regvtable's, we make a call to _soX_patchstrings
//  (implemented in RuntimeHelpers.cpp) which iterates over string literals and
//  patches their vtables (as _soX_regvtable on the string class was already called
//  and we know the string vtable).
//
//  And that's about it. In the GC, we must make sure the new GC segment doesn't
//  make our basic collection scheme a mess. To achieve that, after every mark
//  phase, we re-mark all string literals back to "live" so that their vtables
//  weren't corrupted. We don't add string literals as roots because allocating
//  string literals might cause a GC collection somewhere and, since string literals
//  have zero vtables, that would crash the GC as it expects the string objects to
//  to have valid vtables. So, our string literals are kept in a totally separate segment
//  of the GC, and are always "live" even if they aren't roots.
//
//  In the sweep phase, the GC iterates over objects on the normal heap (see
//  SMemoryManager::FirstCell and SMemoryManager::LastCell) and never "sees" our string
//  literals which are stored in a different segment (SMemoryManager::StringLiterals),
//  therefore never attempting to free them.
//
//  String literals are only destroyed on domain teardown
//  (SMemoryManager::CollectGarbage(..) with "judgementDay" flag set).
// ***********************************************************************************

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

extern "C" {

int SKIZO_API _so_string_length(void* self)
{
    SKIZO_NULL_CHECK(self);

    return so_string_of(self)->Length();
}

_so_char SKIZO_API _so_string_get(void* self, int index)
{
    SKIZO_NULL_CHECK(self);

    SStringHeader* header = (SStringHeader*)self;

    if(index < 0 || index >= header->pStr->Length()) {
        CDomain::Abort("Char index out of range.");
        return 0;
    }

    return header->pStr->Chars()[index];
}

void* SKIZO_API _so_string_op_add(void* self, void* other)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(other);

    Auto<const CString> r (so_string_of(self)->Concat(so_string_of(other)));
    return CDomain::ForCurrentThread()->CreateString(r);
}

void* SKIZO_API _so_string_toString(void* self)
{
    SKIZO_NULL_CHECK(self);

    return self;
}

void SKIZO_API _so_string_print(void* self)
{
    SKIZO_NULL_CHECK(self);

    so_string_of(self)->DebugPrint();
}

void* SKIZO_API _so_string_substring(void* self, int start, int length)
{
    SKIZO_NULL_CHECK(self);

    Auto<const CString> r;
    bool abort = false;

    try {
        r.SetPtr(so_string_of(self)->Substring(start, length));
    } catch (const SException& e) {
        abort = true;
    }
    if(abort) {
        CDomain::Abort("Out of range.");
    }

    return CDomain::ForCurrentThread()->CreateString(r);
}

int SKIZO_API _so_string_hashCode(void* self)
{
    SKIZO_NULL_CHECK(self);

    return so_string_of(self)->GetHashCode();
}

_so_bool SKIZO_API _so_string_op_equals(void* self, void* other)
{
    if(!self && !other) return true;
    if(!self && other) return false;
    if(self && !other) return false;

    return so_string_of(self)->Equals(so_string_of(other));
}

_so_bool SKIZO_API _so_string_equals(void* self, void* other)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(other);

    if(so_class_of(self) != so_class_of(other)) {
        return false;
    }

    return _so_string_op_equals(self, other);
}

void* SKIZO_API _so_string_split(void* self, void* substring)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(substring);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<CArrayList<const CString*> > _r
            (so_string_of(self)->Split(so_string_of(substring)));

        r = CDomain::ForCurrentThread()->CreateArray(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_string_toLowerCase(void* self)
{
    SKIZO_NULL_CHECK(self);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (so_string_of(self)->ToLowerCase());
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_string_toUpperCase(void* self)
{
    SKIZO_NULL_CHECK(self);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (so_string_of(self)->ToUpperCase());
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

int SKIZO_API _so_string_findSubstring(void* self, void* del, int sourceStart)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(del);

    int r = -1;
    SKIZO_GUARD_BEGIN
        r = so_string_of(self)->FindSubstring(so_string_of(del), sourceStart);
    SKIZO_GUARD_END
    return r;
}

_so_bool SKIZO_API _so_string_startsWith(void* self, void* substring)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(substring);
    return so_string_of(self)->StartsWith(so_string_of(substring));
}

_so_bool SKIZO_API _so_string_endsWith(void* self, void* substring)
{
    SKIZO_NULL_CHECK(self);
    SKIZO_NULL_CHECK(substring);
    return so_string_of(self)->EndsWith(so_string_of(substring));
}

void* SKIZO_API _so_string_trim(void* self)
{
    SKIZO_NULL_CHECK(self);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (so_string_of(self)->Trim());
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void SKIZO_API _so_string_dtor(void* self)
{
    so_string_of(self)->Unref();
}

}

} }
