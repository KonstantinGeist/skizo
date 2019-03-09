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

#ifndef FILEUTILS_H_INCLUDED
#define FILEUTILS_H_INCLUDED

#include "ArrayList.h"
#include "Stream.h"
#include "String.h"

namespace skizo { namespace io { namespace FileUtils {

/**
 * Opens a text file by the given path, reads all lines of the file into a
 * string array, and then closes the file.
 */
skizo::collections::CArrayList<const skizo::core::CString*>*
        ReadAllLines(const skizo::core::CString* path);

/**
 * Reads all lines from the stream into a string array.
 */
skizo::collections::CArrayList<const skizo::core::CString*>*
        ReadAllLines(skizo::io::CStream* stream);

/**
 * Opens a text file, reads all lines of the file into a string, and then closes
 * the file.
 */
const skizo::core::CString* ReadAllText(const skizo::core::CString* path);

/**
 * Reads all lines from the stream into a string.
 */
const skizo::core::CString* ReadAllText(skizo::io::CStream* stream);

/**
 * Creates a new file, writes one or more strings to the file, and then closes
 * the file.
 */
void WriteAllLines(const skizo::core::CString* path,
                   const skizo::collections::CArrayList<const skizo::core::CString*>* lines);

/**
 * Reads all bytes in the stream (depends on stream::Size())
 * Delete the output with delete []
 */
so_byte* ReadAllBytes(skizo::io::CStream* stream, so_long* out_size = 0);

}

} }

#endif // FILEUTILS_H_INCLUDED
