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

#ifndef PUBLICHEADERS_H_INCLUDED
#define PUBLICHEADERS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
//   The engine is divided into two parts: private and public.
//   The private part is written in C++. The public part is a C wrapper.
//   We explicitly separate data structures of C++ and C not to allow
//   embedders too much power to shoot themselves in the foot. However,
//   certain data types can be sharable without exposing implementation
//   details. This file is included by both the private and public parts
//   of the engine.
// ***********************************************************************

#define SKIZO_API __cdecl

typedef int so_bool;
typedef void* so_string;
typedef void* so_object;

#define SKIZO_RUNTIME_VERSION 1000

#ifdef __cplusplus
}
#endif

#endif // PUBLICHEADERS_H_INCLUDED
