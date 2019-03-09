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

#ifndef BASEDEFS_H_INCLUDED
#define BASEDEFS_H_INCLUDED

#include <new>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <atomic>

#ifdef SKIZO_WIN
  #define UNICODE
  #define _UNICODE
  #define WINVER 0x0501
  #define WIN32_LEAN_AND_MEAN // Excludes winsock v1 and a lot of other garbage.
  #include <windows.h>

  // FIX for O3+vectorization, non-16-byte aligned stack (which can happen on
  // Windows) can wreck havoc when using SSE. Used mostly for OS callbacks.
  #define SKIZO_FORCE_ALIGNMENT __attribute__((force_align_arg_pointer))
#endif

#ifdef SKIZO_X
  // Linux complains about it. It doesn't matter, as __cdecl is by default on Linux.
  #define __cdecl

  // No need to force alignment under Linux.
  #define SKIZO_FORCE_ALIGNMENT
#endif

// ************************************
//   System-indepedent integer values.
// ************************************

typedef unsigned char so_char8;
typedef unsigned char so_byte;
typedef uint16_t so_char16;
typedef uint16_t so_uint16;
typedef uint32_t so_uint32;
typedef int64_t so_long;
typedef uint64_t so_uint64;

// ************************************

/**
 * The size of boolean is not stable across frameworks. We standardize it to be
 * same as "int" for better interop with other languages such as C#.
 */
typedef int so_bool; // For interop.

/**
 * The minimum allowed in32 value.
 */
#define SKIZO_INT32_MIN (-2147483647 - 1)

/**
 * The maximum allowed in32 value.
 */
#define SKIZO_INT32_MAX 2147483647

/**
 * The maximum allowed in16 value.
 */
#define SKIZO_UINT16_MAX 65535

/**
 * Atomic int.
 */
typedef std::atomic_int so_atomic_int;

/**
 * Converts a C literal to a skizo char16 literal.
 */
#define SKIZO_CHAR(x) ((so_char16)(L ## x)) // FIXME warn about data loss?

#endif // BASEDEFS_H_INCLUDED
