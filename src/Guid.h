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

#ifndef GUID_H_INCLUDED
#define GUID_H_INCLUDED

#include "String.h"

namespace skizo { namespace core { namespace Guid {

/**
 * Creates a GUID, a unique 128-bit integer.
 */
const CString* NewGuid();

} } }

#endif // GUID_H_INCLUDED
