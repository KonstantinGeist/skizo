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

#include "../../FileSystem.h"
#include "../../Application.h"
#include "../../Exception.h"
#include "../../Path.h"
#include "../../String.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <cerrno>
#include <fcntl.h>

namespace skizo { namespace io { namespace FileSystem {
using namespace skizo::core;
using namespace skizo::collections;

CArrayList<const CString*>* GetLogicalDrives()
{
    // The idea of drives doesn't really make sense under Linux.
    // So we instead simply return the home folder.

    CArrayList<const CString*>* r = new CArrayList<const CString*>();
    Auto<const CString> specialFolder (Application::GetSpecialFolder(E_SPECIALFOLDER_HOME));
    r->Add(specialFolder);
    return r;
}

bool FileExists(const CString* path)
{
    CoreUtils::ValidatePath(path);

    Utf8Auto cPath (path->ToUtf8());

    struct stat stbuf;
    if(stat(cPath, &stbuf) == -1) {
        return false;
    }

    return !S_ISDIR(stbuf.st_mode);
}

bool DirectoryExists(const CString* path)
{
    CoreUtils::ValidatePath(path);

    Utf8Auto cPath (path->ToUtf8());

    struct stat stbuf;
    if(stat(cPath, &stbuf) == -1) {
        return false;
    }

    return S_ISDIR(stbuf.st_mode);
}

void CreateDirectory(const CString* path)
{
    CoreUtils::ValidatePath(path);

    Utf8Auto cPath (path->ToUtf8());

    mode_t process_mask = umask(0);
    int result_code = mkdir(cPath, S_IRWXU | S_IRWXG | S_IRWXO);
    umask(process_mask);

    if(result_code == -1) {
        // TODO use something like ThrowLastUnixError, just like we have with Win32
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

void SetCurrentDirectory(const CString* path)
{
    CoreUtils::ValidatePath(path);

    Utf8Auto cPath (path->ToUtf8());
    if(chdir(cPath) == -1) {
        SKIZO_THROW(EC_PATH_NOT_FOUND);
    }
}

const CString* GetCurrentDirectory()
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return CString::FromUtf8(cwd);
    } else {
        return Application::GetSpecialFolder(E_SPECIALFOLDER_HOME);
    }
}

static CArrayList<const CString*>* listCommon(const CString* rootPath,
                                              bool returnFullPath,
                                              bool listDirs)
{
    struct dirent *entry;
    DIR* cDir;

    Utf8Auto cPath (rootPath->ToUtf8());
    cDir = opendir(cPath);
    if(!cDir) {
        // Conforms to the behavior of the Win32 counterpart.
        SKIZO_THROW(EC_PATH_NOT_FOUND);
    }

    Auto<CArrayList<const CString*> > r (new CArrayList<const CString*>());

    while((entry = readdir(cDir)) != NULL) {
        if(strcmp(entry->d_name, ".") != 0
          && strcmp(entry->d_name, "..") != 0)
        {
            Auto<const CString> entryPath (CString::FromUtf8(entry->d_name));
            Auto<const CString> fullPath (Path::Combine(rootPath, entryPath));

            // Trying to tell if it's a file or a directory...
            Utf8Auto cFullPath (fullPath->ToUtf8());
            struct stat stbuf;
            if(stat(cFullPath, &stbuf) == -1) { // Cannot access, or something like that.
                continue;
            }

            if(S_ISDIR(stbuf.st_mode) == listDirs) {
                if(returnFullPath) {
                    r->Add(fullPath);
                } else {
                    r->Add(entryPath);
                }
            }
        }
    }

    closedir(cDir);

    r->Ref();
    return r;
}

CArrayList<const CString*>* ListFiles(const CString* path, bool returnFullPath)
{
    CoreUtils::ValidatePath(path);

    return listCommon(path, returnFullPath, false);
}

CArrayList<const CString*>* ListDirectories(const CString* path, bool returnFullPath)
{
    CoreUtils::ValidatePath(path);

    return listCommon(path, returnFullPath, true);
}

CFileSystemInfo* GetFileSystemInfo(const CString* path)
{
    CoreUtils::ValidatePath(path);

    SDateTime lastWriteTime;
    Utf8Auto cPath (path->ToUtf8());

    struct stat stbuf;
    if(stat(cPath, &stbuf) != -1) {
        time_t tmp = stbuf.st_mtime;
        struct tm tm = *gmtime(&tmp);
        SDateTime lastWriteTime (E_DATETIMEKIND_UTC,
                                 tm.tm_year + 1900,
                                 tm.tm_mon + 1,
                                 tm.tm_mday,
                                 tm.tm_hour,
                                 tm.tm_min,
                                 tm.tm_sec,
                                 0);

        return new CFileSystemInfo(lastWriteTime, stbuf.st_size);
    } else {
        return new CFileSystemInfo(SDateTime(), 0);
    }
}

void DeleteDirectory(const CString* path)
{
    CoreUtils::ValidatePath(path);

    // TODO
    SKIZO_THROW(EC_NOT_IMPLEMENTED);
}

void DeleteFile(const CString* path)
{
    CoreUtils::ValidatePath(path);
    
    Utf8Auto cPath (path->ToUtf8());

    if(remove(cPath) != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

// Unix has no built-in function to copy files. Sad! Everyone has to roll their own
// sloppy version.
// Based on http://stackoverflow.com/questions/2180079/how-can-i-copy-a-file-on-unix-using-c
// License unknown but the code is pretty straightforward.
static EExceptionCode copyFileImpl(const char* from, const char* to)
{
    int fd_from, fd_to;
    char buf[4096];
    ssize_t nread;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0) {
        return EC_PATH_NOT_FOUND; // failure
    }

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd_to < 0) {
        goto out_error;
    }

    while(nread = read(fd_from, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if(nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else if(errno != EINTR) {
                goto out_error;
            }
        } while (nread > 0);
    }

    if(nread == 0) {
        if(close(fd_to) < 0) {
            fd_to = -1;
            goto out_error;
        }

        close(fd_from);

        return EC_OK;
    }

  out_error:
    close(fd_from);

    if(fd_to >= 0) {
        close(fd_to);
    }

    return EC_PLATFORM_DEPENDENT;
}

void CopyFile(const CString* daOldPath, const CString* daNewPath)
{
    CoreUtils::ValidatePath(daOldPath);
    CoreUtils::ValidatePath(daNewPath);

    Utf8Auto cOldPath (daOldPath->ToUtf8());
    Utf8Auto cNewPath (daNewPath->ToUtf8());

    auto ec = copyFileImpl(cOldPath, cNewPath);
    if(ec != EC_OK) {
        SKIZO_THROW(ec);
    }
}

void RenameDirectory(const CString* oldPath, const CString* newPath)
{
    CoreUtils::ValidatePath(oldPath);
    CoreUtils::ValidatePath(newPath);

    Utf8Auto cOldPath (oldPath->ToUtf8());
    Utf8Auto cNewPath (newPath->ToUtf8());

    if(rename(cOldPath, cNewPath) != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

bool IsSameFile(const CString* path1, const CString* path2)
{
    CoreUtils::ValidatePath(path1);
    CoreUtils::ValidatePath(path2);

    // TODO stub
    return path1->Equals(path2);
}

} } }
