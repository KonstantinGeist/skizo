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

#include "icall.h"
#include "Domain.h"

namespace skizo { namespace script {

// NOTE the function is called after parsing metadata, so we can selectively ignore certain icalls
// if the classes that hold their Skizo descriptions were never imported.
void CDomain::registerStandardICalls()
{
    // ************
    //   Library.
    // ************

    registerICall("_so_string_length", (void*)_so_string_length);
    registerICall("_so_string_get", (void*)_so_string_get);
    registerICall("_so_string_op_add", (void*)_so_string_op_add);
    registerICall("_so_string_toString", (void*)_so_string_toString);
    registerICall("_so_string_print", (void*)_so_string_print);
    registerICall("_so_string_substring", (void*)_so_string_substring);
    registerICall("_so_string_hashCode", (void*)_so_string_hashCode);
    registerICall("_so_string_op_equals", (void*)_so_string_op_equals);
    registerICall("_so_string_equals", (void*)_so_string_equals);
    registerICall("_so_string_split", (void*)_so_string_split);

    registerICall("_so_string_toLowerCase", (void*)_so_string_toLowerCase);
    registerICall("_so_string_toUpperCase", (void*)_so_string_toUpperCase);
    registerICall("_so_string_findSubstring", (void*)_so_string_findSubstring);
    registerICall("_so_string_startsWith", (void*)_so_string_startsWith);
    registerICall("_so_string_endsWith", (void*)_so_string_endsWith);
    registerICall("_so_string_trim", (void*)_so_string_trim);

    registerICall("_so_string_dtor", (void*)_so_string_dtor);

    registerICall("_so_int_toString", (void*)_so_int_toString);
    registerICall("_so_float_toString", (void*)_so_float_toString);
    registerICall("_so_bool_toString", (void*)_so_bool_toString);
    registerICall("_so_char_toString", (void*)_so_char_toString);
    registerICall("_so_intptr_toString", (void*)_so_intptr_toString);

    registerICall("_so_int_hashCode", (void*)_so_int_hashCode);
    registerICall("_so_float_hashCode", (void*)_so_float_hashCode);
    registerICall("_so_bool_hashCode", (void*)_so_bool_hashCode);
    registerICall("_so_char_hashCode", (void*)_so_char_hashCode);
    registerICall("_so_intptr_hashCode", (void*)_so_intptr_hashCode);

    registerICall("_so_int_equals", (void*)_so_int_equals);
    registerICall("_so_float_equals", (void*)_so_float_equals);
    registerICall("_so_bool_equals", (void*)_so_bool_equals);
    registerICall("_so_char_equals", (void*)_so_char_equals);
    registerICall("_so_intptr_equals", (void*)_so_intptr_equals);

    registerICall("_so_Range_loop", (void*)_so_Range_loop);
    registerICall("_so_Range_step", (void*)_so_Range_step);
    registerICall("_so_bool_then", (void*)_so_bool_then);
    registerICall("_so_bool_else", (void*)_so_bool_else);
    registerICall("_so_bool_while", (void*)_so_bool_while);

    // Parsing
    // TODO add only if "parsing.skizo" is imported
    registerICall("_so_int_parseImpl", (void*)_so_int_parseImpl);
    registerICall("_so_float_parseImpl", (void*)_so_float_parseImpl);

    if(isClassLoaded("Arrays")) {
        registerICall("_so_Arrays_clone", (void*)_so_Arrays_clone);
    }

    if(isClassLoaded("Snapshot")) {
        registerICall("_so_Snapshot_createFromImpl", (void*)_so_Snapshot_createFromImpl);
        registerICall("_so_Snapshot_destroyImpl", (void*)_so_Snapshot_destroyImpl);
        registerICall("_so_Snapshot_saveToFileImpl", (void*)_so_Snapshot_saveToFileImpl);
        registerICall("_so_Snapshot_loadFromFileImpl", (void*)_so_Snapshot_loadFromFileImpl);
        registerICall("_so_Snapshot_toObjectImpl", (void*)_so_Snapshot_toObjectImpl);
    }

    if(isClassLoaded("Math")) {
        registerICall("_so_Math_sqrt", (void*)_so_Math_sqrt);
        registerICall("_so_Math_abs", (void*)_so_Math_abs);
        registerICall("_so_Math_fabs", (void*)_so_Math_fabs);
        registerICall("_so_Math_sin", (void*)_so_Math_sin);
        registerICall("_so_Math_cos", (void*)_so_Math_cos);
        registerICall("_so_Math_acos", (void*)_so_Math_acos);
        registerICall("_so_Math_fmod", (void*)_so_Math_fmod);
        registerICall("_so_Math_floor", (void*)_so_Math_floor);
        registerICall("_so_Math_min", (void*)_so_Math_min);
        registerICall("_so_Math_max", (void*)_so_Math_max);
    }

    if(isClassLoaded("StackTrace")) {
        registerICall("_so_StackTrace_print", (void*)_so_StackTrace_print);
    }

    if(isClassLoaded("Domain")) {
        registerICall("_so_Domain_runGenericImpl", (void*)_so_Domain_runGenericImpl);
        registerICall("_so_Domain_try", (void*)_so_Domain_try);
        //registerICall("_so_Domain_tryUntrusted", (void*)_so_Domain_tryUntrusted);
        registerICall("_so_Domain_sleep", (void*)_so_Domain_sleep);
        registerICall("_so_Domain_name", (void*)_so_Domain_name);
        registerICall("_so_Domain_isBaseDomain", (void*)_so_Domain_isBaseDomain);
        registerICall("_so_DomainHandle_dtorImpl", (void*)_so_DomainHandle_dtorImpl);
        registerICall("_so_DomainHandle_isAliveImpl", (void*)_so_DomainHandle_isAliveImpl);
        registerICall("_so_DomainHandle_waitImpl", (void*)_so_DomainHandle_waitImpl);
        registerICall("_so_Domain_exportObject", (void*)_so_Domain_exportObject);
        registerICall("_so_DomainHandle_importObjectImpl", (void*)_so_DomainHandle_importObjectImpl);
        registerICall("_so_Domain_listen", (void*)_so_Domain_listen);
        registerICall("_so_Domain_isTrusted", (void*)_so_Domain_isTrusted);
        registerICall("_so_Domain_permissions", (void*)_so_Domain_permissions);
    }

    if(isClassLoaded("GC")) {
        registerICall("_so_GC_collect", (void*)_so_GC_collect);
        registerICall("_so_GC_addRoot", (void*)_so_GC_addRoot);
        registerICall("_so_GC_removeRoot", (void*)_so_GC_removeRoot);
        registerICall("_so_GC_addMemoryPressure", (void*)_so_GC_addMemoryPressure);
        registerICall("_so_GC_removeMemoryPressure", (void*)_so_GC_removeMemoryPressure);
        registerICall("_so_GC_isValidObject", (void*)_so_GC_isValidObject);
    }

    if(isClassLoaded("Permission")) {
        registerICall("_so_Permission_demandImpl", (void*)_so_Permission_demandImpl);
    }

    if(isClassLoaded("Console")) {
        registerICall("_so_Console_readLine", (void*)_so_Console_readLine);
    }

    if(isClassLoaded("Application")) {
        registerICall("_so_Application_NEWLINE", (void*)_so_Application_NEWLINE);
        registerICall("_so_Application_exit", (void*)_so_Application_exit);
        registerICall("_so_Application_exeFileName", (void*)_so_Application_exeFileName);
        registerICall("_so_Application_processorCount", (void*)_so_Application_processorCount);
        registerICall("_so_Application_tickCount", (void*)_so_Application_tickCount);
    }

    if(isClassLoaded("DateTime")) {
        registerICall("_so_DateTime_verify", (void*)_so_DateTime_verify);
        registerICall("_so_DateTime_toLocalTimeImpl", (void*)_so_DateTime_toLocalTimeImpl);
        registerICall("_so_DateTime_toStringImpl", (void*)_so_DateTime_toStringImpl);
        registerICall("_so_DateTime_nowImpl", (void*)_so_DateTime_nowImpl);
    }

    if(isClassLoaded("Random")) {
        registerICall("_so_Random_createImpl", (void*)_so_Random_createImpl);
        registerICall("_so_Random_createFromSeedImpl", (void*)_so_Random_createFromSeedImpl);
        registerICall("_so_Random_destroyImpl", (void*)_so_Random_destroyImpl);
        registerICall("_so_Random_nextIntImpl", (void*)_so_Random_nextIntImpl);
        registerICall("_so_Random_nextFloatImpl", (void*)_so_Random_nextFloatImpl);
    }

    if(isClassLoaded("Stopwatch")) {
        registerICall("_so_Stopwatch_startImpl", (void*)_so_Stopwatch_startImpl);
        registerICall("_so_Stopwatch_endImpl", (void*)_so_Stopwatch_endImpl);
        registerICall("_so_Stopwatch_destroyImpl", (void*)_so_Stopwatch_destroyImpl);
    }

    if(isClassLoaded("StringBuilder")) {
        registerICall("_so_StringBuilder_createImpl", (void*)_so_StringBuilder_createImpl);
        registerICall("_so_StringBuilder_destroyImpl", (void*)_so_StringBuilder_destroyImpl);
        registerICall("_so_StringBuilder_appendImpl", (void*)_so_StringBuilder_appendImpl);
        registerICall("_so_StringBuilder_toStringImpl", (void*)_so_StringBuilder_toStringImpl);
        registerICall("_so_StringBuilder_lengthImpl", (void*)_so_StringBuilder_lengthImpl);
        registerICall("_so_StringBuilder_clearImpl", (void*)_so_StringBuilder_clearImpl);
    }

    if(isClassLoaded("Marshal")) {
        registerICall("_so_Marshal_stringToUtf16", (void*)_so_Marshal_stringToUtf16);
        registerICall("_so_Marshal_freeUtf16String", (void*)_so_Marshal_freeUtf16String);
        registerICall("_so_Marshal_stringToUtf8", (void*)_so_Marshal_stringToUtf8);
        registerICall("_so_Marshal_sizeOfUtf8String", (void*)_so_Marshal_sizeOfUtf8String);
        registerICall("_so_Marshal_freeUtf8String", (void*)_so_Marshal_freeUtf8String);
        registerICall("_so_Marshal_utf8ToString", (void*)_so_Marshal_utf8ToString);
        registerICall("_so_Marshal_utf16ToString", (void*)_so_Marshal_utf16ToString);
        registerICall("_so_Marshal_nativeMemoryToArray", (void*)_so_Marshal_nativeMemoryToArray);
        registerICall("_so_Marshal_offset", (void*)_so_Marshal_offset);
        registerICall("_so_Marshal_dataOffset", (void*)_so_Marshal_dataOffset);
        registerICall("_so_Marshal_codeOffset", (void*)_so_Marshal_codeOffset);
        registerICall("_so_Marshal_copyMemory", (void*)_so_Marshal_copyMemory);
        registerICall("_so_Marshal_readByte", (void*)_so_Marshal_readByte);
        registerICall("_so_Marshal_readInt", (void*)_so_Marshal_readInt);
        registerICall("_so_Marshal_readIntPtr", (void*)_so_Marshal_readIntPtr);
        registerICall("_so_Marshal_writeByte", (void*)_so_Marshal_writeByte);
        registerICall("_so_Marshal_writeInt", (void*)_so_Marshal_writeInt);
        registerICall("_so_Marshal_writeIntPtr", (void*)_so_Marshal_writeIntPtr);
        registerICall("_so_Marshal_readFloat", (void*)_so_Marshal_readFloat);
        registerICall("_so_Marshal_writeFloat", (void*)_so_Marshal_writeFloat);
        registerICall("_so_Marshal_allocNativeMemory", (void*)_so_Marshal_allocNativeMemory);
        registerICall("_so_Marshal_freeNativeMemory", (void*)_so_Marshal_freeNativeMemory);
        registerICall("_so_Marshal_pointerSize", (void*)_so_Marshal_pointerSize);

        //registerICall("_so_Marshal_invokeTest", (void*)_so_Marshal_invokeTest);
    }

    if(isClassLoaded("Type")) {
        registerICall("_so_Type_typeHandleOf", (void*)_so_Type_typeHandleOf);
        registerICall("_so_Type_nameImpl", (void*)_so_Type_nameImpl);
        registerICall("_so_Type_fromTypeHandleImpl", (void*)_so_Type_fromTypeHandleImpl);
        registerICall("_so_Type_setToTypeHandle", (void*)_so_Type_setToTypeHandle);
        registerICall("_so_Type_allTypeHandles", (void*)_so_Type_allTypeHandles);
        registerICall("_so_Type_forNameImpl", (void*)_so_Type_forNameImpl);
        registerICall("_so_Type_getAttributeImpl", (void*)_so_Type_getAttributeImpl);
        registerICall("_so_Type_getBoolProp", (void*)_so_Type_getBoolProp);
        registerICall("_so_Type_methodsImpl", (void*)_so_Type_methodsImpl);
        registerICall("_so_Type_propertiesImpl", (void*)_so_Type_propertiesImpl);
        registerICall("_so_Type_createInstanceImpl", (void*)_so_Type_createInstanceImpl);
    }

    if(isClassLoaded("Method")) {
        registerICall("_so_Method_getAttributeImpl", (void*)_so_Method_getAttributeImpl);
        registerICall("_so_Method_invokeImpl", (void*)_so_Method_invokeImpl);
        registerICall("_so_Method_nameImpl", (void*)_so_Method_nameImpl);
        registerICall("_so_Method_getParameterCount", (void*)_so_Method_getParameterCount);
        registerICall("_so_Method_getParameterTypeHandle", (void*)_so_Method_getParameterTypeHandle);
        registerICall("_so_Method_getParameterName", (void*)_so_Method_getParameterName);
        registerICall("_so_Method_getAccessModifierImpl", (void*)_so_Method_getAccessModifierImpl);
    }

    if(isClassLoaded("FileStream")) {
        registerICall("_so_FileStream_openImpl", (void*)_so_FileStream_openImpl);
        registerICall("_so_FileStream_destroyImpl", (void*)_so_FileStream_destroyImpl);
        registerICall("_so_FileStream_getBoolProp", (void*)_so_FileStream_getBoolProp);
        registerICall("_so_FileStream_getIntProp", (void*)_so_FileStream_getIntProp);
        registerICall("_so_FileStream_setIntProp", (void*)_so_FileStream_setIntProp);
        registerICall("_so_FileStream_readImpl", (void*)_so_FileStream_readImpl);
        registerICall("_so_FileStream_writeImpl", (void*)_so_FileStream_writeImpl);
    }

    if(isClassLoaded("FileSystem")) {
        registerICall("_so_FileSystem_fileExists", (void*)_so_FileSystem_fileExists);
        registerICall("_so_FileSystem_directoryExists", (void*)_so_FileSystem_directoryExists);
        registerICall("_so_FileSystem_currentDirectory", (void*)_so_FileSystem_currentDirectory);
        registerICall("_so_FileSystem_createDirectory", (void*)_so_FileSystem_createDirectory);
        registerICall("_so_FileSystem_deleteDirectory", (void*)_so_FileSystem_deleteDirectory);
        registerICall("_so_FileSystem_listFiles", (void*)_so_FileSystem_listFiles);
        registerICall("_so_FileSystem_listDirectories", (void*)_so_FileSystem_listDirectories);
        registerICall("_so_FileSystem_logicalDrives", (void*)_so_FileSystem_logicalDrives);
        registerICall("_so_FileSystem_isSameFile", (void*)_so_FileSystem_isSameFile);
        registerICall("_so_FileSystem_copyFile", (void*)_so_FileSystem_copyFile);
    }

    if(isClassLoaded("Path")) {
        registerICall("_so_Path_changeExtension", (void*)_so_Path_changeExtension);
        registerICall("_so_Path_getExtension", (void*)_so_Path_getExtension);
        registerICall("_so_Path_hasExtension", (void*)_so_Path_hasExtension);
        registerICall("_so_Path_combine", (void*)_so_Path_combine);
        registerICall("_so_Path_getDirectoryName", (void*)_so_Path_getDirectoryName);
        registerICall("_so_Path_getFileName", (void*)_so_Path_getFileName);
        registerICall("_so_Path_getParent", (void*)_so_Path_getParent);
        registerICall("_so_Path_getFullPath", (void*)_so_Path_getFullPath);
    }

    if(isClassLoaded("Map")) {
        registerICall("_so_Map_createImpl", (void*)_so_Map_createImpl);
        registerICall("_so_Map_destroyImpl", (void*)_so_Map_destroyImpl);
        registerICall("_so_Map_getImpl", (void*)_so_Map_getImpl);
        registerICall("_so_Map_containsImpl", (void*)_so_Map_containsImpl);
        registerICall("_so_Map_setImpl", (void*)_so_Map_setImpl);
        registerICall("_so_Map_removeImpl", (void*)_so_Map_removeImpl);
        registerICall("_so_Map_clearImpl", (void*)_so_Map_clearImpl);
        registerICall("_so_Map_sizeImpl", (void*)_so_Map_sizeImpl);
        registerICall("_so_Map_loopImpl", (void*)_so_Map_loopImpl);
    }
}

} }
