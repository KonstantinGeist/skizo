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

#include "../Path.h"
#include "../Domain.h"
#include "../RuntimeHelpers.h"
#include "../String.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::io;

extern "C" {

void* SKIZO_API _so_Path_changeExtension(void* path, void* newExt)
{
    SKIZO_NULL_CHECK(path);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (Path::ChangeExtension(so_string_of(path),
                                                newExt? so_string_of(newExt): 0));
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_Path_getExtension(void* path)
{
    SKIZO_NULL_CHECK(path);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> str (Path::GetExtension(so_string_of(path)));
        if(str) {
            r = CDomain::ForCurrentThread()->CreateString(str);
        }
    SKIZO_GUARD_END
    return r;
}

_so_bool SKIZO_API _so_Path_hasExtension(void* path, void* ext)
{
    SKIZO_NULL_CHECK(path);
    SKIZO_NULL_CHECK(ext);

    _so_bool b = false;
    SKIZO_GUARD_BEGIN
        b = Path::HasExtension(so_string_of(path), so_string_of(ext));
    SKIZO_GUARD_END
    return b;
}

void* SKIZO_API _so_Path_combine(void* path1, void* path2)
{
    SKIZO_NULL_CHECK(path1);
    SKIZO_NULL_CHECK(path2);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (Path::Combine(so_string_of(path1), so_string_of(path2)));
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_Path_getDirectoryName(void* path)
{
    SKIZO_NULL_CHECK(path);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (Path::GetDirectoryName(so_string_of(path)));
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_Path_getFileName(void* path)
{
    SKIZO_NULL_CHECK(path);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (Path::GetFileName(so_string_of(path)));
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_Path_getParent(void* path)
{
    SKIZO_NULL_CHECK(path);

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (Path::GetParent(so_string_of(path)));
        r = CDomain::ForCurrentThread()->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_Path_getFullPath(void* path)
{
    SKIZO_NULL_CHECK(path);

    CDomain* domain = CDomain::ForCurrentThread();

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<const CString> _r (domain->SecurityManager().GetFullPath(so_string_of(path)));
        r = domain->CreateString(_r);
    SKIZO_GUARD_END
    return r;
}

}

} }
