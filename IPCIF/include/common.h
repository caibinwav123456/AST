#ifndef _COMMON_H_
#define _COMMON_H_
#define NULL 0
#define VALID(M) ((M)==0?false:((M)==(void*)-1?((M)=0,false):true))
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
#include "ASTError.h"
#include "cmd_struct.h"
#include "defines.h"
#include "sys.h"
#ifdef WIN32
#pragma warning(disable:4996)
#ifndef DLL_IMPORT
#define DLL __declspec(dllexport)
#else
#define DLL __declspec(dllimport)
#endif
#define DLLAPI(T) DLL T __stdcall
#define main_entry WINAPI _tWinMain
#define main_args HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nShowCmd
#else
#define DLL
#define DLLAPI(T) T
#define main_entry main
#define main_args int argc, char** argv
#endif
#endif