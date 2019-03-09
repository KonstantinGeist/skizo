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

#ifndef SKIZOCORE_H_INCLUDED
#define SKIZOCORE_H_INCLUDED

/*
 * Before using the runtime, it should be initialized (for a given EXE/DLL).
 * Initialization is done explicitly to avoid static constructor order fiasco
 * (the order is poorly definable and may differ from compiler to compiler).
 */

namespace skizo { namespace core {

/**
 * Initializes the runtime's per-process  structures.
 */
void InitSkizo();

/**
 * Deinitializes the runtime's per-process  structures.
 *
 * @warning All known skizo threads should be aborted and joined to make sure
 * none of them are active during this call to prevent potential crashes on exit.
 */
void DeinitSkizo();

/**
 * Tells whether InitSkizo() was called. Useful to check for initialization
 * problems (used in the constructor of CObject in debug mode).
 */
bool IsSkizoInitialized();

} }

#endif // SKIZOCORE_H_INCLUDED
