#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

#define MAX_LEN 100

int _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE hOut;
	DWORD nOut, n;
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInformation;
	TCHAR sendline[MAX_LEN], tempName[MAX_PATH], commandline[MAX_LEN];
	LARGE_INTEGER file_reserved, file_pos;
	OVERLAPPED ov = { 0, 0, 0, 0, NULL }, saveOv = { 0, 0, 0, 0, NULL };

	LPTSTR prog_name;

	if ((prog_name = _tcsrchr(argv[0], _T('\\'))) != NULL)
		prog_name++;	/*step past the rightmost backslash*/
	else
		prog_name = argv[0];	/*no backslashes in the name*/


	GetTempFileName(_T("."), _T("tmp"), 0, tempName);

	hOut = CreateFile(tempName, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("[%s] --- CreateFile failed\r\n"));
		return 1;
	}
	
	GetStartupInfo(&startupInfo);
	
	file_reserved.QuadPart = sizeof(sendline);

	/*first write: i need to write before the read process starts
	or it will lock the first record and the writer will be blocked*/
	_ftprintf(stdout, _T("[%s] --- > "), prog_name); fflush(stdout);

	if (_ftscanf(stdin, _T("%s"), sendline) == EOF) {
		return 0;
	}
	n = 1;
	file_pos.QuadPart = 0;
	ov.Offset = file_pos.LowPart;
	ov.OffsetHigh = file_pos.HighPart;
	LockFileEx(hOut, LOCK_EXCLUSIVE, NULL, file_reserved.LowPart, file_reserved.HighPart, &ov);
	WriteFile(hOut, sendline, sizeof(sendline), &nOut, &ov);
	

	/*start server process*/
	_stprintf(commandline, _T("server.exe %s"), tempName);

	/*I've used CREATE_NEW_CONSOLE flag to avoid output interleaving*/
	CreateProcess(NULL, commandline, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL,
		&startupInfo, &processInformation);
	CloseHandle(processInformation.hThread);	
	while (1) {
		file_pos.QuadPart = n * sizeof(sendline);
		
		saveOv = ov;
		ov.Offset = file_pos.LowPart;
		ov.OffsetHigh = file_pos.HighPart;

		/*lock next record before unlocking the following one or the reader will block it*/
		LockFileEx(hOut, LOCK_EXCLUSIVE, NULL, file_reserved.LowPart, file_reserved.HighPart, &ov);

		/*unlock previous record*/
		UnlockFileEx(hOut, NULL, file_reserved.LowPart, file_reserved.HighPart, &saveOv);

		_ftprintf(stdout, _T("[%s] --- > "), prog_name); fflush(stdout);
		if (_ftscanf(stdin, _T("%s"), sendline) == 0)
			break;
		WriteFile(hOut, sendline, sizeof(sendline), &nOut, &ov);
		n++;
	}

	/*received eof*/
	_stprintf(sendline, _T(".end"));

	/*send end string*/
	WriteFile(hOut, sendline, sizeof(sendline), &nOut, &ov);
	UnlockFileEx(hOut, NULL, file_reserved.LowPart, file_reserved.HighPart, &ov);

	/*wait for child process completion*/
	WaitForSingleObject(processInformation.hProcess, INFINITE);
	DWORD retVal;
	
	GetExitCodeProcess(processInformation.hProcess, &retVal);
	_ftprintf(stdout, _T("[%s] --- server process exited with code %d\r\n"), prog_name, retVal);

	CloseHandle(processInformation.hProcess); 
	CloseHandle(hOut);

	DeleteFile(tempName);

	return 0;
}