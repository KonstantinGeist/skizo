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

#ifndef FILESYSTEM_H_INCLUDED
#define FILESYSTEM_H_INCLUDED

#include "ArrayList.h"
#include "DateTime.h"

// ****************************************************
// Removes WinAPI defines which wreck our method names.
#ifdef SetCurrentDirectory
    #undef SetCurrentDirectory
#endif
#ifdef CopyFile
    #undef CopyFile
#endif
#ifdef CreateFile
    #undef CreateFile
#endif
#ifdef DeleteFile
    #undef DeleteFile
#endif
// ****************************************************

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace io {

/**
 * File info.
 */
class CFileSystemInfo: public skizo::core::CObject
{
public:
    CFileSystemInfo(const skizo::core::SDateTime& lastWriteTimeUtc, so_long size);

    /**
     * Last time the file was written to in local time.
     */
    skizo::core::SDateTime LastWriteTime() const;

    /**
     * Last time the file was written to in UTC time.
     */    
    skizo::core::SDateTime LastWriteTimeUtc() const;

    /**
     * The size of the file.
     */
    so_long Size() const;

private:
    skizo::core::SDateTime m_lastWriteTimeUtc;
    so_long m_size;
};

} }

namespace skizo { namespace io { namespace FileSystem {

/**
 * Determines whether the specified file exists.
 *
 * @param path the file to check
 * @return true if the path contains the name of an existing file; otherwise,
 * false. This method also returns false if path is an invalid path, or a
 * zero-length string.
 */
bool FileExists(const skizo::core::CString* path);

/**
 * Determines whether the specified directory exists.
 *
 * @param path the file to check
 * @return true if the path contains the name of an existing directory; otherwise,
 * false. This method also returns false if path is an invalid path, or a
 * zero-length string.
 */
bool DirectoryExists(const skizo::core::CString* path);

/**
 * Gets the fully qualified path of the current working directory.
 *
 * @warning Not thread-safe.
 */
const skizo::core::CString* GetCurrentDirectory();

/**
 * Sets the current working directory.
 *
 * @warning Not thread-safe.
 */
void SetCurrentDirectory(const skizo::core::CString* curDic);

/**
 * Creates a directory.
 */
void CreateDirectory(const skizo::core::CString* path);

/**
 * Recursively deletes the directory and all the content inside it.
 */
void DeleteDirectory(const skizo::core::CString* path);

/**
 * Creates a file at a specified path.
 */
void CreateFile(const skizo::core::CString* path);

/**
 * Deletes a file.
 */
void DeleteFile(const skizo::core::CString* path);

/**
 * Renames/moves a directory from oldPath to newPath.
 */ 
void RenameDirectory(const skizo::core::CString* oldPath, const skizo::core::CString* newPath);

/**
 * Lists files in a given directory.
 *
 * @param dir the directory to list files in
 * @param returnFullPath if returnFullPath is true, then automatically combines
 * the found short paths with "dir". Otherwise, returns short paths relative to dir
 * @return a list of paths to the files in the given directory.
 */
skizo::collections::CArrayList<const skizo::core::CString*>*
    ListFiles(const skizo::core::CString* dir, bool returnFullPath);

/**
 * Lists subdirectories in a given parent directory.
 *
 * @param dir the parent directory to list subdirectories in
 * @param returnFullPath if returnFullPath is true, then automatically combines
 * the found short paths with "dir". Otherwise, returns short paths relative to dir
 * @return a list of paths to the subdirectories in the given parent directory.
 */
skizo::collections::CArrayList<const skizo::core::CString*>*
    ListDirectories(const skizo::core::CString* dir, bool returnFullPath);

/**
 * Returns a list of logical drives.
 *
 * @warning As Linux has no notion of logical drives, the home path is returned
 * instead.
 */
skizo::collections::CArrayList<const skizo::core::CString*>* GetLogicalDrives();

skizo::io::CFileSystemInfo* GetFileSystemInfo(const skizo::core::CString* path);

/**
 * Compares two paths to find out whether they refer to the same physical file
 * (works wih hard links and other forms of indirection).
 */
bool IsSameFile(const skizo::core::CString* path1, const skizo::core::CString* path2);

/**
 * Copies an existing file to a new location.
 */
void CopyFile(const skizo::core::CString* oldPath, const skizo::core::CString* newPath);

}

} }

#endif // FILESYSTEM_H_INCLUDED
