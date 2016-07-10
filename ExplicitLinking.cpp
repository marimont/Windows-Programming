/*explicit DLL linking*/

/*
*/

#define	UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

typedef INT (*Add)(INT, INT);

int _tmain(DWORD argc, LPTSTR argv[]) {
	INT x, y, res;
	HINSTANCE lib;
	Add addFunc;

	_ftprintf(stdout, _T("Insert to integer numbers: "));
	_ftscanf(stdin, _T("%d %d"), &x, &y);

	/*load library*/
	lib = LoadLibrary(_T("DLL1.dll"));
	if (lib == NULL) {
		_ftprintf(stderr, _T("LoadLibary failed\r\n")); return 1;
	}
	/*get function entry point address*/
	addFunc = (Add)GetProcAddress(lib, "Add");
	res = addFunc(x, y);
	FreeLibrary(lib);

	_ftprintf(stdout, _T("Result: %d\r\n"), res);

	return 0;
}