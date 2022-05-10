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

#ifndef ABORT_H_INCLUDED
#define ABORT_H_INCLUDED

// WARNING To use SoAbortDomainException, the C++ compiler must use SJLJ exceptions.
// SJLJ is theoretically safer when unwinding across machine-generated stackframes.
#define SKIZO_USE_SJLJ_ABORT_EXCEPTION

#ifdef SKIZO_USE_SJLJ_ABORT_EXCEPTION

    /**
     * This C++ exception is used to unwind the stack and return back to CDomain::InvokeEntryPoint() if Skizo code raises
     * an error, be it a runtime error or an explicit "abort" expression.
     */
    class SoDomainAbortException
    {
    public:
        char* Message;

        explicit SoDomainAbortException(char* msg)
            : Message(msg)
        {
        }
    };

    /**
     * Locks aware of SoDomainAbortException. The standard SKIZO_LOCK and SKIZO_END_LOCK don't "see" SoDomainAbortExceptions.
     */
    #define SKIZO_LOCK_AB(mu) (mu)->Lock(); try {
    #define SKIZO_END_LOCK_AB(mu) } catch(...) { (mu)->Unlock(); throw; } (mu)->Unlock()
    #define SKIZO_END_LOCK_AB_NOEXCEPT(mu) } catch(...) {} (mu)->Unlock()

#else // NO SKIZO_USE_SJLJ_ABORT_EXCEPTION

    // TODO
    #error "Not implemented."

#endif

#endif // ABORT_H_INCLUDED
