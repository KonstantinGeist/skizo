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

#include "BinaryWriter.h"
#include "Contract.h"
#include "String.h"

namespace skizo { namespace io {
using namespace skizo::core;

SBinaryWriter::SBinaryWriter(CStream* stream, EByteOrder byteOrder)
    : m_stream(stream), m_byteOrder(byteOrder)
{
}

CStream* SBinaryWriter::Stream() const
{
    return m_stream;
}

void SBinaryWriter::WriteInt(int i)
{
    if(m_byteOrder == E_BYTEORDER_NETWORK) {
        i = CoreUtils::ByteOrderHostToNetwork(i);
    }

    const so_long sz = m_stream->Write((char*)&i, sizeof(int));
    if(sz != sizeof(int)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteUInt32(so_uint32 i)
{
    if(m_byteOrder == E_BYTEORDER_NETWORK) {
        i = CoreUtils::ByteOrderHostToNetwork(i);
    }

    const so_long sz = m_stream->Write((char*)&i, sizeof(so_uint32));
    if(sz != sizeof(so_uint32)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteUInt16(so_uint16 i)
{
    // TODO
    SKIZO_REQ_EQUALS(m_byteOrder, E_BYTEORDER_HOST);

    const so_long sz = m_stream->Write((char*)&i, sizeof(so_uint16));
    if(sz != sizeof(so_uint16)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteLong(so_long l)
{
    // TODO
    SKIZO_REQ_EQUALS(m_byteOrder, E_BYTEORDER_HOST);

    const so_long sz = m_stream->Write((char*)&l, sizeof(so_long));
    if(sz != sizeof(so_long)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteFloat(float f)
{
    // TODO byte order
    const so_long sz = m_stream->Write((char*)&f, sizeof(float));
    if(sz != sizeof(float)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteBool(bool b)
{
    this->WriteInt(b); // TODO?
}

void SBinaryWriter::WriteUTF16(const CString* str, bool lengthHeader)
{
    if(str) {

        if(lengthHeader) {
            this->WriteInt(str->Length());
        }

        const so_long sz = m_stream->Write((char*)str->Chars(), str->Length() * sizeof(so_char16));
        if(sz != str->Length() * (int)sizeof(so_char16)) {
            SKIZO_THROW(EC_BAD_FORMAT);
        }

    } else {

        if(lengthHeader) {
            // ALLOWED: str==nullptr, lengthHeader==true
            this->WriteInt(0);
        } else {
            // DISALLOWED: str==nullptr, lengthHeader==false
            SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
        }

    }
}

void SBinaryWriter::WriteUTF8(const char* str, so_long size, bool lengthHeader)
{
    if(lengthHeader) {
        this->WriteInt(size);
    }

    const so_long sz = m_stream->Write(str, size);
    if(sz != size) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteUtf8(const char str[256])
{
    const int header = strlen(str);
    if(header == 0 || header == 256) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }

    this->WriteByte((so_byte)header);

    const so_long sz = m_stream->Write(str, header);
    if(sz != header) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteByte(so_byte d)
{
    const so_long sz = m_stream->Write((char*)&d, sizeof(so_byte));
    if(sz != sizeof(so_byte)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteChar(so_char16 c)
{
    // TODO byte order?

    const so_long sz = m_stream->Write((char*)&c, sizeof(so_char16));
    if(sz != sizeof(so_char16)) {
        SKIZO_THROW(EC_BAD_FORMAT);
    }
}

void SBinaryWriter::WriteVariant(const SVariant& value)
{
    if(value.Type() == E_VARIANTTYPE_OBJECT) {
        if(!value.ObjectValue()) {
            // NULL object: not really meaningful, but required for
            // calculating composition nodes' SDataId (via binary chunks)
            this->WriteUInt32(E_VARIANTTYPEEX_NULL);
        } else {
            Auto<const CString> asStr (value.ToString());

            // Special case for strings: marshals them by value.
            this->WriteUInt32(E_VARIANTTYPEEX_STRING);
            // Writes the size of the string.
            // Writes the string as UTF8.
            Utf8Auto utf8Str (asStr->ToUtf8());
            const so_long utf8StrLength = strlen(utf8Str.Ptr());
            this->WriteUInt32(utf8StrLength);
            const so_long sz = m_stream->Write(utf8Str.Ptr(), utf8StrLength);
            if(sz != utf8StrLength) {
                SKIZO_THROW(EC_BAD_FORMAT);
            }
        }
    } else {
        this->WriteUInt32((so_uint32)value.Type());

        // Writes the actual value.
        switch(value.Type()) {
            case E_VARIANTTYPE_NOTHING:
                // Write nothing.
                break;

            case E_VARIANTTYPE_INT:
                this->WriteInt(value.IntValue());
                break;

            case E_VARIANTTYPE_BOOL:
                this->WriteBool(value.BoolValue());
                break;

            case E_VARIANTTYPE_FLOAT:
                this->WriteFloat(value.FloatValue());
                break;

            default:
                SKIZO_THROW(EC_NOT_IMPLEMENTED);
                break;
        }
    }
}

void SBinaryWriter::Flush()
{
    m_stream->Flush();
}

} }
