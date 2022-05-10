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

#include "FileSystem.h"
#include "FileStream.h" // for CreateFile(..)

using namespace skizo::core;
using namespace skizo::collections;

namespace skizo { namespace io {

// **********
// CFileSystemInfo
// *********

CFileSystemInfo::CFileSystemInfo(const SDateTime& lastWriteTimeUtc, so_long size)
    : m_lastWriteTimeUtc(lastWriteTimeUtc),
      m_size(size)
{
}

SDateTime CFileSystemInfo::LastWriteTime() const
{
    return m_lastWriteTimeUtc.ToLocalTime();
}

SDateTime CFileSystemInfo::LastWriteTimeUtc() const
{
    return m_lastWriteTimeUtc;
}

so_long CFileSystemInfo::Size() const
{
    return m_size;
}

namespace FileSystem {

void CreateFile(const CString* path)
{
    // Forces creation of a file.
    auto fs = CFileStream::Open(path, E_FILEACCESS_WRITE);
    fs->Unref();
}

}

} }
