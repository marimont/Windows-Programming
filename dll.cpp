#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include "dll.h"

extern "C" {
	LIBSPECS INT Add(INT x, INT y) {
		return x + y;
	}
	LIBSPECS INT Sub(INT x, INT y) {
		return x - y;
	}
	LIBSPECS INT Mul(INT x, INT y) {
		return x * y;
	}
	LIBSPECS INT Div(INT x, INT y) {
		if (y != 0)
			return x / y;
		else
			RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, 0, 0, NULL);
	}
}

