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

#include "../../CoreUtils.h"
#include "../../Exception.h"
#include "../../String.h"
#include "../../Thread.h"
#include "../../WaitObject.h"

#ifdef SKIZO_WIN
    #include <shlwapi.h>
#endif

namespace skizo { namespace core { namespace CoreUtils {

void ThrowWin32Error()
{
    DWORD winCode = GetLastError();
    EExceptionCode daCode;

    switch(winCode) {
        case ERROR_SUCCESS:
            daCode = EC_OK;
            break;

        case ERROR_PATH_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_NOT_READY:
            daCode = EC_PATH_NOT_FOUND;
            break;

        case ERROR_INVALID_DRIVE:
        case ERROR_INVALID_DATA:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_PATHNAME:
            daCode = EC_ILLEGAL_ARGUMENT;
            break;

        case ERROR_BAD_FORMAT:
            daCode = EC_BAD_FORMAT;
            break;

        case ERROR_NOT_ENOUGH_MEMORY:
            daCode = EC_OUT_OF_RESOURCES;
            break;

        default:
            daCode = EC_PLATFORM_DEPENDENT;
            break;
    }

    if(daCode != EC_OK) {
        ThrowHelper(daCode, nullptr, "", 0);
    }
}

// The WinAPI version uses the Windows Shell implementation as it
// supports globalization and makes the output look just like
// in Windows Explorer.
const CString* MemorySizeToString(so_long sz)
{
    if(sz < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    WCHAR cs[64]; // must suffice
    StrFormatByteSizeW((LONGLONG)sz, cs, 64);

    return CString::FromUtf16((so_char16*)cs);
}

    // ********************
    //     ShowMessage
    // ********************

class CShowMessageThread: public CThread
{
public:
    Auto<const CString> Message;
    Auto<CWaitObject> WaitObject;

    virtual void OnStart() override
    {
        MessageBoxW(NULL, (LPCTSTR)this->Message->Chars(), L"Fatal Error", MB_OK | MB_ICONERROR);

        // Notify the original thread that the message box is closed.
        this->WaitObject->Pulse();
    }
};

void ShowMessage(const CString* msg, bool isFatal)
{
    if(isFatal) {
        // NOTE A fatal message box has to be in a separate thread, because
        // MessageBox pumps messages, which will pump it for a dead HWND-object
        // which just crashed as well. The HWND object exists in a partial,
        // corrupt state and its messages should not be pumped. (Or who knows
        // how else the current stack is composited.)
        Auto<CShowMessageThread> showMsgThread (new CShowMessageThread());
        showMsgThread->Message.SetVal(msg);
        showMsgThread->WaitObject.SetPtr(new CWaitObject());
        showMsgThread->Start();

        // The crashed thread waits until the user closes this message box.
        CThread::Wait(showMsgThread->WaitObject);
    } else {
        MessageBoxW(NULL, (LPCTSTR)msg->Chars(), L"Message", MB_OK | MB_ICONINFORMATION);
    }
}

} } }
