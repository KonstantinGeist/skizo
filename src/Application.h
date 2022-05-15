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

#ifndef APPLICATION_H_INCLUDED
#define APPLICATION_H_INCLUDED

#include "basedefs.h"

namespace skizo { namespace core {
class CString;

/**
 * Defines often-used applications.
 */
enum EApplication
{
    /**
     * Describes this application. Use together with skizo::core::Application::Launch
     * to launch other instances of this application.
     */
    E_APPLICATION_THIS = 0
};

/**
 * Describes a platform-specific string.
 */
enum EPlatformString
{
    /**
     * Platform-specific newline character(s): "\n" for Linux, "\r\n" for Windows,
     * etc.
     */
    E_PLATFORMSTRING_NEWLINE = 0,

    /**
     * Platform-specific file separator character(s): "\" for Windows, "/" for
     * Linux, etc.
     */
    E_PLATFORMSTRING_FILE_SEPARATOR = 1
};

/**
 * Describes platform-specific folders.
 */
enum ESpecialFolder
{
    /**
     * The path to this application's application data (settings, temporary data, etc.)
     */
    E_SPECIALFOLDER_APPDATA = 0,

    /**
     * The path to this user's 'home' folder.
     */
    E_SPECIALFOLDER_HOME = 1
};

/**
 * Options for Application::Launch(..)
 */
struct SLaunchOptions
{
    /**
     * The new process inherits console.
     */
    bool InheritConsole;

    /**
     * Waits for the application to exit before proceding.
     */
    bool WaitForExit;

    SLaunchOptions()
        : InheritConsole(false),
          WaitForExit(false)
    {
    }
};

/**
 * Provides methods and properties to manage the application, such as methods to
 * stop the application, and properties to get information about the application.
 */
namespace Application {

/**
 * Terminates the currently running process. The argument serves as a status code;
 * by convention, a nonzero status code indicates abnormal termination.
 *
 * @param code status code
 */
void Exit(int code);

/**
 * Prints a message and quickly terminates the currently running process.
 */
void FailFast(const char* msg);

/**
 * Gets the filename of the currently running executable. The returned
 * string can be used to spawn another instance of the running program.
 */
const CString* GetExeFileName();

/**
 * Gets the current application's memory usage for diagnostic purposes.
 */
so_long GetMemoryUsage();

/**
 * Gets the number of milliseconds elapsed since a platform-dependent
 * epoch.
 *
 * @warning Implementations should guarantee not to call new/delete as this
 * method may be used in memory allocators to check allocation rates.
 */
so_long TickCount();

/**
 * Returns platform-specific strings.
 */
const CString* PlatformString(EPlatformString ps);

/**
 * Launches predefined applications.
 *
 * @param app the predefined app
 * @param args
 */
void Launch(EApplication app, const CString* args);

/**
 * Launches the application with the specified path and arguments. Uses the default
 * constructor of SLaunchOptions (does not wait for exit; may create a new window).
 * Arguments can be null.
 */
void Launch(const CString* path, const CString* args);

/**
 * Launches the application with the specified path and arguments. An extended
 * version which allows options.
 */
void Launch(const CString* path, const CString* args, const SLaunchOptions& options);

/**
 * Returns the number of processors on the current machine.
 */
int GetProcessorCount();

/**
 * Returns a list of the program's command line arguments (not including the
 * program name).
 * Returns an empty string if nothing was passed.
 */
const CString* GetCommandLineArgs();

/**
 * Gets the path to the system special folder that is identified by the
 * specified enumeration.
 */
const CString* GetSpecialFolder(ESpecialFolder specialFolder);

/**
 * Retrieves the name of the OS, for purely debugging purposes. The exact nature
 * of the return string highly depends from OS to OS.
 */
const CString* GetOSVersion();

}

// WARNING Do not call directly.
void __InitApplication();
void __DeinitApplication();

} }

#endif // APPLICATION_H_INCLUDED
