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

#include "Thread.h"
#include "Contract.h"
#include "Mutex.h"

namespace skizo { namespace core {

// **************
// Thread locals.
// **************

// FIX important!
static CMutex* g_tlsMutex = nullptr;
static int tlsIdCount = 0;

// ***********************************
//   INITIALIZATION/DEINITIALIZATION
// ***********************************

void __InitThread()
{
    g_tlsMutex = new CMutex();
}

void __DeinitThread()
{
    SKIZO_REQ_PTR(g_tlsMutex);

    g_tlsMutex->Unref();
    g_tlsMutex = nullptr;
}

// ***********************************

SThreadLocal::SThreadLocal()
{
    SKIZO_LOCK(g_tlsMutex)
    {
        m_id = ++tlsIdCount;
    }
    SKIZO_END_LOCK(g_tlsMutex);
}

void SThreadLocal::SetInt(int i)
{
    SVariant v;
    v.SetInt(i);
    Set(v);
}

void SThreadLocal::SetBool(int b)
{
    SVariant v;
    v.SetBool(b);
    Set(v);
}

void SThreadLocal::SetBlob(void* ptr)
{
    SVariant v;
    v.SetBlob(ptr);
    Set(v);
}

void SThreadLocal::setObject(const CObject* obj)
{
    SVariant v;
    v.SetObject(obj);
    Set(v);
}

void SThreadLocal::Set(const SVariant& v)
{
    CThread* current = CThread::Current();
    current->SetThreadLocal(m_id, v);
}

void SThreadLocal::SetNothing()
{
    SVariant v;
    Set(v);
}

SVariant SThreadLocal::Get() const
{
    CThread* current = CThread::Current();

    SVariant r;
    current->TryGetThreadLocal(m_id, &r);
    // If not found, r contains NOTHING.
    return r;
}

int SThreadLocal::IntValue() const
{
    const SVariant r = Get();
    if(r.Type() == E_VARIANTTYPE_NOTHING) {
        return 0;
    } else {
        return r.IntValue();
    }
}

int SThreadLocal::BoolValue() const
{
    const SVariant r = Get();
    if(r.Type() == E_VARIANTTYPE_NOTHING) {
        return 0;
    } else {
        return r.BoolValue();
    }
}

void* SThreadLocal::BlobValue() const
{
    const SVariant r = Get();
    if(r.Type() == E_VARIANTTYPE_NOTHING) {
        return nullptr;
    } else {
        return r.BlobValue();
    }
}

CObject* SThreadLocal::objectValue() const
{
    const SVariant r = Get();
    if(r.Type() == E_VARIANTTYPE_NOTHING) {
        return nullptr;
    } else {
        return r.ObjectValue();
    }
}

} }
