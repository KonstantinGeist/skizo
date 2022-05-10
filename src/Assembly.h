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

#ifndef ASSEMBLY_H_INCLUDED
#define ASSEMBLY_H_INCLUDED

#include "String.h"

namespace skizo { namespace reflection {

/**
 * Defines an assembly, which is a dynamically loadable building block of an
 * application; a library.
 */
class CAssembly final: public skizo::core::CObject
{
public:
    /**
     * Tries to load an assembly at the specified path.
     *
     * @param path The path to the assembly, or its name. The loading rules
     * depend on the platform. Do not use system-specific extensions (.dll, .so,
     * etc.): they will be appended automatically.
     *
     * @throws EC_PATH_NOT_FOUND If the assembly cannot be resolved.
     */
    static CAssembly* Load(const skizo::core::CString* path);

    virtual ~CAssembly();

    /**
     * Returns a function by name.
     *
     * @warning Cast the returned pointer to your function signature at your own
     * risk.
     * @warning The returned value may no longer be valid if the object is destroyed.
     * @note The returned value is cached.
     * @throws EC_KEY_NOT_FOUND if no function under such name can be found.
     */
    template <class T>
    inline T GetFunction(const char* name) const
    {
        return (T)getFunctionImpl(name);
    }

    /**
     * Returns a platform-independent assembly name, that is, without any platform-
     * specific extensions. Returns null if the path is not an assembly path (does
     * not conform to the platform's guidelines). For example, both "libtest.so"
     * on Linux and "test.dll" on Windows are returned as "test".
     */
    static const skizo::core::CString* GetAssemblyName(const skizo::core::CString* path);

private:
    struct AssemblyPrivate* p;

    CAssembly();
    void* getFunctionImpl(const char* name) const;
};

} }

#endif // ASSEMBLY_H_INCLUDED
