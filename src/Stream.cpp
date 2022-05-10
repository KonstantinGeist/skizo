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

#include "Stream.h"
#include "Contract.h"

namespace skizo { namespace io {
using namespace skizo::core;

bool CStream::CanSeek() const
{
    return false;
}

so_long CStream::GetPosition() const
{
    SKIZO_THROW(EC_NOT_IMPLEMENTED);
    return 0;
}

void CStream::SetPosition(so_long pos)
{
    SKIZO_THROW(EC_NOT_IMPLEMENTED);
}

so_long CStream::Size() const
{
    SKIZO_THROW(EC_NOT_IMPLEMENTED);
    return 0;
}

so_long CStream::Read(char* buf, so_long count, bool allowPartial)
{
    return this->Read(buf, count);
}

so_long CStream::Write(const char* buf, so_long count, bool allowPartial)
{
    return this->Write(buf, count);
}

#define READTO_BUFSZ 1024
void CStream::ReadTo(CStream* stream, so_long sz)
{
    SKIZO_REQ_NOT_NEG(sz);

    char buf[READTO_BUFSZ];
    so_long totalSize = 0;

    while(totalSize < sz) {
        int bReq = sz - totalSize;
        if(bReq > READTO_BUFSZ) {
            bReq = READTO_BUFSZ;
        }

        const so_long bRead = this->Read(buf, bReq, true);
        if(bRead < READTO_BUFSZ) {
            // The remnant.
            stream->Write(buf, bRead);
            break;
        } else {
            stream->Write(buf, READTO_BUFSZ);
        }

        totalSize += bRead;
    }
}

void CStream::Flush()
{
    // No-op.
}

} }
