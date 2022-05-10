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

#include "../FileSystem.h"
#include "../Class.h"
#include "../Domain.h"
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

extern "C" {

_so_bool SKIZO_API _so_FileSystem_fileExists(void* path)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    bool r = false;
    SKIZO_GUARD_BEGIN
        r = FileSystem::FileExists(so_string_of(path));
    SKIZO_GUARD_END
    return r;
}

_so_bool SKIZO_API _so_FileSystem_directoryExists(void* path)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    bool r = false;
    SKIZO_GUARD_BEGIN
        r = FileSystem::DirectoryExists(so_string_of(path));
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_FileSystem_currentDirectory()
{
    CDomain::DemandPermission("FileIOPermission");

    CDomain* domain = CDomain::ForCurrentThread();
    return domain->CreateString(domain->SecurityManager().CurrentDirectory(), true);
}

void SKIZO_API _so_FileSystem_createDirectory(void* path)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    SKIZO_GUARD_BEGIN
        FileSystem::CreateDirectory(so_string_of(path));
    SKIZO_GUARD_END
}

void SKIZO_API _so_FileSystem_deleteDirectory(void* path)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    SKIZO_GUARD_BEGIN
        FileSystem::DeleteDirectory(so_string_of(path));
    SKIZO_GUARD_END
}

void* SKIZO_API _so_FileSystem_listFiles(void* path, _so_bool returnFullPath)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<CArrayList<const CString*> > files
                (FileSystem::ListFiles(so_string_of(path), returnFullPath));

        r = CDomain::ForCurrentThread()->CreateArray(files);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_FileSystem_listDirectories(void* path)
{
    SKIZO_NULL_CHECK(path);
    CDomain::DemandFileIOPermission(so_string_of(path));

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        // NOTE Doesn't make use of the "returnFullPath" feature as it uses
        // Skizo's GetFullPath which isn't domain-safe due to its reliance
        // on the process-wide current directory setting.
        Auto<CArrayList<const CString*> > dirs
                (FileSystem::ListDirectories(so_string_of(path), false));

        r = CDomain::ForCurrentThread()->CreateArray(dirs);
    SKIZO_GUARD_END
    return r;
}

void* SKIZO_API _so_FileSystem_logicalDrives()
{
    CDomain::DemandPermission("FileIOPermission");

    void* r = nullptr;
    SKIZO_GUARD_BEGIN
        Auto<CArrayList<const CString*> > drives (FileSystem::GetLogicalDrives());
        r = CDomain::ForCurrentThread()->CreateArray(drives);
    SKIZO_GUARD_END
    return r;
}

_so_bool SKIZO_API _so_FileSystem_isSameFile(void* path1, void* path2)
{
    SKIZO_NULL_CHECK(path1);
    SKIZO_NULL_CHECK(path2);
    CDomain::DemandPermission("FileIOPermission");

    bool r = false;
    SKIZO_GUARD_BEGIN
        r = FileSystem::IsSameFile(so_string_of(path1), so_string_of(path2));
    SKIZO_GUARD_END

    return r;
}

void SKIZO_API _so_FileSystem_copyFile(void* oldPath, void* newPath)
{
    SKIZO_NULL_CHECK(oldPath);
    SKIZO_NULL_CHECK(newPath);
    CDomain::DemandFileIOPermission(so_string_of(oldPath));
    CDomain::DemandFileIOPermission(so_string_of(newPath));

    SKIZO_GUARD_BEGIN
        FileSystem::CopyFile(so_string_of(oldPath), so_string_of(newPath));
    SKIZO_GUARD_END
}

}

} }
