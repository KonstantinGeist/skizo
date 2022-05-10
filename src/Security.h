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

#ifndef SECURITY_H_INCLUDED
#define SECURITY_H_INCLUDED

#include "ArrayList.h"
#include "String.h"

namespace skizo { namespace script {

/**
 * This struct is to be embedded into CDomain.
 */
struct SSecurityManager
{
    SSecurityManager();

    /**
     * An untrusted domain requires permission demands to be satisfied.
     * An untrusted domain is created via Domain::runPathUntrusted(..) or Domain::runStringUntrusted(..)
     * on Skizo side.
     * Trusted domains ignore permissions. Usually, the base domain is trusted.
     * Trusted domains are created with Domain::runPath(..) or Domain::runString(..) created from
     * trusted domains or by the hosting C/C++ code. Untrusted domains CAN'T spawn trusted domains,
     * that would compromise our security model.
     */
    bool IsTrusted() const { return m_isTrusted; }
    void SetTrusted(bool value);

    /**
     * Permissions as specified by Domain::runPathUntrusted(..) or Domain::runStringUntrusted(..)
     */
    const skizo::collections::CArrayList<const skizo::core::CString*>* Permissions() const { return m_permissions; }

    void AddPermission(const skizo::core::CString* permission);

    /**
     * Demands a permission by name 'name' to be specified for this domain.
     * Implements the "Permission::demand()" feature.
     *
     * We use objects instead of names to avoid typos: we switch on names of permission classes
     * instead of string literals. If there's a mistake, the compiler will catch that.
     */
    void DemandPermission(void* soPermObj) const;
    void DemandPermission(const char* name) const;

    // ************
    //   SecureIO
    // ************

    bool IsPermissionGranted(const char* name) const;

    /**
     * For each new untrusted domain with a FileIOPermission granted, the execution engine creates a temporary
     * folder the domain is restricted to.
     * @warning Should be called inside the global domain mutex because it accesses a shared static structure.
     * @see ::SecurePath
     */
    void InitSecureIO();

    /**
     * @warning Call from inside the global mutex. Called when the domain is almost destroyed, be careful.
     */
    void DeinitSecureIO();

    /**
     * Built-in file IO icalls in the untrusted mode always check if paths they are fed are safe to work with.
     * The path parameter can be short or full.
     * Assumes the current domain (for the current thread).
     */
    void DemandFileIOPermission(const skizo::core::CString* path) const;

    /**
     * Gets the current directory in a safe manner. Always use this instead of FileSystem::GetCurrentDirectory().
     * See the implementation in Security.cpp for more information.
     * Used by _so_FileSystem_currentDirectory().
     */
    const skizo::core::CString* CurrentDirectory() const;

    /**
     * Gets the full path from a short path in a safe manner. Always use this instead of Path::GetFullPath()
     */
    const skizo::core::CString* GetFullPath(const skizo::core::CString* path) const;

    /**
     * Gets the full path to the system 'base' path in a safe manner.
     */
    const skizo::core::CString* BaseModuleFullPath() const;

private:
    bool m_isTrusted;
    skizo::core::Auto<skizo::collections::CArrayList<const skizo::core::CString*> > m_permissions;
    skizo::core::Auto<const skizo::core::CString> m_securePath;
};

// Do not call directly.
void __InitSecurity();
void __DeinitSecurity();

} }

#endif // SECURITY_H_INCLUDED
