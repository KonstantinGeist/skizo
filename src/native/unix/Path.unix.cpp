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

#include "Path.h"
#include "Exception.h"
#include "String.h"
#include "StringBuilder.h"
#include "FileSystem.h"

namespace skizo { namespace io { namespace Path {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

// Intentionally makes it broken so that malformed paths like "../path/../test"
// were broken and did not allow it to pass to OS
// TODO wut
static const CString* garbled(CArrayList<const CString*>* split)
{
    Auto<CStringBuilder> sb (new CStringBuilder());
    for(int i = 0; i < split->Count(); i++) {
        const CString* fragment = split->Array()[i];

        if(!fragment->EqualsASCII("..") && !fragment->EqualsASCII("."))
            sb->Append(fragment);
    }
    return sb->ToString();
}

// Linux' realpath works only with paths that exist.
// We want "GetFullPath" to work with any paths.
const CString* GetFullPath(const CString* path)
{
    if(CString::IsNullOrEmpty(path)) {
        path->Ref();
        return path;
    }

    Auto<CArrayList<const CString*> > split (path->Split(SKIZO_CHAR('/')));
    Auto<const CString> parentDir (FileSystem::GetCurrentDirectory());

    int parentCount = 0;
    for(int i = 0; i < split->Count(); i++) {
        const CString* fragment = split->Array()[i];

        if(fragment->EqualsASCII("..")) {

            parentCount++;

            int lastIndex = parentDir->FindLastChar(SKIZO_CHAR('/'));
            if(lastIndex == -1)
                return garbled(split); // ERROR: too many ".."

            parentDir.SetPtr(parentDir->Substring(0, lastIndex));

        } else if(fragment ->EqualsASCII(".")) {
            return garbled(split); // ERROR: "." is not supported
        } else {
            break;
        }
    }

    Auto<CStringBuilder> sb (new CStringBuilder());

    sb->Append(parentDir);

    for(int i = parentCount; i < split->Count(); i++) {
        const CString* fragment = split->Array()[i];

        if(fragment->EqualsASCII(".."))
            return garbled(split); // ERROR: ".." is not at the beginning

        sb->Append(SKIZO_CHAR('/'));
        sb->Append(fragment);
    }

    return sb->ToString();
}

} } }
