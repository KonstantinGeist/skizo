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

#ifndef TEMPLATE_H_INCLUDED
#define TEMPLATE_H_INCLUDED

#include "Class.h"
#include "String.h"

namespace skizo { namespace script {

/**

 * A template allows to render a Skizo object to a string using template information
 * that's decoupled from the object itself.
 */
class CTemplate: public skizo::core::CObject
{
public:
    /**
     * Creates a template for a given class. For instance, if a rendered object has property "name",
     * then it can be referred to in the template as {name}. Property access is recursive;
     * for example, {name length} refers to the length property of the name property of the object
     * that is being rendered.
     */
    static CTemplate* CreateForClass(const skizo::core::CString* source, const CClass* klass);

    ~CTemplate();

    const skizo::core::CString* Render(void* obj) const;

private:
    explicit CTemplate(struct TemplatePrivate* p);

    struct TemplatePrivate* p;
};

} }

#endif // TEMPLATE_H_INCLUDED
