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

#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED

// IMPORTANT NOTE
// Under Windows, compile the project with TDM-MinGW 4.8.x, the rest of releases handle exceptions in a
// thread-unsafe manner (Skizo aborts rely on the C++ exception mechanism).
// To use SoAbortDomainException, the compiler must use SJLJ exceptions.

/**
 * Once defined, this macro allows to find memory leaks with little overhead.
 * The purpose of this mode is to find if any leakage actually takes place without
 * using heavy tools (the leak detector makes use of a simple atomic integer).
 */
#ifdef SKIZO_DEBUG_MODE
    #define SKIZO_BASIC_LEAK_DETECTOR
#endif

/**
 * Commonly used by skizo collections as a threshold to resize the internal array:
 * if the count of objects is equal to or bigger than 0.75f * capacity, the array
 * is resized.
 */
#define SKIZO_LOAD_FACTOR 0.75f

/**
 * If this option is enabled, skizo collections will perform an addition check
 * during inserts/removals: if a collection is modified while it's being iterated,
 * it will throw an exception (as concurrent modification can easily corrupt data).
 */
#define SKIZO_COLLECTIONS_MODCOUNT

/**
 * Can be redefined to make Ref() and Unref() virtual. It can make it easier to
 * debug reference counting problems as we can override subclasses' Ref()/Unref()
 * and check access only to specific classes if we know they're to blame.
 */
#define SKIZO_VIRTUAL_REF

/**
 * Should be set for debug mode. Set from CMakeLists.txt
 */
//#define SKIZO_DEBUG_MODE

/**
 * This option can be enabled to make the Ref/Unref single-threaded: it would
 * use a non-atomic reference count (except for thread-related objects) which
 * speeds up things for most cases. Set from CMakeLists.txt
 */
//#define SKIZO_SINGLE_THREADED

#define SKIZO_BASE_MODULE_PATH "base"
#define SKIZO_SECURE_PATH "secure"

/**
 * @see SMemoryManager::MinGCThreshold
 */
#define SKIZO_MIN_GC_THRESHOLD (5 * 1024)

#endif // OPTIONS_H_INCLUDED
