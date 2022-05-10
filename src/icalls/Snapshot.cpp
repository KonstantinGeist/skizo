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

#include "../Domain.h"
#include "../FileStream.h"
#include "../RuntimeHelpers.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;

// TODO move logic outside of the icall wrapper
using namespace skizo::collections;
using namespace skizo::io;

extern "C" {

static void appendStringSlice(SFastByteBuffer& bb, const SStringSlice& slice)
{
    // The length.
    const int length = slice.End - slice.Start;
    bb.AppendBytes((so_byte*)&length, sizeof(int));

    bb.AppendBytes((so_byte*)(slice.String->Chars() + slice.Start), length * sizeof(so_char16));
}

static void doObject(void* soObj, SFastByteBuffer& bb)
{
    CClass* objClass = so_class_of(soObj);
    if(objClass->SpecialClass() != E_SPECIALCLASS_NONE) {
        CDomain::Abort("Binary blobs, closures, foreign objects, failables not supported as properties.");
    }

    // Prints the name of the class of this object.
    SStringSlice niceName (objClass->NiceName());
    appendStringSlice(bb, niceName);

    // Iterates over all properties.

    Auto<CArrayList<CProperty*> > props (objClass->GetProperties(false));

    // Prints the number of properties.
    const int propCount = props->Count();
    bb.AppendBytes((so_byte*)&propCount, sizeof(int));

    for(int i = 0; i < propCount; i++) {
        const CProperty* prop = props->Array()[i];
        CClass* propClass = prop->Getter->Signature().ReturnType.ResolvedClass;
        assert(propClass);

        // *********************************************************************
        // Prints the property name (the setter! we're preparing it for loading)
        // *********************************************************************
        appendStringSlice(bb, prop->Setter->Name());
        // *********************************************************************

        void* result = prop->Getter->InvokeDynamic(soObj, nullptr);
        if(result) {
            // With reference objects, it's a bit more complex than with valuetypes. It may be defined as "parent",
            // but the actual object assigned can be of type "subclass".
            propClass = so_class_of(result);
        }

        // Prints the class of the property (for verification).
        niceName = SStringSlice(propClass->NiceName());
        appendStringSlice(bb, niceName);

        if(propClass->SpecialClass() == E_SPECIALCLASS_BOXED) {
            // The return is a boxed class.
            bb.AppendBytes((so_byte*)(SKIZO_GET_BOXED_DATA(result)), propClass->ResolvedWrappedClass()->GCInfo().ContentSize);
        } else {
            if(result) {
                doObject(result, bb);
            } else {
                bb.AppendBytes((so_byte*)&result, sizeof(void*));
            }
        }
    }
}

void* SKIZO_API _so_Snapshot_createFromImpl(void* soObj)
{
    SKIZO_NULL_CHECK(soObj);

    const CClass* objClass = so_class_of(soObj);
    if(objClass->SpecialClass() != E_SPECIALCLASS_NONE
    || objClass->IsValueType()) {
        CDomain::Abort("Valuetypes, binary blobs, closures, foreign objects, failables not supported.");
    }

    SFastByteBuffer bb (32);

    // Empty space to inject the size header to.
    // The size header is required for ::saveToFile and ::loadFromFile
    bb.AppendBytes(0, sizeof(int));

    // Magic value...
    bb.AppendBytes((so_byte*)"SNPSH1", 6);
    // Starts appending data from the root object.
    doObject(soObj, bb);

    // After everything is ready, inject the size header.
    const int sz = (int)bb.Size();
    memcpy((void*)bb.Bytes(), (void*)&sz, sizeof(int));

    void* r = malloc(sz);
    if(!r) {
        _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
    }
    memcpy(r, bb.Bytes(), sz);

    return r;
}

void* SKIZO_API _so_Snapshot_toObjectImpl(void* pSnapshot)
{
    // TODO
    return nullptr;
}

void SKIZO_API _so_Snapshot_destroyImpl(void* pSnapshot)
{
    free(pSnapshot);
}

void SKIZO_API _so_Snapshot_saveToFileImpl(void* pSnapshot, void* soPath)
{
    SKIZO_NULL_CHECK(soPath);

    const CString* daPath = so_string_of(soPath);
    CDomain::DemandFileIOPermission(daPath);

SKIZO_GUARD_BEGIN
    Auto<CFileStream> fs (CFileStream::Open(daPath, E_FILEACCESS_WRITE));
    // The first 4 bytes is a size header
    fs->Write((const char*)pSnapshot, *((int*)pSnapshot));
SKIZO_GUARD_END
}

#define SO_CORRUPT_SNAPSHOT "Corrupt snapshot."
void* SKIZO_API _so_Snapshot_loadFromFileImpl(void* soPath)
{
    SKIZO_NULL_CHECK(soPath);

    const CString* daPath = so_string_of(soPath);
    CDomain::DemandFileIOPermission(daPath);

    char* r = nullptr;

SKIZO_GUARD_BEGIN
    Auto<CFileStream> fs (CFileStream::Open(daPath, E_FILEACCESS_READ));

    // Reads the size header.
    int sz;
    if(fs->Read((char*)&sz, sizeof(int)) != sizeof(int)) {
        CDomain::Abort(SO_CORRUPT_SNAPSHOT);
    }

    // Reads the magic value to confirm it's a snapshot file.
    char magic[7];
    if(fs->Read(magic, 6) != 6) {
        CDomain::Abort(SO_CORRUPT_SNAPSHOT);
    }
    magic[6] = 0; // null termination for 'strcmp'
    if(strcmp(magic, "SNPSH1") != 0) {
        CDomain::Abort(SO_CORRUPT_SNAPSHOT);
    }

    // Null or negative sizes aren't allowed.
    if(sz <= 0) {
        CDomain::Abort(SO_CORRUPT_SNAPSHOT);
    }

    // NOTE Allocates stuff ONLY after checking the magic value, otherwise may explode with out of memory
    // exceptions on incorrect files if the first 4 bytes happen to be a huge value.
    r = (char*)malloc(sz);
    if(!r) {
        _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
    }

    // Content size is the size minus the size header and the magic value.
    const size_t contentSize = sz - sizeof(int) - 6;
    const size_t bytesRead = fs->Read(r + sizeof(int) + 6, contentSize); // NOTE skips the headers
    if(bytesRead != contentSize) {
        CDomain::Abort(SO_CORRUPT_SNAPSHOT); // unexpected end of file
    }

    // Write the headers of the returned buffer.
    *((int*)r) = sz;
    memcpy(r + 4, magic, 6);

SKIZO_GUARD_END

    return r;
}

}

} }
