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

#include "FileStream.h"
#include "Contract.h"
#include "Exception.h"
#include "String.h"

namespace skizo { namespace io {
using namespace skizo::core;

bool CFileStream::CanRead() const
{
    return (m_access == E_FILEACCESS_READ || m_access == E_FILEACCESS_READ_WRITE)
        && !feof((FILE*)m_handle); // TODO test?
}

bool CFileStream::CanWrite() const
{
    return m_access == E_FILEACCESS_WRITE || m_access == E_FILEACCESS_READ_WRITE;
}

CFileStream::CFileStream(char* path, EFileAccess access)
    : m_access(access),
      m_size(-1)
{
    const char* cmode;
    if(access == E_FILEACCESS_READ) {
        cmode = "rb";
    } else if(access == E_FILEACCESS_WRITE) {
        cmode = "wb";
    } else if(access == E_FILEACCESS_READ_WRITE) {
        cmode = "r+b";
    } else {
        SKIZO_THROW(EC_NOT_IMPLEMENTED);
        return;
    }

    // Opens the file. The handle may be null -- this is checked
    // by CFileStream::Open(..)
    m_handle = static_cast<void*>(fopen(path, cmode));
}

CFileStream::~CFileStream()
{
    if(m_handle) // The handle may be null, then we're being called
                 // from CFileStream::Open on error.
    {
        fclose((FILE*)m_handle);
    }
}

CFileStream* CFileStream::Open(const CString* path, EFileAccess access)
{
    CoreUtils::ValidatePath(path);

    // NOTE Required to call ToCLibString as Windows/Linux don't agree on the
    // encoding here.
    char* const cs = path->ToCLibString();
    auto r = new CFileStream(cs, access);

    // Avoiding exceptions in the constructor.
    if(!r->m_handle) {
        r->Unref();

        CString::FreeCLibString(cs);
        SKIZO_THROW(EC_PATH_NOT_FOUND);
    }

    CString::FreeCLibString(cs);
    return r;
}

so_long CFileStream::Read(char* buf, so_long count)
{
    SKIZO_REQ_NOT_NEG(count);

    return fread(buf, 1, count, (FILE*)m_handle);
}

so_long CFileStream::Write(const char* buf, so_long count)
{
    SKIZO_REQ_NOT_NEG(count);

    return fwrite(buf, 1, count, (FILE*)m_handle);
}

bool CFileStream::CanSeek() const
{
    return true;
}

void CFileStream::SetPosition(so_long pos)
{
    fseek((FILE*)m_handle, pos, SEEK_SET);
}

so_long CFileStream::GetPosition() const
{
    return ftell((FILE*)m_handle);
}

so_long CFileStream::Size() const
{
    if(m_size == -1) {
        // Save the current position.
        const so_long savedPos = ftell((FILE*)m_handle);

        // Seek to the end.
        fseek((FILE*)m_handle, 0L, SEEK_END);

        // Fetch the new position.
        m_size = ftell((FILE*)m_handle);

        // Restore the previous position.
        fseek((FILE*)m_handle, savedPos, SEEK_SET);
    }

    // Return.
    return m_size;
}

} }
