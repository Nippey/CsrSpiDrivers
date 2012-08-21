#pragma once

#define WIN32_LEAN_AND_MEAN             // Selten verwendete Teile der Windows-Header nicht einbinden.
// Windows-Headerdateien:
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>


// TODO: Hier auf zusätzliche Header, die das Programm erfordert, verweisen.

#include "spifns.h"
#include "ftd2xx.h"
#include "IniReader.h"

//#define BUILD_DLL //Place this statement in your main dll-file before including this header
#ifdef BUILD_DLL
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT __declspec(dllimport)
#endif

