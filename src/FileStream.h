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

#ifndef FILESTREAM_H_INCLUDED
#define FILESTREAM_H_INCLUDED

#include "Stream.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace io {

/**
 * Defines restrictions imposed on a file stream.
 */
enum EFileAccess
{
    /**
     * Read access to the file. Data can be read from the file, but can't be
     * written to it.
     *
     * @note the file must exist.
     */
    E_FILEACCESS_READ,

    /**
     * Write access to the file. Data can be written to the file, but can't be
     * read from it.
     */
    E_FILEACCESS_WRITE,

    /**
     * Read & Write access to the file. Data can be both written to the file and
     * read from it.
     *
     * @note the file must exist.
     */
	E_FILEACCESS_READ_WRITE
};

/**
 * CFileStream enables applications to read from and write to a file on disk.
 * Use CFileStream to access the information in disk files. CFileStream will open
 * a named file and provide methods to read from or write to it.
 */
class CFileStream final: public CStream
{
public:
    /**
     * Opens a new file stream.
     * @param path the path to locate the file
     * @param access the access
     */
    static CFileStream* Open(const skizo::core::CString* path,
                             EFileAccess access);
    virtual ~CFileStream();

    // **********************
    //   Implements CStream
    // **********************

    virtual bool CanRead() const override;
    virtual bool CanWrite() const override;
    virtual so_long Read(char* buf, so_long count) override;
    virtual so_long Write(const char* buf, so_long count) override;

    virtual bool CanSeek() const override;
    virtual void SetPosition(so_long pos) override;
    virtual so_long GetPosition() const override;

    virtual so_long Size() const override;

    // **********************

private:
    void* m_handle; // Native handle being wrapped.
    EFileAccess m_access;
    mutable so_long m_size;
    CFileStream(char* path, EFileAccess access);
};

} }

#endif // FILESTREAM_H_INCLUDED
