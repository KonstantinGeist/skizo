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

#include "../FileStream.h"
#include "../RuntimeHelpers.h"
#include "../Domain.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::io;

extern "C" {

#define SKIZO_CHECK_IF_CLOSED(x) if(!x) CDomain::Abort("FileStream instance closed.")

void* SKIZO_API _so_FileStream_openImpl(void* pPath, void* pAccess)
{
    SKIZO_NULL_CHECK(pPath);
    SKIZO_NULL_CHECK(pAccess);

    CDomain::DemandFileIOPermission(so_string_of(pPath));

    const SEnumHeader* soaccess = (SEnumHeader*)pAccess;
    EFileAccess daaccess;

    if(soaccess->stringValue->pStr->EqualsASCII("READ")) {
        daaccess = E_FILEACCESS_READ;
    } else if(soaccess->stringValue->pStr->EqualsASCII("WRITE")) {
        daaccess = E_FILEACCESS_WRITE;
    } else {
        daaccess = E_FILEACCESS_READ;
        SKIZO_REQ_NEVER
    }

    try {
        return CFileStream::Open(so_string_of(pPath), daaccess);
    } catch (const SException& e) {
        // NOTE Doesn't abort because Skizo is to return "FileStream?"
        return nullptr;
    }
}

void SKIZO_API _so_FileStream_destroyImpl(void* dFileStream)
{
    if(dFileStream) { // guideline to check for self in dtors
        ((CFileStream*)dFileStream)->Unref();
    }
}

_so_bool SKIZO_API _so_FileStream_getBoolProp(void* dFileStream, int index)
{
    SKIZO_CHECK_IF_CLOSED(dFileStream);

    switch(index) {
        case 0: return ((CFileStream*)dFileStream)->CanRead();
        case 1: return ((CFileStream*)dFileStream)->CanWrite();
        case 2: return ((CFileStream*)dFileStream)->CanSeek();
        default: SKIZO_REQ_NEVER; return false;
    }
}

int SKIZO_API _so_FileStream_getIntProp(void* dFileStream, int index)
{
    SKIZO_CHECK_IF_CLOSED(dFileStream);

    switch(index) {
        case 0: return (int)((CFileStream*)dFileStream)->GetPosition();
        case 1: return (int)((CFileStream*)dFileStream)->Size();
        default: SKIZO_REQ_NEVER; return 0;
    }
}

void SKIZO_API _so_FileStream_setIntProp(void* dFileStream, int index, int value)
{
    SKIZO_CHECK_IF_CLOSED(dFileStream);

    switch(index) {
        case 0: ((CFileStream*)dFileStream)->SetPosition(value); break;
        default: SKIZO_REQ_NEVER; break;
    }
}

int SKIZO_API _so_FileStream_readImpl(void* dFileStream, void* buffer, int count)
{
    SKIZO_CHECK_IF_CLOSED(dFileStream);
    SKIZO_NULL_CHECK(buffer);

    return (int)((CFileStream*)dFileStream)->Read((char*)buffer, count);
}

int SKIZO_API _so_FileStream_writeImpl(void* dFileStream, void* buffer, int count)
{
    SKIZO_CHECK_IF_CLOSED(dFileStream);
    SKIZO_NULL_CHECK(buffer);

    return (int)((CFileStream*)dFileStream)->Write((char*)buffer, count);
}

}

} }
