/*This program shows an example of overlapped I/0
	- 3 operations are always active
	- 3 read start first
	- then, we wait on a WaitForMultipleObjects which waits on events
	- if a read has been completed the read numb is incremented by ten and a write operation starts
	- if a write operation ends a new read operation starts
	- there are 2 arrays of overlapped structures: one for each possible operation
	- a matrix of event handles so that I can assign read events to the first 3 events, corresponding to the +
		first row in the matrices logic, and the write events to the last 3 events.
		In this way, I have a correspondance 1 to 1 between write and read which operate on the same
		section of file
		
*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

#define nIO 3

int _tmain(DWORD argc, LPTSTR argv[]) {
	OVERLAPPED ov_Out[nIO], ov_In[nIO];
	HANDLE hE[2][nIO], hIn;
	DWORD i, nRecords, nOut, nIn, num[nIO], n, iWaits, iWrites, nn;
	LARGE_INTEGER file_pos;
	LONG file_size;
	DWORD recSize;

	if (argc < 2) {
		_ftprintf(stderr, _T("invalid number of input parameters")); return 1;
	}

	/*create events: first 4 will be used to signal
	read completion and last 4 to signal write completion*/
	for (i = 0; i < nIO; i++) {
		hE[0][i] = CreateEvent(0, TRUE, FALSE, NULL);
		ov_In[i].hEvent = hE[0][i];

		hE[1][i] = CreateEvent(0, TRUE, FALSE, NULL);
		ov_Out[i].hEvent = hE[1][i];
	}


	hIn = CreateFile(_T("rand.bin"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_ATTRIBUTE_NORMAL, NULL);
	file_size = GetFileSize(hIn, NULL);
	recSize = sizeof(DWORD);
	nRecords = file_size / recSize;


	/*start 4 parallel read*/
	for (i = 0; i < nIO; i++) {
		file_pos.QuadPart = i * recSize;
		ov_In[i].Offset = file_pos.LowPart;
		ov_In[i].OffsetHigh = file_pos.HighPart;
		ReadFile(hIn, &num[i], sizeof(num[i]), &nIn, &ov_In[i]);
	}

	/*start async operations: at each time, at least 4 I/O operations will be running.
	A part from the last operations which could be not exactly one per thread*/


	iWaits = 0; iWrites = 0;
	while (iWaits < 2 * nRecords) {
		n = WaitForMultipleObjects(2 * nIO, hE[0], FALSE, INFINITE);
		n = n - WAIT_OBJECT_0;
		ResetEvent(hE[n / nIO][n%nIO]);
		/*if read completed*/
		if (n < nIO) {
			/*update and write*/
			num[n] = num[n] + 10;

			ov_Out[n].Offset = ov_In[n].Offset;
			ov_Out[n].OffsetHigh = ov_In[n].OffsetHigh;

			WriteFile(hIn, &num[n], sizeof(num[n]), &nOut, &ov_Out[n]);
		}
		else { /*write completed*/
			file_pos.LowPart = ov_Out[n%nIO].Offset;
			file_pos.HighPart = ov_Out[n%nIO].OffsetHigh;

			file_pos.QuadPart = file_pos.QuadPart + nIO * sizeof(DWORD);

			ov_In[n%nIO].Offset = file_pos.LowPart;
			ov_In[n%nIO].OffsetHigh = file_pos.HighPart;

			ReadFile(hIn, &num[n%nIO], sizeof(num[n%nIO]), &nIn, &ov_In[n%nIO]);

		}
		iWaits++;
	}

	CloseHandle(hIn);

	for (i = 0; i < nIO; i++) {
		CloseHandle(hE[0][i]);
		CloseHandle(hE[1][i]);

	}

	return 0;
}