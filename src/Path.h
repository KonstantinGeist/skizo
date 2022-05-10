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

#ifndef PATH_H_INCLUDED
#define PATH_H_INCLUDED

namespace skizo { namespace core {
    class CString;
} }

/**
 * Utility functions to work with paths.
 *
 * @warning Paths should be "normalized", i.e. Linux-like (except, logical
 * drivers are supported on Windows).
 */
namespace skizo { namespace io { namespace Path {

/**
 * Converts the path to the normalized form (Linux-like).
 * For Windows, that means converting '\' to '/'. For Linux, it's a no-op.
 */
const skizo::core::CString* Normalize(const skizo::core::CString* path);

/**
 * Changes the extension of a path string.
 *
 * @note newExt can be null -- the path without an extension is returned.
 */
const skizo::core::CString* ChangeExtension(const skizo::core::CString* path,
                                           const skizo::core::CString* newExt);
const skizo::core::CString* ChangeExtension(const skizo::core::CString* path,
                                           const char* newExt);

/**
 * Returns the extension of the specified path string.
 *
 * @note Returns null if no extension.
 */
const skizo::core::CString* GetExtension(const skizo::core::CString* path);

/**
 * Determines whether a path includes a file name extension.
 */
bool HasExtension(const skizo::core::CString* path,
                  const skizo::core::CString* ext);

/**
 * Combines two strings into a path in a portable manner.
 */
const skizo::core::CString* Combine(const skizo::core::CString* path1,
                                   const skizo::core::CString* path2);
const skizo::core::CString* Combine(const skizo::core::CString* path1,
                                   const char* path2);

/**
 * Returns the short name of the directory this path refers to.
 * For example, for "C:/mydir", returns "mydir".
 */
const skizo::core::CString* GetDirectoryName(const skizo::core::CString* path);

/**
 * Returns the short name of the file this path refers to.
 * For example, for "C:/myprogram.exe", returns "myprogram.exe".
 */
const skizo::core::CString* GetFileName(const skizo::core::CString* path);

/**
 * Gets the parent of this path ("C:/a/b" => "C:/a")
 *
 * @note Inverse of GetDirectoryName.
 */
const skizo::core::CString* GetParent(const skizo::core::CString* path);

/**
 * Retrieves the full path and file name of the specified file.
 *
 * @warning May depend on FileSystem::GetCurrentDirectory() and should be
 * discouraged from being used in multithreaded situations.
 */
const skizo::core::CString* GetFullPath(const skizo::core::CString* path);

}

} }

#endif // PATH_H_INCLUDED
