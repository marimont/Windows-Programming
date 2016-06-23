/*Explanatory comments are provided next to crucial code segments*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

/*argument structure that allows me to pass to a thread function a poDWORDer to it
so that it can retrieve the filename and, at the same way, return to the main thread
the allocated and sorted array and its size
*/


typedef struct {
	LPTSTR filename;
	DWORD size; 
	DWORD *array;
	DWORD index;
}arg_t;

DWORD find_pos(arg_t *args, DWORD n, DWORD tid) {
	DWORD i;
	for (i = 0; i < n; i++)
		if (args[i].index == tid)
			return i;
	return -1;
}

DWORD myMerge(DWORD* L, DWORD *R, DWORD n1, DWORD n2) {		//the leftpart is always the  return array 
	DWORD i, j, k, *vet, N;
	N = n1 + n2;
	vet = (DWORD*)malloc(N * sizeof(DWORD));
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

VOID merge(DWORD *vet, DWORD left, DWORD middle, DWORD right) {
	DWORD i, j, k;
	DWORD n1 = middle - left + 1;
	DWORD n2 = right - middle;
	DWORD *L, *R;  // temp arrays

	L = (DWORD*)malloc(n1 * sizeof(DWORD));
	R = (DWORD*)malloc(n2 * sizeof(DWORD));

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

VOID mergeSort(DWORD *vet, DWORD left, DWORD right) {
	DWORD middle;

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
	DWORD n, i;
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
	(*arguments).array = (DWORD*)malloc(n * sizeof(DWORD));
	for (i = 0; i < n; i++) {
		ReadFile(hIn, &(*arguments).array[i], sizeof(DWORD), &nIn, NULL);
	}

	mergeSort((*arguments).array, 0, n - 1);

	CloseHandle(hIn);
	ExitThread(0);
}

DWORD _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE *threads;
	DWORD tid, i, NTOT = 0, size;
	DWORD *threadIds;
	arg_t *arguments;
	DWORD *resArray;
	HANDLE hOut;
	DWORD nOut, k, m;

	DWORD nthreads = argc - 2;

	threads = (HANDLE*)malloc(nthreads * sizeof(HANDLE));
	threadIds = (DWORD*)malloc(nthreads * sizeof(DWORD));
	arguments = (arg_t*)malloc(nthreads * sizeof(arg_t));


	for (tid = 0; tid < nthreads; tid++) {
		arguments[tid].filename = argv[tid + 1];
		arguments[tid].index = tid;
		threads[tid] = CreateThread(0, 0, sortThread, &arguments[tid], 0, &threadIds[tid]);
	}


	/*At each iteration:
 	- I wait for a ordering thread to terminate
	- I compute the tid of the terminated thread
	- I close its handle and remove it from the handles array, so that the wait function will not wait for it anymore
	- if the first one I allocate resArray, otherwise I realloc it so that I enlarge it depending on the size of the array to merge to the current ordered one
	- at first iteration I merge an ordered array with an empty one, so basically I copy it
	- then I perform actual orderings
	- anytime i receive the current size of the resArray
	- at the end, since I rearranged the handle array so that the last handle is now where it was the last processed one and I've
	 closed that one, i need to move the args too id I want to have direct access to them. I cannot move the actual args in the array 
	 because there could be some thread still working on it and, by moving it, I would move an incomplete information and some not initialized
	 values and this would cause an exception. So, I've added a field to the arg_t that signals which is the position in the thread handles
	 array of each thread according to the main. So, when I move thread NTHREAD-1 to the last processed position, I need to update also
	 this info in its arg_t so that when it returns and the main thread is signaled with its NEW TID is capable to access the correct
	 structure in the args array.

	 ex tids = {0, 1, 3}
		t1 returns
		t1 handle is closed. t3 handle is assigned to t1 handle. t3 is closed.
		t3 args need to be moved to pos 1 
		arg.tid (where the old value was 3, found by means of a function) = 1
		t3, now t1, is still capable to modify its structure
		the main can access to that structure when t1 (old t3) returns
	*/

	k = nthreads;
	DWORD *tmp;
	size = 0;
	resArray = (DWORD*)malloc(size * sizeof(DWORD));
	while (k > 0) {
		DWORD tid1 = WaitForMultipleObjects(k, threads, FALSE, INFINITE); 
		tid1 = tid1 - WAIT_OBJECT_0;
		m = k - 1;
		CloseHandle(threads[tid1]);
		threads[tid1] = threads[m];
		threads[m] = NULL;
		DWORD index = find_pos(arguments, nthreads, tid1);
		if (index == -1) {
			_ftprintf(stderr, _T("Can't find args of thread %d"), tid1);
			ExitThread(3);
		}
		tmp = (DWORD*)realloc(resArray, (size + arguments[index].size) * sizeof(DWORD));
		
		resArray = tmp;

		size = myMerge(resArray, arguments[index].array, size, arguments[index].size);
		
		if (k != 1) {
			DWORD tmp = index;
			index = find_pos(arguments, nthreads, m);
		
			if (index == -1) {
				_ftprintf(stderr, _T("Can't find args of thread %d"), tid1);
				ExitThread(4);
			}
			
			arguments[index].index = tid1;
			arguments[tmp].index = m;
		}
		k--;


	}
	

	hOut = CreateFile(argv[argc - 1], GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Cannot open input file. Error: %x\n"), GetLastError());
		ExitThread(1);
	}
	WriteFile(hOut, &size, sizeof(DWORD), &nOut, NULL);
	for (i = 0; i < size; i++)
		WriteFile(hOut, &resArray[i], sizeof(DWORD), &nOut, NULL);
	CloseHandle(hOut);

	hOut = CreateFile(argv[argc - 1], GENERIC_READ, FILE_SHARE_READ, NULL,			//i should provide new permissions and rewind the handler
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Cannot open input file. Error: %x\n"), GetLastError());
		ExitThread(2);
	}

	/*prDWORD loop to check the correctness of the produced binary file*/

	DWORD x;
	while (ReadFile(hOut, &x, sizeof(x), &nOut, NULL) && nOut > 0) {
		_ftprintf(stdout, _T("%d "), x); fflush(stdout);
	}
	_ftprintf(stdout, _T("\n")); fflush(stdout);

	
	free(arguments[0].array);
	free(arguments);
	free(threadIds);


	CloseHandle(hOut);
	ExitThread(0);
}
