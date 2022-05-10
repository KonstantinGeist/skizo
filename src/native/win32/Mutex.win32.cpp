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

#include "../../Mutex.h"
#include "../../Exception.h"

namespace skizo { namespace core {

CMutex::CMutex()
{
    InitializeCriticalSection(&m_data); /* FIXME check */
}

void CMutex::Lock()
{
    //this->Ref(); // Makes sure the object is pinned.
    EnterCriticalSection(&m_data);
}

void CMutex::Unlock()
{
    LeaveCriticalSection(&m_data);
    //this->Unref(); // Unpins the object.
}

CMutex::~CMutex()
{
    /*
     * From MSDN: If a critical section is deleted while it is still owned,
     * the state of the threads waiting for ownership of the deleted critical
     * section is undefined.
     */
    DeleteCriticalSection(&m_data);
}

} }
