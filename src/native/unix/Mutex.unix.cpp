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

#include <errno.h>
#include <assert.h>

namespace skizo { namespace core {

CMutex::CMutex()
{
    pthread_mutexattr_t mutex_attr;

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

    int r = pthread_mutex_init(&m_data, &mutex_attr);
    if(r != 0) {
        if(r == ENOMEM || r == EAGAIN) {
            SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Run out of mutexes.");
        } else {
            SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Mutex construction failed.");
        }
    }
}

void CMutex::Lock()
{
    int r = pthread_mutex_lock(&m_data);
    if(r != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

void CMutex::Unlock()
{
    int r = pthread_mutex_unlock(&m_data);
    if(r != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

CMutex::~CMutex()
{
    int r = pthread_mutex_destroy(&m_data);
    if(r != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }
}

} }
