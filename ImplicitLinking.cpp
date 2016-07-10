/*implicit DLL linking*/

/*In order to see how implicit linking is actually effective:
	- look at executable disk size
	- run the exe
	- look at RAM occupation -> they are different because the loaded executable contains the
		dll too!

	In this case:
		- disk size = 37kB
		- RAM size = 532kB!! 
*/

#define	UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

/*to make a reference to the dll:
references -> add a reference -> projects -> select dll project
to include th  header:
project -> properties -> VC++ directories -> include directories -> edit -> add .h directory
*/

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include "dll.h"

int _tmain(DWORD argc, LPTSTR argv[]) {
	INT x, y, res;

	_ftprintf(stdout, _T("Insert to integer numbers: "));
	_ftscanf(stdin, _T("%d %d"), &x, &y);

	/*it looks like a call to a static library function!!*/
	res = Add(x, y);
	_ftprintf(stdout, _T("Result: %d\r\n"), res);

	return 0;
}