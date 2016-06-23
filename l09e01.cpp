/*Explanatory comments are provided next to crucial code fragments*/


#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

/*argument structure that allows me to pass to a thread function a pointer to it 
so that it can retrieve the filename and, at the same way, return to the main thread 
the allocated and sorted array and its size
*/

typedef struct {
	LPTSTR filename;
	INT size;
	INT *array;
}arg_t;

INT myMerge(INT* L, INT *R, INT n1, INT n2) {		//the leftpart is always the  return array 
	INT i, j, k, *vet, N;
	N = n1 + n2;
	vet = (INT*)malloc(N * sizeof(INT));	
	i = 0;
	j = 0;
	k = 0;
	while (i < n1 && j < n2) {
		if (L[i] <= R[j])
			vet[k++] = L[i++];
		else
			vet[k++] = R[j++];
	}
	// Copy the remaining elements of L[]
	while (i < n1)
		vet[k++] = L[i++];
	// Copy the remaining elements of R[]
	while (j < n2)
		vet[k++] = R[j++];

	for (i = 0; i < N; i++)
		L[i] = vet[i];

	return N;								//so I return  the  size of the ordered array;
}

VOID merge(INT *vet, INT left, INT middle, INT right) {
	INT i, j, k;
	INT n1 = middle - left + 1;
	INT n2 = right - middle;
	INT *L, *R;  // temp arrays

	L = (INT*)malloc(n1 * sizeof(INT));
	R = (INT*)malloc(n2 * sizeof(INT));

	for (i = 0; i <= n1; i++)  // make a copy
		L[i] = vet[left + i];
	for (j = 0; j <= n2; j++)
		R[j] = vet[middle + 1 + j];

	// Merge the temp arrays in vet[l..r]
	i = 0;
	j = 0;
	k = left; // Initial index of merged subarray
	while (i < n1 && j < n2) {
		if (L[i] <= R[j])
			vet[k++] = L[i++];
		else
			vet[k++] = R[j++];
	}
	// Copy the remaining elements of L[]
	while (i < n1)
		vet[k++] = L[i++];
	// Copy the remaining elements of R[]
	while (j < n2)
		vet[k++] = R[j++];
}

VOID mergeSort(INT *vet, INT left, INT right) {
	INT middle;

	if (left < right) {
		middle = left + (right - left) / 2;  // Same as (left + right)/2, but avoids overflow for large l and r

		mergeSort(vet, left, middle);
		mergeSort(vet, middle + 1, right);

		merge(vet, left, middle, right);
	}
}

DWORD WINAPI sortThread(PVOID args) {

	HANDLE hIn;
	DWORD nIn;
	INT n, i;
	arg_t *arguments = (arg_t*)args;
	LPTSTR filename = (*arguments).filename;

	hIn = CreateFile(filename, FILE_READ_ACCESS, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Can't open file\n")); ExitThread(1);
	}
	ReadFile(hIn, &n, sizeof(n), &nIn, NULL);
	if (nIn < 0) {
		_ftprintf(stderr, _T("Can't read elements num\n")); ExitThread(2);
	}
	(*arguments).size = n;
	(*arguments).array = (INT*)malloc(n * sizeof(INT));
	for (i = 0; i < n; i++) {
		ReadFile(hIn, &(*arguments).array[i], sizeof(INT), &nIn, NULL);
	}

	mergeSort((*arguments).array, 0, n - 1);

	ExitThread(0);
}

INT _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE *threads;
	INT tid, i, NTOT = 0;
	DWORD *threadIds;
	arg_t *arguments;
	INT *resArray;
	HANDLE hOut;
	DWORD nOut;

	INT nthreads = argc - 2;

	threads = (HANDLE*)malloc(nthreads * sizeof(HANDLE));
	threadIds = (DWORD*)malloc(nthreads * sizeof(DWORD));
	arguments = (arg_t*)malloc(nthreads * sizeof(arg_t));

	for (tid = 0; tid < nthreads; tid++) {
		arguments[tid].filename = argv[tid + 1];
		threads[tid] = CreateThread(0, 0, sortThread, &arguments[tid], 0, &threadIds[tid]);
	}
	WaitForMultipleObjects(nthreads, threads, TRUE, INFINITE);
	for (tid = 0; tid < nthreads; tid++)
		CloseHandle(threads[tid]);

	for (tid = 0; tid < nthreads; tid++) 
		NTOT = NTOT + arguments[tid].size;
	resArray = (INT*)malloc(NTOT * sizeof(INT));
	INT size = 0;
	/*At each iteration I merge the resArray with one of the ordered arrays
	- at first iteration the first array will be completely copied into resArray 
	- resArray size is returned
	- at following iterations the resArray is gonna be merged with other arrays 
	- the size is updated anytime*/
	for (tid = 0; tid < nthreads; tid++) 
		size = myMerge(resArray, arguments[tid].array, size, arguments[tid].size);


	hOut = CreateFile(argv[argc - 1], GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Cannot open input file. Error: %x\n"), GetLastError());
		ExitThread(1);
	}
	WriteFile(hOut, &NTOT, sizeof(INT), &nOut, NULL);
	for(i = 0; i < NTOT; i++)
		WriteFile(hOut, &resArray[i], sizeof(INT), &nOut, NULL);
	CloseHandle(hOut);

	hOut = CreateFile(argv[argc - 1], GENERIC_READ, FILE_SHARE_READ, NULL,			//i should provide new permissions and rewind the handler
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Cannot open input file. Error: %x\n"), GetLastError());
		ExitThread(2);
	}

	/*print loop to check the correctness of the produced binary file*/

	INT x;
	while (ReadFile(hOut, &x, sizeof(x), &nOut, NULL) && nOut > 0) {
		_ftprintf(stdout, _T("%d "), x); fflush(stdout);
	}
	_ftprintf(stdout, _T("\n")); fflush(stdout);

	for (i = 0; i < nthreads; i++) {
		free(arguments[i].array);
	}
	free(arguments);
	free(threads);
	free(threadIds);
	

	CloseHandle(hOut);
	ExitThread(0);
}
