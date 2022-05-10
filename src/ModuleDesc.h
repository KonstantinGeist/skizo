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

#ifndef MODULEDESC_H_INCLUDED
#define MODULEDESC_H_INCLUDED

#include "Object.h"

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

/**
 * A module desc contains several useful pieces of information about a module which are
 * used by the runtime.
 */
class CModuleDesc: public skizo::core::CObject
{
public:
    /**
     * Where the module was located. This field is used for generating nicer errors.
     */
    const skizo::core::CString* FilePath;

    /**
     * A base module is a module defined in the SKIZO_BASE_MODULE_PATH directory.
     * Base modules are vital to the runtime, so they have several exceptions, such as:
     *   -- icalls and ecalls in base modules can be called from untrusted domains.
     */
    bool IsBaseModule;

    // TODO at least the time when the module was created + optionally some sort of a very simple CRC
    // (see Matches(CModuleDesc*)

    CModuleDesc(const skizo::core::CString* filePath, bool isBaseModule)
        : FilePath(filePath),
          IsBaseModule(isBaseModule)
    {
    }

	/**
     * Used during remoting to verify class metadata for the logically same class across different
	 * domains stem from the same module.
     */
    bool Matches(const CModuleDesc* other) const;
};

} }

#endif // MODULEDESC_H_INCLUDED
