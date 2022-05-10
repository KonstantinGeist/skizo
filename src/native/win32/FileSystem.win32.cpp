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
#include "../../CoreUtils.h"
#include "../../Path.h"
#include "../../String.h"
#include <limits.h> // for CHAR_BIT

using namespace skizo::core;
using namespace skizo::collections;

namespace skizo { namespace io { namespace FileSystem {

bool FileExists(const CString* path)
{
    // FIX old code used path validation here, this kind of queries shouldn't do that

    DWORD attr = GetFileAttributes((LPCTSTR)path->Chars());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryExists(const CString* path)
{
    // FIX old code used path validation here, this kind of queries shouldn't do that

    DWORD attr = GetFileAttributes((LPCTSTR)path->Chars());
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// *******************

enum EListFiles
{
    E_LISTFILES_FILES,
    E_LISTFILES_ENTRIES,
    E_LISTFILES_DIRECTORIES,
};

const CString* GetCurrentDirectory()
{
    WCHAR tmp[MAX_PATH + 1];
    ::GetCurrentDirectory(MAX_PATH + 1, tmp);

    Auto<const CString> preR (CString::FromUtf16(reinterpret_cast<so_char16*>(tmp)));
    return Path::Normalize(preR);
}

void SetCurrentDirectory(const CString* curDic)
{
    CoreUtils::ValidatePath(curDic);

    if(!::SetCurrentDirectoryW((LPCTSTR)curDic->Chars())) {
        CoreUtils::ThrowWin32Error();
    }
}

void CreateDirectory(const CString* _path)
{
    // Win32 code only creates one directory at a time. If the specified path is
    // "A/B/C", CreateDirectory(..) fails.
    // Thanks to http://blog.nuclex-games.com/2012/06/how-to-create-directories-recursively-with-win32/
    CoreUtils::ValidatePath(_path);

	Auto<const CString> path (Path::Normalize(_path));

    // If the specified directory name doesn't exist, does our thing.
    DWORD fileAttributes = ::GetFileAttributes((LPCTSTR)path->Chars());
    if(fileAttributes == INVALID_FILE_ATTRIBUTES) {

        // Recursively does it all again for the parent directory, if any.
        int slashIndex = path->FindLastChar(SKIZO_CHAR('/'));
        if(slashIndex != -1) {
            Auto<const CString> upperPath (path->Substring(0, slashIndex));
            CreateDirectory(upperPath);
        }

        // Creates the last directory on the path (the recursive calls will
        // have taken care of the parent directories by now).
        BOOL result = ::CreateDirectoryW((LPCTSTR)path->Chars(), 0);
        if(result == FALSE) {
            CoreUtils::ThrowWin32Error();
        }

    } else { // Specified directory name already exists as a file or directory.

        bool isDirectoryOrJunction =
          ((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
          ((fileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);

        if(!isDirectoryOrJunction) {
            SKIZO_THROW_WITH_MSG(EC_PATH_NOT_FOUND, "Could not create directory because a file with the same name exists");
        }
    }
}

// For use by DeleteDirectory(..)
struct SSearchHandleScope {
    HANDLE handle;

    explicit SSearchHandleScope(HANDLE searchHandle): handle(searchHandle) { }
    ~SSearchHandleScope() { ::FindClose(this->handle); }
};

void DeleteDirectory(const CString* path)
{
    // Win32 code removes only one empty directory at a time. If the specified
    // path is "A/B/C" or has files inside, RemoveDirectory(..) fails.
    // Thanks to http://blog.nuclex-games.com/2012/06/how-to-delete-directories-recursively-with-win32/
    CoreUtils::ValidatePath(path);

    WIN32_FIND_DATA findData;

    // First, deletes the contents of the directory, recursively for subdirectories.
    Auto<const CString> allFilesMask (CString::FromUtf16((so_char16*)L"\\*"));
    Auto<const CString> searchMask (path->Concat(allFilesMask));

    HANDLE searchHandle = ::FindFirstFileEx((LPCTSTR)searchMask->Chars(),
                                            FindExInfoStandard, //FindExInfoBasic,
                                            &findData,
                                            FindExSearchNameMatch,
                                            0,
                                            0);
    if(searchHandle == INVALID_HANDLE_VALUE) {
        CoreUtils::ThrowWin32Error();
        DWORD lastError = ::GetLastError();
        if(lastError != ERROR_FILE_NOT_FOUND) { // or ERROR_NO_MORE_FILES, ERROR_NOT_FOUND?
            SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Could not start directory enumeration.");
        }
    }

    // Did this directory have any contents? If so, deletes them first.
    if(searchHandle != INVALID_HANDLE_VALUE) {
        SSearchHandleScope scope(searchHandle);
        for(;;) {

            // Do not process the obligatory '.' and '..' directories.
            if(findData.cFileName[0] != '.') {
                bool isDirectory =
                    ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
                    ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);

                // Subdirectories need to be handled by deleting their contents first.
                Auto<const CString> fileName (CString::FromUtf16((so_char16*)&findData.cFileName[0]));
                Auto<const CString> filePath (CString::Format("%o\\%o",
                                                              static_cast<const CObject*>(path),
                                                              static_cast<const CObject*>(fileName.Ptr())));

                if(isDirectory) {
                    DeleteDirectory(filePath);
                } else {
                    BOOL result = ::DeleteFileW((LPCTSTR)filePath->Chars());
                    if(result == FALSE) {
                        SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Could not delete file.");
                    }
                }
            }

            // Advances to the next file in the directory
            BOOL result = ::FindNextFile(searchHandle, &findData);
            if(result == FALSE) {
                DWORD lastError = ::GetLastError();
                if(lastError != ERROR_NO_MORE_FILES) {
                    SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Error enumerating directory.");
                }
                break; // All directory contents enumerated and deleted.
            }

        } // for
    }

    // The directory is empty, we can now safely remove it.
    BOOL result = ::RemoveDirectory((LPCTSTR)path->Chars());
    if(result == FALSE) {
        SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Could not remove directory.");
    }
}

void DeleteFile(const CString* path)
{
    CoreUtils::ValidatePath(path);

    BOOL b = DeleteFileW((LPCTSTR)path->Chars());
    if(!b) {
        CoreUtils::ThrowWin32Error();
    }
}

static CArrayList<const CString*>* listEntriesInternal(const CString* dir,
                                                       bool returnFullPath,
                                                       EListFiles type)
{
    CoreUtils::ValidatePath(dir);

    Auto<CArrayList<const CString*> > r (new CArrayList<const CString*>());

    // Windows needs a wildcard actually, not just a path (unlike Linux).
    Auto<const CString> path (Path::Combine(dir, "*"));

    WIN32_FIND_DATA data;

    HANDLE handle = FindFirstFile((LPCTSTR)path->Chars(), &data);
    if(handle == INVALID_HANDLE_VALUE) {
        CoreUtils::ThrowWin32Error();
    }

    while(FindNextFile(handle, &data) != 0) {
        Auto<const CString> name (CString::FromUtf16((so_char16*)data.cFileName));
        name.SetPtr(Path::Normalize(name));

        if(name->EqualsASCII(".") || name->EqualsASCII("..")) {
            continue;
        }

        if(type == E_LISTFILES_FILES || type == E_LISTFILES_DIRECTORIES) {
            // If we don't need directories, skip it.
            if(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if(type != E_LISTFILES_DIRECTORIES) {
                    continue;
                }
            } else {
                // It's a file.
                if(type != E_LISTFILES_FILES) {
                    continue;
                }
            }
        }

        if(returnFullPath) {
            Auto<const CString> combinedName (Path::Combine(dir, name));
            r->Add(combinedName);
        } else {
            r->Add(name);
        }
    }

    if(handle != INVALID_HANDLE_VALUE) {
        FindClose(handle);
    }

    r->Ref();
    return r;
}

CArrayList<const CString*>* ListFiles(const CString* dir, bool returnFullPath)
{
    CoreUtils::ValidatePath(dir);

    return listEntriesInternal(dir, returnFullPath, E_LISTFILES_FILES);
}

CArrayList<const CString*>* ListDirectories(const CString* dir, bool returnFullPath)
{
    CoreUtils::ValidatePath(dir);

    return listEntriesInternal(dir, returnFullPath, E_LISTFILES_DIRECTORIES);
}

CArrayList<const CString*>* GetLogicalDrives()
{
    const int maxDriveCount = (sizeof(DWORD) * CHAR_BIT);
    CArrayList<const CString*>* r = new CArrayList<const CString*>(maxDriveCount);

    DWORD bitmask = ::GetLogicalDrives();
    for(int i = 0; i < maxDriveCount; i++) {
        DWORD bitIndex = 1 << i;
        if (bitmask & bitIndex) {
            so_char16* driveChars;
            Auto<const CString> drive (CString::CreateBuffer(3, &driveChars));
            driveChars[0] = so_char16('A') + i;
            driveChars[1] = ':';
            driveChars[2] = '/';
            r->Add(drive);
        }
    }

    return r;
}

CFileSystemInfo* GetFileSystemInfo(const CString* path)
{
    CoreUtils::ValidatePath(path);

    WIN32_FILE_ATTRIBUTE_DATA data;
    const BOOL b = GetFileAttributesEx((LPCTSTR)path->Chars(),
                                        GetFileExInfoStandard,
                                       &data);
    if(!b) {
        CoreUtils::ThrowWin32Error();
    }

    // Finds last write time.
    SYSTEMTIME systemTime;
    FileTimeToSystemTime(&data.ftLastWriteTime, &systemTime);
    SDateTime lastWriteTime (SDateTime::CreateFromSYSTEMTIME(E_DATETIMEKIND_UTC, &systemTime));

    // Finds size.
    const so_long size = (((so_long)data.nFileSizeHigh) << 32 ) + data.nFileSizeLow;

    return new CFileSystemInfo(lastWriteTime, size);
}

bool IsSameFile(const CString* path1, const CString* path2)
{
    CoreUtils::ValidatePath(path1);
    CoreUtils::ValidatePath(path2);

    // First check, the most optimistic.
    if(path1->Equals(path2)) {
        return true;
    }

    // The optimistic check failed, let's do a less optimistic check:
    // get full paths and compare them once again.
    Auto<const CString> fullPath1 (Path::GetFullPath(path1));
    Auto<const CString> fullPath2 (Path::GetFullPath(path2));
    if(fullPath1->Equals(fullPath2)) {
        return true;
    }

    bool result = false;

    // The least optimistic and the most hardcore way to check whether two paths refer
    // to the same file is to open paths and compare their internal unique numbers.
    HANDLE handles [2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    for(int i = 0; i < 2; i++) {
        const CString* path = (i == 0)? fullPath1: fullPath2;
        handles[i] = ::CreateFileW((LPWSTR)path->Chars(),
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL);
    }

    if(handles[0] != INVALID_HANDLE_VALUE && handles[1] != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION fileInfos[2];
        BOOL b = TRUE;

        for(int i = 0; i < 2; i++) {
            b &= GetFileInformationByHandle(handles[i],
                                            &fileInfos[i]);
        }

        if(b) {
            result = (fileInfos[0].dwVolumeSerialNumber == fileInfos[1].dwVolumeSerialNumber)
                  && (fileInfos[0].nFileIndexHigh == fileInfos[1].nFileIndexHigh)
                  && (fileInfos[0].nFileIndexLow == fileInfos[1].nFileIndexLow);
        }
    }

    for(int i = 0; i < 2; i++) {
        if(handles[i] != INVALID_HANDLE_VALUE) {
            CloseHandle(handles[i]);
        }
    }

    return result;
}

void CopyFile(const CString* oldPath, const CString* newPath)
{
    CoreUtils::ValidatePath(oldPath);
    CoreUtils::ValidatePath(newPath);

    if(!::CopyFileW((WCHAR*)oldPath->Chars(), (WCHAR*)newPath->Chars(), FALSE)) {
        CoreUtils::ThrowWin32Error();
    }
}

void RenameDirectory(const CString* oldPath, const CString* newPath)
{
    CoreUtils::ValidatePath(oldPath);
    CoreUtils::ValidatePath(newPath);

    if(!MoveFile((LPCTSTR)oldPath->Chars(), (LPCTSTR)newPath->Chars())) {
        CoreUtils::ThrowWin32Error();
    }
}

} } }
