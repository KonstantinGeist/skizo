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

#include "FileUtils.h"
#include "FileStream.h"
#include "TextReader.h"
#include "TextWriter.h"
#include "StringBuilder.h"

namespace skizo { namespace io {
using namespace skizo::core;
using namespace skizo::collections;

namespace FileUtils {

CArrayList<const CString*>* ReadAllLines(CStream* stream)
{
    Auto<CTextReader> reader (new CTextReader(stream));
    Auto<CArrayList<const CString*> > r (new CArrayList<const CString*>());

    while(const CString* const str = reader->ReadLine()) {
        r->Add(str);
        str->Unref();
    }

    r->Ref();
    return r;
}

CArrayList<const CString*>* ReadAllLines(const CString* path)
{
    Auto<CFileStream> stream (CFileStream::Open(path, E_FILEACCESS_READ));
    return ReadAllLines(stream);
}

const CString* ReadAllText(CStream* stream)
{
    Auto<CTextReader> reader (new CTextReader(stream));
    Auto<CStringBuilder> sb (new CStringBuilder());

    while(const CString* str = reader->ReadLine()) {
        sb->Append(str);
        sb->AppendLine();
        str->Unref();
    }

    return sb->ToString();
}

const CString* ReadAllText(const CString* path)
{
    Auto<CFileStream> stream (CFileStream::Open(path, E_FILEACCESS_READ));
    return ReadAllText(stream);
}

void WriteAllLines(const CString* path, const CArrayList<const CString*>* lines)
{
    Auto<CFileStream> stream (CFileStream::Open(path, E_FILEACCESS_WRITE));
    Auto<CTextWriter> writer (new CTextWriter(stream));

    for(int i = 0; i < lines->Count(); i++) {
        const CString* const line = lines->Array()[i];

        writer->Write(line);
        writer->WriteLine();
    }
}

so_byte* ReadAllBytes(CStream* stream, so_long* out_size)
{
    const so_long streamSize = stream->Size();
    if(out_size) {
        *out_size = streamSize;
    }
    SKIZO_REQ_POS(streamSize);

    so_byte* const tmpBuf = new so_byte[streamSize];
    const so_long readSize = stream->Read((char*)tmpBuf, streamSize);
    if(readSize != streamSize) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    return tmpBuf;
}

}

} }
