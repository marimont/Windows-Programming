/*Simple program to test explicit heap management.

This program reads a binary file and orders it.

The file is read in one-shot - i.e. a data structure, which is a pointer
to record_t data, of the size of the file is allocated and a ReadFile stores
the whole file into this structure.

In its turn, this data structure is allocated into one dedicated heap. 
In this case it is not compulsory to have a dedicate heap, but in principle, it could
be used to allocate fixed size data (records) and this provides advantages, as we know.

A qsort is performed and memory freed and released.
*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>


typedef struct {
	DWORD num;
} record_t;


int CompareInt(const void *p1, const void *p2) {
	record_t *x = (record_t*)p1;
	record_t *y = (record_t*)p2;

	return (*x).num - (*y).num;
}


int _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE SortHeap, hIn;
	record_t *records;
	DWORD nIn, i;
	DWORD file_size, rec_size, recn;

	if (argc < 2) {
		_ftprintf(stderr, _T("invalid number of input parameters\r\n")); return 1;
	}

	hIn = CreateFile(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("CreateFile() failed\r\n")); return 2;
	}

	file_size = GetFileSize(hIn, NULL);
	rec_size = sizeof(record_t);
	recn = file_size / rec_size;

	SortHeap = HeapCreate(0, 0, MAXIMUM_ALLOWED);
		/*it will store the entire file with one single call to ReadFile */
	records = (record_t*) HeapAlloc(SortHeap, 0, file_size + sizeof(TCHAR));
	if (!ReadFile(hIn, records, file_size, &nIn, NULL)) {
		_ftprintf(stderr, _T("ReadFile() failed\r\n")); return 3;
	}
	CloseHandle(hIn);

	qsort(records, recn, rec_size, CompareInt);

	for (i = 0; i < recn; i++) {
		_ftprintf(stdout, _T("%d "), records[i].num);
	}

	HeapFree(SortHeap, 0, records);
	HeapDestroy(SortHeap);

	return 0;
}