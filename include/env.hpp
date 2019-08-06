#pragma once

#define OS_FAMILY_WINDOWS 0
#define OS_FAMILY_POSIX 1

#define OS_TYPE_WINDOWS 0
#define OS_TYPE_LINUX 1

#if defined(_WIN32) || defined(_WIN64)
    #if defined(_WIN64)
        #define SYS_ENV_64BIT
    #else
        #define SYS_ENV_32BIT
    #endif
    #define OS_FAMILY OS_FAMILY_WINDOWS
    #define OS_TYPE OS_TYPE_WINDOWS

	#ifndef UNICODE
	#define UNICODE
	#endif

	#ifndef _UNICODE
	#define _UNICODE
	#endif

	#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
	#endif

	#include <windows.h>
#endif

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__) || defined(_POSIX_VERSION)
    #define OS_FAMILY OS_FAMILY_POSIX
#endif

#if defined(__linux__) || defined(__CYGWIN__)
    #define OS_TYPE OS_TYPE_LINUX
#endif

#if defined(__GNUC__)
    #if defined(__x86_64__) || defined(__pc64__)
        #define SYS_ENV_64BIT
    #else
        #define SYS_ENV_32BIT
    #endif
#endif

#if !defined(SYS_ENV_32BIT) && !defined(SYS_ENV_64BIT)
    #error "System is not detected as either 32-bit or 64-bit"
#endif

#if !defined(OS_FAMILY)
    #error "System not supported. Only Windows and Posix systems supported right now"
#endif

#if !defined(OS_TYPE)
    #error "System not supported. Only Windows and linux systems supported right now"
#endif
