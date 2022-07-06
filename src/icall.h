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

#ifndef ICALL_H_INCLUDED
#define ICALL_H_INCLUDED

#include "Class.h"
#include "NativeHeaders.h"

namespace skizo { namespace script {

// *******************
//   BUILT-IN TYPES.
// *******************

extern "C" {

int SKIZO_API _so_string_length(void* self);
_so_char SKIZO_API _so_string_get(void* self, int index);
void* SKIZO_API _so_string_op_add(void* self, void* other);
void* SKIZO_API _so_string_toString(void* self);
void SKIZO_API _so_string_print(void* self);
void* SKIZO_API _so_string_substring(void* self, int start, int length);
int SKIZO_API _so_string_hashCode(void* self);
_so_bool SKIZO_API _so_string_op_equals(void* self, void* other);
_so_bool SKIZO_API _so_string_equals(void* self, void* other);
void* SKIZO_API _so_string_split(void* self, void* substring);
void* SKIZO_API _so_string_toLowerCase(void* self);
void* SKIZO_API _so_string_toUpperCase(void* self);
int SKIZO_API _so_string_findSubstring(void* self, void* del, int sourceStart);
_so_bool SKIZO_API _so_string_startsWith(void* self, void* substring);
_so_bool SKIZO_API _so_string_endsWith(void* self, void* substring);
void* SKIZO_API _so_string_trim(void* self);
void SKIZO_API _so_string_dtor(void* self);

void* SKIZO_API _so_int_toString(int i);
void* SKIZO_API _so_float_toString(float f);
void* SKIZO_API _so_bool_toString(_so_bool b);
void* SKIZO_API _so_char_toString(_so_char c);
void* SKIZO_API _so_intptr_toString(void* ptr);

int SKIZO_API _so_int_hashCode(int i);
int SKIZO_API _so_float_hashCode(float f);
int SKIZO_API _so_bool_hashCode(_so_bool b);
int SKIZO_API _so_char_hashCode(_so_char c);
int SKIZO_API _so_intptr_hashCode(void* ptr);

_so_bool SKIZO_API _so_int_equals(int i, void* obj);
_so_bool SKIZO_API _so_float_equals(float f, void* obj);
_so_bool SKIZO_API _so_bool_equals(_so_bool b, void* obj);
_so_bool SKIZO_API _so_char_equals(_so_char c, void* obj);
_so_bool SKIZO_API _so_intptr_equals(void* ptr, void* obj);

void SKIZO_API _so_Range_loop(SRange range, void* loopIterObj);
void SKIZO_API _so_Range_step(SRange range, int step, void* loopIterObj);
_so_bool SKIZO_API _so_bool_then(_so_bool b, void* actionObj);
_so_bool SKIZO_API _so_bool_else(_so_bool b, void* actionObj);
void SKIZO_API _so_bool_while(void* predObj, void* actionObj);


// **********
//   Arrays
// **********

void* SKIZO_API _so_Arrays_clone(void* arr);

// ************
//   Snapshot
// ************

void* SKIZO_API _so_Snapshot_createFromImpl(void* soObj);
void SKIZO_API _so_Snapshot_destroyImpl(void* pSnapshot);
void SKIZO_API _so_Snapshot_saveToFileImpl(void* pSnapshot, void* soPath);
void* SKIZO_API _so_Snapshot_loadFromFileImpl(void* soPath);
void* SKIZO_API _so_Snapshot_toObjectImpl(void* pSnapshot);

// ***************************
//   Parse
// (extended via parse.skizo)
// ***************************

_so_bool SKIZO_API _so_int_parseImpl(void* string, int* outp);
_so_bool SKIZO_API _so_float_parseImpl(void* string, float* outp);

// ********
//   Math
// ********

float SKIZO_API _so_Math_sqrt(float f);
int SKIZO_API _so_Math_abs(int i);
float SKIZO_API _so_Math_fabs(float  v);
float SKIZO_API _so_Math_sin(float f);
float SKIZO_API _so_Math_cos(float f);
float SKIZO_API _so_Math_acos(float f);
float SKIZO_API _so_Math_fmod(float x, float y);
float SKIZO_API _so_Math_floor(float f);

float SKIZO_API _so_Math_min(float a, float b);
float SKIZO_API _so_Math_max(float a, float b);

// **************
//   StackTrace
// **************

void SKIZO_API _so_StackTrace_print();

// **********
//   Domain
// **********

// NOTE This API returns "DomainHandles" which allow to communicate with the created domains in a
// synchronized manner.
void* SKIZO_API _so_Domain_runGenericImpl(void* _source, int sourceKind, void* soPermArray);
void* SKIZO_API _so_Domain_try(void* soAction);                             // implemented in Security.cpp
//void* SKIZO_API _so_Domain_tryUntrusted(void* soPermArray, void* soAction); // implemented in Security.cpp
void SKIZO_API _so_Domain_sleep(int i);
void* SKIZO_API _so_Domain_name();
_so_bool SKIZO_API _so_Domain_isBaseDomain();
void SKIZO_API _so_Domain_listen(void* soStopPred);
_so_bool SKIZO_API _so_Domain_isTrusted();
void* SKIZO_API _so_Domain_permissions();
void SKIZO_API _so_Domain_addDependency(void* intrfc, void* impl);
void* SKIZO_API _so_Domain_getDependency(void* intrfc);
void* SKIZO_API _so_Domain_createInstance(void* intrfc);

_so_bool SKIZO_API _so_DomainHandle_isAliveImpl(void* handle);
void SKIZO_API _so_DomainHandle_dtorImpl(void* handle);
_so_bool SKIZO_API _so_DomainHandle_waitImpl(void* handle, int timeout);
void SKIZO_API _so_Domain_exportObject(void* name, void* obj);
void* SKIZO_API _so_DomainHandle_importObjectImpl(void* daHandle, void* soHandle, void* name);

// ************
//   Security
// ************

void SKIZO_API _so_Permission_demandImpl(void* soPermObj);

// ******
//   GC
// ******

void SKIZO_API _so_GC_collect();

void SKIZO_API _so_GC_addRoot(void* obj);
void SKIZO_API _so_GC_removeRoot(void* obj);

void SKIZO_API _so_GC_addMemoryPressure(int i);
void SKIZO_API _so_GC_removeMemoryPressure(int i);

_so_bool SKIZO_API _so_GC_isValidObject(void* obj);

// ***********
//   Console
// ***********

void* SKIZO_API _so_Console_readLine();

// ***************
//   Application
// ***************

void* SKIZO_API _so_Application_NEWLINE();
void SKIZO_API _so_Application_exit(int code);
void* SKIZO_API _so_Application_exeFileName();
int SKIZO_API _so_Application_processorCount();
int SKIZO_API _so_Application_tickCount();
void* SKIZO_API _so_Application_osVersion();

// ************
//   DateTime
// ************

void SKIZO_API _so_DateTime_verify(void* dt);
void SKIZO_API _so_DateTime_toLocalTimeImpl(void* src, void* dst);
void* SKIZO_API _so_DateTime_toStringImpl(void* dt);
void SKIZO_API _so_DateTime_nowImpl(void* dt);

// **********
//   Random
// **********

void* SKIZO_API _so_Random_createImpl();
void* SKIZO_API _so_Random_createFromSeedImpl(int seed);
void SKIZO_API _so_Random_destroyImpl(void* pSelf);
int SKIZO_API _so_Random_nextIntImpl(void* pSelf, int min, int max);
float SKIZO_API _so_Random_nextFloatImpl(void* pSelf);

// *************
//   Stopwatch
// *************

void* SKIZO_API _so_Stopwatch_startImpl();
int SKIZO_API _so_Stopwatch_endImpl(void* pSelf);
void SKIZO_API _so_Stopwatch_destroyImpl(void* pSelf);

// *****************
//   StringBuilder
// *****************

void* SKIZO_API _so_StringBuilder_createImpl(int cap);
void SKIZO_API _so_StringBuilder_destroyImpl(void* pSelf);
void SKIZO_API _so_StringBuilder_appendImpl(void* pSelf, void* _str);
void* SKIZO_API _so_StringBuilder_toStringImpl(void* pSelf);
int SKIZO_API _so_StringBuilder_lengthImpl(void* pSelf);
void SKIZO_API _so_StringBuilder_clearImpl(void* pSelf);

// ***********
//   Marshal
// ***********

void* SKIZO_API _so_Marshal_stringToUtf16(void* str);
void SKIZO_API _so_Marshal_freeUtf16String(void* pstr);
void* SKIZO_API _so_Marshal_stringToUtf8(void* str);
int SKIZO_API _so_Marshal_sizeOfUtf8String(void* str);
void SKIZO_API _so_Marshal_freeUtf8String(void* pstr);
void* SKIZO_API _so_Marshal_utf8ToString(void* str);
void* SKIZO_API _so_Marshal_utf16ToString(void* str);
void SKIZO_API _so_Marshal_nativeMemoryToArray(void* pArray, void* soArray);
void* SKIZO_API _so_Marshal_allocNativeMemory(int size);
void SKIZO_API _so_Marshal_freeNativeMemory(void* ptr);

void* SKIZO_API _so_Marshal_dataOffset(void* soObj);
void* SKIZO_API _so_Marshal_codeOffset(void* soObj);

void SKIZO_API _so_Marshal_copyMemory(void* dst, void* src, int size);

void* SKIZO_API _so_Marshal_offset(void* ptr, int offset);
int SKIZO_API _so_Marshal_readByte(void* ptr);
int SKIZO_API _so_Marshal_readInt(void* ptr);
float SKIZO_API _so_Marshal_readFloat(void* ptr);
void* SKIZO_API _so_Marshal_readIntPtr(void* ptr);
void SKIZO_API _so_Marshal_writeByte(void* ptr, int value);
void SKIZO_API _so_Marshal_writeInt(void* ptr, int value);
void SKIZO_API _so_Marshal_writeFloat(void* ptr, float value);
void SKIZO_API _so_Marshal_writeIntPtr(void* ptr, void* value);

int SKIZO_API _so_Marshal_pointerSize();

//int SKIZO_API _so_Marshal_invokeTest(void* v, int a, int b);

// ********
//   Type
// ********

void* SKIZO_API _so_Type_typeHandleOf(void* obj);
void* SKIZO_API _so_Type_nameImpl(void* typeHandle);

// See CClass::pRuntimeObj for more details.
void* SKIZO_API _so_Type_fromTypeHandleImpl(void* typeHandle);
void SKIZO_API _so_Type_setToTypeHandle(void* typeHandle, void* runtimeObj);

// Fills the runtime-allocated array with type handles (pTypeHandleArr) and returns the number of types.
// If pTypeHandleArr zero, returns only the number of types.
int SKIZO_API _so_Type_allTypeHandles(void* pTypeHandleArr);

void* SKIZO_API _so_Type_forNameImpl(void* pClassName);

void* SKIZO_API _so_Type_getAttributeImpl(void* typeHandle, void* pAttrName);

// Unified API to access properties of a type not to pollute the icall namespace.
_so_bool SKIZO_API _so_Type_getBoolProp(void* typeHandle, void* pName);

void* SKIZO_API _so_Type_methodsImpl(void* typeHandle, int kind);

void* SKIZO_API _so_Type_createInstanceImpl(void* typeHandle, void* pCtorName, void* args);

// getters=false => setters
void* SKIZO_API _so_Type_propertiesImpl(void* typeHandle, _so_bool getters, _so_bool isStatic);

// **********
//   Method
// **********

void* SKIZO_API _so_Method_getAttributeImpl(void* methodHandle, void* name);
void* SKIZO_API _so_Method_invokeImpl(void* methodHandle, void* thisObj, void* args);
void* SKIZO_API _so_Method_nameImpl(void* methodHandle);

/* The icalls are designed to generate zero garbage, even though they're awkward. */
/* NOTE index "-1" will return info for returnParameter */
int SKIZO_API _so_Method_getParameterCount(void* methodHandle);
void* SKIZO_API _so_Method_getParameterTypeHandle(void* methodHandle, int index);
void* SKIZO_API _so_Method_getParameterName(void* methodHandle, int index);

int SKIZO_API _so_Method_getAccessModifierImpl(void* methodHandle);

// ***************
//   FileStream
// ***************

void* SKIZO_API _so_FileStream_openImpl(void* pPath, void* pAccess);
void SKIZO_API _so_FileStream_destroyImpl(void* dFileStream);
_so_bool SKIZO_API _so_FileStream_getBoolProp(void* dFileStream, int index);
int SKIZO_API _so_FileStream_getIntProp(void* dFileStream, int index);
void SKIZO_API _so_FileStream_setIntProp(void* dFileStream, int index, int value);
int SKIZO_API _so_FileStream_readImpl(void* dFileStream, void* buffer, int count);
int SKIZO_API _so_FileStream_writeImpl(void* dFileStream, void* buffer, int count);

// **************
//   FileSystem
// **************

_so_bool SKIZO_API _so_FileSystem_fileExists(void* path);
_so_bool SKIZO_API _so_FileSystem_directoryExists(void* path);
void* SKIZO_API _so_FileSystem_currentDirectory();
void SKIZO_API _so_FileSystem_createDirectory(void* path);
void SKIZO_API _so_FileSystem_deleteDirectory(void* path);
void* SKIZO_API _so_FileSystem_listFiles(void* path, _so_bool returnFullPath);
void* SKIZO_API _so_FileSystem_listDirectories(void* path);
void* SKIZO_API _so_FileSystem_logicalDrives();
_so_bool SKIZO_API _so_FileSystem_isSameFile(void* path1, void* path2);
void SKIZO_API _so_FileSystem_copyFile(void* oldPath, void* newPath);

// ********
//   Path
// ********

void* SKIZO_API _so_Path_changeExtension(void* path, void* newExt);
void* SKIZO_API _so_Path_getExtension(void* path);
_so_bool SKIZO_API _so_Path_hasExtension(void* path, void* ext);
void* SKIZO_API _so_Path_combine(void* path1, void* path2);
void* SKIZO_API _so_Path_getDirectoryName(void* path);
void* SKIZO_API _so_Path_getFileName(void* path);
void* SKIZO_API _so_Path_getParent(void* path);
void* SKIZO_API _so_Path_getFullPath(void* path);

   // **********************************************************************************************
    //   Map
    //
    // Skizo's maps are thin wrappers around internal SkizoMapObjects written in C++ which
    // themselves are thin wrappers around Skizo's HashMaps, with some infrastructure to cache
    // classes and their methods, which allows maps with identically-typed keys to be accesed faster.
    // NOTE GC has a special knowledge of Map internals to correctly scan them for references.
    // **********************************************************************************************

struct SkizoMapObjectKey
{
    void* Key;
    int HashCode;
    FEquals EqualsMethodPtr; // Extracted and cached ::equals(any) of the key.

    SkizoMapObjectKey(int i) { } // FIX for the hashmap (TODO)
    SkizoMapObjectKey() { }
};

// *********************************
inline void SKIZO_REF(SkizoMapObjectKey& v) { }
inline void SKIZO_UNREF(SkizoMapObjectKey& v) { }
inline bool SKIZO_EQUALS(SkizoMapObjectKey& v1, SkizoMapObjectKey& v2)
{
    if(so_class_of(v1.Key) != so_class_of(v2.Key)) {
        return false;
    }

    return v1.EqualsMethodPtr(v1.Key, v2.Key);
}
inline int SKIZO_HASHCODE(SkizoMapObjectKey& v)
{
    return v.HashCode;
}
inline bool SKIZO_IS_NULL(SkizoMapObjectKey& v)
{
    return false;
}

struct SkizoMapObject
{
    // ********************************************************************************
    //   Cache.
    //
    // A map accesses a key's "hashCode" and "equals" methods for the mapping algorithm
    // to properly function. These methods are found dynamically, as a map can contain
    // keys of different types. If a map contains keys of the same type, a trick is
    // used: the map "remembers" the last used "hashCode" and "equals" methods.
    // ********************************************************************************
    mutable const CClass* KeyClassCache;
    mutable FHashCode HashCodeMethodPtr;
    mutable FEquals EqualsMethodPtr;
    // **********

    // *******************
    //   Storage itself.
    // *******************
    skizo::core::Auto<skizo::collections::CHashMap<SkizoMapObjectKey, void*> > BackingMap;

    SkizoMapObject()
        : KeyClassCache(nullptr),
          HashCodeMethodPtr(nullptr),
          EqualsMethodPtr(nullptr),
          BackingMap(new skizo::collections::CHashMap<SkizoMapObjectKey, void*>())
    {
    }
};
// *********************************

void* SKIZO_API _so_Map_createImpl();
void SKIZO_API _so_Map_destroyImpl(void* mapObj);
void* SKIZO_API _so_Map_getImpl(void* mapObj, void* key);
_so_bool SKIZO_API _so_Map_containsImpl(void* mapObj, void* key);
void SKIZO_API _so_Map_setImpl(void* mapObj, void* key, void* value);
void SKIZO_API _so_Map_removeImpl(void* mapObj, void* key);
void SKIZO_API _so_Map_clearImpl(void* mapObj);
int SKIZO_API _so_Map_sizeImpl(void* mapObj);
void SKIZO_API _so_Map_loopImpl(void* mapObj, void* mapLooper);

// *************
//   Template
// *************

void* SKIZO_API _so_Template_createImpl(void* sourceObj, void* typeObj);
void SKIZO_API _so_Template_destroyImpl(void* pSelf);
void* SKIZO_API _so_Template_renderImpl(void* pSelf, void* str);

// ************
//    Guid
// ************

void* SKIZO_API _so_Guid_generate();

}

} }

#endif // ICALL_H_INCLUDED
