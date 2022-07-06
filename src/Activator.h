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

#ifndef ACTIVATOR_H_INCLUDED
#define ACTIVATOR_H_INCLUDED

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {
class CClass;
class CDomain;

/**
 * The activator manages dependencies and allows to create object instances through built-in
 * dependency injection.
 */
struct SActivator
{
public:
    explicit SActivator(CDomain* domain);
    ~SActivator();

    /**
     * Tells the activator that references to the given interface should be resolved by the given
     * class. Names are passed instead of class objects so that Skizo code didn't have to reference
     * reflection. Aborts if there are no classes with such names; or the given class doesn't actually
     * implement the given interface; or there's already an implementation registered for the given interface.
     */
    void AddDependency(const skizo::core::CString* interfaceName, const skizo::core::CString* implName);
    void AddDependency(CClass* interface, CClass* impl);

    /**
     * Creates an instance of the given class, which can be an interface or a concrete class.
     * 
     * If the given class is an interface, then the call is similar to ::GetDependency, except the instance
     * is not cached (a new instance is returned every time). If it's a concrete class, then a concrete class instance
     * is returned.
     * All parameters in this constructor must be depedency interfaces. Otherwise, the method aborts.
     */
    void* CreateInstance(const skizo::core::CString* className) const;
    void* CreateInstance(CClass* klass) const;

    /**
     * Instantiates or retrieves an already instantiated dependency, returns a GC-allocated Skizo object.
     * For the idea how the object is constructed, see ::CreateInstance
     * Note that the object is GC-rooted and is a essentially a singleton (cached/reused).
     * Aborts if some of the dependencies of the object cannot be resolved.
     * This method is useful for configurating dependencies.
     */
    void* GetDependency(const skizo::core::CString* interfaceName) const;
    void* GetDependency(CClass* interface) const;

private:
    struct ActivatorPrivate* p;
};

} }

#endif // ACTIVATOR_H_INCLUDED
