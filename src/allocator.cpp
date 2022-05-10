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

#include <cstdlib>
#include <new>
#include <cstring>
#include <cstdio>
#include <cstdint>

// For Application::TickCount() (which is guaranteed by the spec not to use
// new/delete).
#include "Application.h"

// ****************************************************
// Main hub to switch between global memory allocators.
// ****************************************************

// Choose one of the options below.

//#define SKIZO_ALLOC_SIMPLE
//#define SKIZO_ALLOC_CHECK_RATES
//#define SKIZO_ALLOC_CHECK_CONSISTENCY

// Doug Lea's allocator. For MinGW
#define SKIZO_ALLOC_DL

// ***********************************************************************

// Useful constants.

#define ALIGN_CONSTANT 16 // to preserve for SSE

#ifdef SKIZO_ALLOC_DL
extern "C" {
    #include <stdlib.h>
    #include "third-party/dlmalloc/malloc.c"
}
#endif

// ***********************************************************************

    // ********************************
    //   Default through malloc/free.
    // ********************************

#ifdef SKIZO_ALLOC_SIMPLE

    void* operator new(std::size_t sz)
    {
        void* r = std::malloc(sz);
        if(!r) {
            throw std::bad_alloc();
        }

    #ifdef SKIZO_DEBUG_MODE
        std::memset(r, 0x13, sz);
    #endif

        return r;
    }

    void operator delete(void* ptr) noexcept
    {
        std::free(ptr);
    }

#endif

// ***********************************************************************

    // **********************************
    //    Through Doug Lea's allocator.
    // **********************************

#ifdef SKIZO_ALLOC_DL
    void* operator new(std::size_t sz)
    {       
        void* r = dlmalloc(sz);
        if(!r) {
            throw std::bad_alloc();
        }

        return r;
    }

    void operator delete(void* ptr) noexcept
    {
        dlfree(ptr);
    }
#endif

// ***********************************************************************

    // **********************************************************
    //   Default through malloc/free + checks allocation rates.
    // **********************************************************

    // NOTE Prepends a header to each allocation to know what to subtract in delete(..)

#ifdef SKIZO_ALLOC_CHECK_RATES

    // Not quite thread-safe, but we do not care as it's only for diagnostics.
    static volatile std::size_t g_allocated = 0, g_deallocated = 0;
    static volatile so_long g_lastAllocTime = 0, g_lastDeallocTime = 0;

    void* operator new(std::size_t sz)
    {
        static_assert(sizeof(std::size_t) <= ALIGN_CONSTANT, "sizeof(std::size_t) <= ALIGN_CONSTANT");

        sz += ALIGN_CONSTANT; // expands to be able to fit the header

        void* r = std::malloc(sz);
        if(!r) {
            throw std::bad_alloc();
        }

    #ifdef SKIZO_DEBUG_MODE
        std::memset(r, 0x13, sz);
    #endif

        // Records the size of the allocation into the header.
        *(reinterpret_cast<std::size_t*>(r)) = sz;

        // Remembers the number of allocated bytes and checks if enough time
        // has passed (1 second) to dump the statistics to the console and start
        // everything all over.
        g_allocated += sz;
        const so_long curTime = skizo::core::Application::TickCount();
        if(g_lastAllocTime == 0) {
            g_lastAllocTime = curTime;
        } else if(curTime - g_lastAllocTime >= 1000) {
            printf("Alloc. rates (approx.): %d B/s\n", (int)(g_allocated / ((curTime - g_lastAllocTime) / 1000.0)));
            g_allocated = 0;
            g_lastAllocTime = curTime;
        }

        // Returns the offset into the beginning of the buffer without the header.
        return reinterpret_cast<uint8_t*>(r) + ALIGN_CONSTANT;
    }

    void operator delete(void* ptr) noexcept
    {
        if(!ptr) {
            return;
        }

        // User code works with an offset to the data, we need to fix it up
        // back to the header.
        uint8_t* realBuf = reinterpret_cast<uint8_t*>(ptr) - ALIGN_CONSTANT;

        // We also can restore the original size record.
        const std::size_t sz = *(reinterpret_cast<std::size_t*>(realBuf));

        // Remembers the number of deallocated bytes and checks if enough time
        // has passed (1 second) to dump the statistics to the console and start
        // everything all over.
        g_deallocated += sz;
        const so_long curTime = skizo::core::Application::TickCount();
        if(g_lastDeallocTime == 0) {
            g_lastDeallocTime = curTime;
        } else if(curTime - g_lastDeallocTime >= 1000) {
            printf("Dealloc. rates (aprox.): %d B/s\n", (int)(g_deallocated / ((curTime - g_lastDeallocTime) / 1000.0)));
            g_deallocated = 0;
            g_lastDeallocTime = curTime;
        }

        std::free(realBuf);
    }

#endif

// ***********************************************************************

#ifdef SKIZO_ALLOC_CHECK_CONSISTENCY

    // **********************************************************
    //   Default through malloc/free + checks heap consistency.
    // **********************************************************
    
    // ***************************************************************
    // Useful to test how the application restores from OOM scenarios.
    //#define SKIZO_TEST_OUT_OF_MEMORY
    // ***************************************************************

    // NOTE Appends and prepends additional headers which are checked
    // in delete(..) for under- and overflows.

    #define PREHEADER_MAGIC 0xf1
    #define POSTHEADER_MAGIC 0xf2
    #define PSEUDO_CORRUPTION_MAGIC 0xa5

    #ifdef SKIZO_TEST_OUT_OF_MEMORY
        static std::size_t g_allocated = 0;
    #endif

    // TODO post-header isn't aligned, may crash on CPU's which do not support
    // non-aligned access

    void* operator new(std::size_t sz)
    {       
        static_assert(sizeof(std::size_t) <= ALIGN_CONSTANT, "sizeof(std::size_t) <= ALIGN_CONSTANT");

        sz += ALIGN_CONSTANT * 2; // expands to be able to fit both the pre-header
                                  // and the post-header

        void* r = std::malloc(sz);

    #ifdef SKIZO_TEST_OUT_OF_MEMORY
        if((g_allocated + sz) > (300 * 1024 * 1024)) { // 500 MB
            printf("Memory limit (SKIZO_TEST_OUT_OF_MEMORY defined).\n");
            exit(1); // fail fast
        }
    #endif

        if(!r) {
            throw std::bad_alloc();
        }

    #ifdef SKIZO_TEST_OUT_OF_MEMORY
        g_allocated += sz;
    #endif

        // Fills the pre-header and the post-header with magic values. They will
        // be checked in delete(..) for consistency.
        std::memset(r, 
                    PREHEADER_MAGIC,
                    ALIGN_CONSTANT);
        std::memset(reinterpret_cast<uint8_t*>(r) + sz - ALIGN_CONSTANT,
                    POSTHEADER_MAGIC,
                    ALIGN_CONSTANT);
        // Additionally, we have to record the size information so that we could
        // figure out the position of the post-header inside delete(..)
        *(reinterpret_cast<std::size_t*>(r)) = sz;

        // Returns the offset into the beginning of the buffer without the pre-header.
        return reinterpret_cast<uint8_t*>(r) + ALIGN_CONSTANT;
    }

    void operator delete(void* ptr) noexcept
    {
        if(ptr) {

            // User code works with an offset to the data, we need to fix it up back
            // to the header.
            uint8_t* realBuf = reinterpret_cast<uint8_t*>(ptr) - ALIGN_CONSTANT;

            uint8_t* preHeader = realBuf;

            // Restores the original size record.
            std::size_t sz = *(reinterpret_cast<std::size_t*>(preHeader));

            // Let's see now if the pre-header is intact.
            // NOTE that we had to store size information as well, so the preheader
            // is smaller than the postheader.
            for(int i = sizeof(std::size_t); i < ALIGN_CONSTANT; i++) {
                if(preHeader[i] != PREHEADER_MAGIC) {
                    printf("memory corruption in preheader of '%p': content: %x\n", ptr, preHeader[i]);
                    break;
                }
            }

            uint8_t* postHeader = realBuf + sz - ALIGN_CONSTANT;

            // Let's see now if the post-header is intact.
            for(int i = 0; i < ALIGN_CONSTANT; i++) {
                if(postHeader[i] != POSTHEADER_MAGIC) {
                    printf("memory corruption in postheader of '%p': content: %x\n", ptr, postHeader[i]);
                    break;
                }
            }

            // Corrupts the memory block to detect cases when memory is freed twice.
            std::memset(realBuf, PSEUDO_CORRUPTION_MAGIC, sz);

            std::free(realBuf);

        #ifdef SKIZO_TEST_OUT_OF_MEMORY
            g_allocated -= sz;
        #endif
        }
    }

#endif
