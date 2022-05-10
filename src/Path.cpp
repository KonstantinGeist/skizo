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
#include "CoreUtils.h"
#include "String.h"
#include "StringBuilder.h"

namespace skizo { namespace io {
using namespace skizo::core;

namespace Path {

static int findExtDot(const CString* path)
{
    CoreUtils::ValidatePath(path);

    const so_char16* const cs = path->Chars();
    const int length = path->Length();
    if(length == 0) { // To be sure (I'm paranoid).
        return -1;
    }

    for(int i = length - 1; i >= 0; i--) {
        const so_char16 c = cs[i];
        // The dot must be after any '/'s.
        if(c == SKIZO_CHAR('/')) {
            return -1;
        } else if(c == SKIZO_CHAR('.')) {
            // Dot can't be the last (length - 1) or the first (0). Also, the
            // extension is returned without a dot (i + 1).
            return (i == (length - 1) || i == 0)? (-1): i + 1;
        }
    }

    return -1;
}

const CString* GetExtension(const CString* path)
{
    const int i = findExtDot(path);
    if(i == -1) {
        return nullptr;
    }

    return path->Substring(i);
}

const CString* ChangeExtension(const CString* path, const CString* newExt)
{
    // Path is checked in findExtDot.

    const CString* fstPart = nullptr;
    const int i = findExtDot(path);

    // No extension -- use the whole path...
    if(i == -1) {
        fstPart = path;
    } else {
        // ... otherwise, substring the part before the actual extension.
        fstPart = path->Substring(0, i - 1);
    }

    if(newExt) {

        CoreUtils::ValidatePath(newExt);

        const CString* r;
        const CString* const constDot = CString::FromASCII("."); // NEVER THROWS.
        // newExt may start from "."
        if(newExt->StartsWith(constDot)) {
            r = fstPart->Concat(newExt);
        } else {
            r = fstPart->Concat(constDot, newExt);
        }

        if(i != -1) {
            fstPart->Unref();
        }

        constDot->Unref();
        return r;

    } else {

        if(i == -1) { // path was used, which isn't properly ref'd for return.
            fstPart->Ref();
        }

        return fstPart;

    }
}

const CString* ChangeExtension(const CString* path, const char* cNewExt)
{
    if(cNewExt) {
        Auto<const CString> daNewExt (CString::FromUtf8(cNewExt));
        return ChangeExtension(path, daNewExt);
    } else {
        return ChangeExtension(path, (const CString*)nullptr);
    }
}

bool HasExtension(const CString* path, const CString* ext)
{
    CoreUtils::ValidatePath(path);
    CoreUtils::ValidatePath(ext);

    const int dot = findExtDot(path);
    if(dot == -1) {
        return false;
    }
    if((path->Length() - dot) != ext->Length()) {
        return false;
    }

    for(int i = 0; i < ext->Length(); i++) {
        if((path->Chars()[dot + i]) != (ext->Chars()[i])) {
            return false;
        }
    }

    return true;
}

const CString* Combine(const CString* path1, const CString* path2)
{
    CoreUtils::ValidatePath(path1);
    CoreUtils::ValidatePath(path2);

    Auto<CStringBuilder> sb (new CStringBuilder());
    const so_char16 sep = SKIZO_CHAR('/');

    sb->Append(path1);

    if(!((path1->Length() > 0) && (path1->Chars()[path1->Length() - 1] == sep))
    && !((path2->Length() > 0) && (path2->Chars()[0] == sep)))
    {
        sb->Append(sep);
    }
    sb->Append(path2);

    return sb->ToString();
}

const CString* Combine(const CString* path1, const char* path2)
{
    Auto<const CString> ppath2 (CString::FromUtf8(path2));
    return Combine(path1, ppath2);
}

#ifdef SKIZO_WIN
const CString* Normalize(const CString* path)
{
    if(path->FindChar(SKIZO_CHAR('\\')) != -1) { // a quick test

       Auto<CStringBuilder> sb (new CStringBuilder());

       for(int i = 0; i < path->Length(); i++) {
            const so_char16 curChar = path->Chars()[i];

            if(curChar == SKIZO_CHAR('\\')) {
                sb->Append(SKIZO_CHAR('/'));
            } else {
                sb->Append(curChar);
            }
       }

       return sb->ToString();

    } else {

        path->Ref();
        return path;
    }
}
#else
const CString* Normalize(const CString* path)
{
    path->Ref();
    return path;
}
#endif

const CString* GetFileName(const CString* _path)
{
    CoreUtils::ValidatePath(_path);
    Auto<const CString> path (Normalize(_path));

    const int length = path->Length();
    const so_char16* const chars = path->Chars();
    int num = length;
    while (--num >= 0) {
        const so_char16 c = chars[num];
        if (c == SKIZO_CHAR('/')
        #ifdef SKIZO_WIN
         || c == SKIZO_CHAR(':') // Linux doesn't have volume separators
        #endif
        )
        {
            return path->Substring(num + 1, length - num - 1);
        }
    }

    path->Ref();
    return path;
}

const CString* GetDirectoryName(const CString* path)
{
    CoreUtils::ValidatePath(path);

    const int lastSeparator = path->FindLastChar('/');
    if(lastSeparator == -1) {
        path->Ref();
        return path;
    } else {
        return path->Substring(lastSeparator + 1);
    }
}

// NOTE inverse of GetDirectoryName
const CString* GetParent(const CString* path)
{
    CoreUtils::ValidatePath(path);

    const int lastSeparator = path->FindLastChar('/');

    // *************************************************************
    // Special case for root:
    //      the parent of "/dir1/dir2" is "/dir1"
    //
    // but
    //      the parent of "/dir1" should be "/"
    //
    // However, since lastSeparator == 0, then Substring(0, 0)
    // returns the whole string. Instead, we want to return just "/".
    if(lastSeparator == 0 || lastSeparator == -1) { // + if no '/' at all, return '/' as the root!
        return CString::FromASCII("/");
    }
    // *************************************************************

    return path->Substring(0, lastSeparator);
}

}

} }
