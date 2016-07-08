#define	UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

#define MAX_LEN 100

int _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE hIn;
	DWORD nIn, n;
	TCHAR recvline[MAX_LEN];
	LARGE_INTEGER file_reserved, file_pos;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL };

	LPTSTR prog_name = argv[0];

	if (argc < 2) {
		_ftprintf(stderr, _T("Invalid number of parameters\r\n"));
		return 1;
	}

	hIn = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("[%s] --- CreateFile of %s failed\r\n"), argv[1]);
		return 2;
	}

	file_reserved.QuadPart = sizeof(recvline);
	
	n = 0;
	DWORD keepalive = 1;
	while (keepalive) {
		file_pos.QuadPart = n * sizeof(recvline);
		ov.Offset = file_pos.LowPart;
		ov.OffsetHigh = file_pos.HighPart;

		LockFileEx(hIn, NULL, NULL, file_reserved.LowPart, file_reserved.HighPart, &ov);
		if (!ReadFile(hIn, &recvline, MAX_LEN, &nIn, &ov) || nIn == 0) {
			_ftprintf(stderr, _T("[%s] --- ReadFile failed\r\n"), prog_name);
			return 3;
		}
		_ftprintf(stdout, _T("[%s] --- received string: '%s'\r\n"), prog_name, recvline); fflush(stdout);
		if (!_tcscmp(recvline, _T(".end"))) {
			_ftprintf(stdout, _T("[%s] --- terminating\r\n"), prog_name); fflush(stdout);
			keepalive = 0;
		}
		UnlockFileEx(hIn, NULL, file_reserved.LowPart, file_reserved.HighPart, &ov);
		n++;
	}
	CloseHandle(hIn);

	ExitProcess(0);
}