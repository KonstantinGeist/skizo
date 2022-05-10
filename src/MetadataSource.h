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

#ifndef METADATASOURCE_H_INCLUDED
#define METADATASOURCE_H_INCLUDED

namespace skizo { namespace script {

/**
 * Where this metadata was described: which module and what line number in the module.
 */
struct SMetadataSource
{
    class CModuleDesc* Module;
    int LineNumber;

    SMetadataSource()
        : Module(nullptr),
          LineNumber(0)
    {
    }
};

} }

#endif // METADATASOURCE_H_INCLUDED
