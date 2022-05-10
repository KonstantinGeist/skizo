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

#include "../../WaitObject.h"
#include "../../CoreUtils.h"
#include <assert.h>

namespace skizo { namespace core {

CWaitObject::CWaitObject(bool initialState, bool resetAutomatically)
{
    m_handle = CreateEvent(NULL,   // default security attributes
                           resetAutomatically? FALSE: TRUE,
                           initialState? TRUE: FALSE,
                           NULL);  // unnamed object
    if(!m_handle) {
        CoreUtils::ThrowWin32Error();
    }
}

CWaitObject::~CWaitObject()
{
    CloseHandle(m_handle);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
void CWaitObject::Pulse()
{
    BOOL r = SetEvent(m_handle); // MSDN: "Setting an event that is already set has no effect."
    assert(r);                   //       So it's OK.
}
#pragma GCC diagnostic pop

} }
