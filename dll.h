#pragma once

#define UNICODE
#define _UNICODE

#include <Windows.h>

/*DLL1_EXPORTS defined by default when creating a dll project*/
#if defined DLL1_EXPORTS
#define LIBSPECS __declspec(dllexport)
#else
#define LIBSPECS __declspec(dllimport);
#endif

extern "C" {
	LIBSPECS INT Add(INT, INT);
	LIBSPECS INT Sub(INT, INT);
	LIBSPECS INT Mul(INT, INT);
	LIBSPECS INT Div(INT, INT);
}

