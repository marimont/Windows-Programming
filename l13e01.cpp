/*I have used some global structures which are shared among threads:
	- outfiles: an array of structures defined as outfile_t. Each one of these
		structs stores some informations about each output file: 
		- a write lock, used by scanning threads to write in mutual exclusion
		- an unsigned int to store the current number of lines used to signal to 
		the "printing thread" when it has to print out lines because the condition is verified
		- two handles opened to write on/read from the file. 

	    All these fields are initialized by the first thread that finds this file within the input file. 
		In this way, all other threads are able to use the same handle, so that they are able to start writing
		or reading starting from the exact point where the last read/write operation took place.

		All those handles are then closed by the father threads when all child threads return.

	- rsem: an array of semaphores used to signal to the printing_thread that the "M-condition" on a
		certain file took place

	- st: a simple symbol table containing only an array of strings, which contains all the output files names
	  found by the threads, and an unsigned int, which is instead the size of the array. I've used a symbol 
	  table to define a string-index correspondence so that, the other structures associated
	  with the output files (i.e. outfiles, rsem) can be accessed directly, through a numeric index. Moreover 
	  I have defined some simple functions to manage this table.

	- counter: an unsigned integer, protected by a mutex, which is incremented by each scanning thread when terminating
	so that if it is the last one, it awakes the printing thread which performs a check on that counter and if it is equal
	to the number of threads, it breaks the loop and terminates too.

The input file is opened just once by the father thread and its handle is passed to threads as an argument.
File-scanning happens in mutual exclusion, protected by a semaphore, so that there isn't the possibility of more
threads reading the same line.

The last consideration I want to point out is about a busy waiting I've implemented:
basically the printing-thread should performa a WaitForMultipleObjects on the semaphore list, waiting
for a file to be ready to be printed out. The point is that, since requirements say that we can have at most 128
output files and we need to define 128 semaphores, but the system doesn't allow to wait on more than 64 handles,
I've decided to adopt a busy waiting strategy:
	- the wait is performed in a forever loop
	- at each iteration the thread waits alternatively on the first 64 semaphores and on the last 64 semaphores
	- the wait is not infinite, but it lasts 500 millisecs
	- if the timeout expires the threads tries to wait on the other semaphores pool

That's not the most efficient way to solve this issue. And that's definitely not optimal. A better choise
would have probably been to implement two waiting threads, each one waiting on one pool of semaphores and letting
the printing thread wait on one of them to return an index. But considering that I'm not working with huge files
and that within the 500 secs the printing thread will be signaled almost always (so, no busy waiting), 
I've decided to follow this "easier" way to solve this issue. But it would be inappropriate with big files.

Other comments are left within the code.

*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

typedef struct {
	TCHAR dirname[128];
	TCHAR inpfile[128];
	TCHAR outfile[128];
} record_t;

typedef struct{
	DWORD filesN;
	DWORD charsN;
	DWORD linesN;
}out_record_t;

typedef struct{
	DWORD linesN;
	HANDLE wlock;
	HANDLE hIn;
	HANDLE hOut;
}outfile_t;

typedef struct {
	HANDLE inputFile;
	HANDLE me;
	DWORD M;
}arg_t;

typedef struct {
	DWORD M;
	DWORD N;
}printarg_t;

typedef struct {
	LPTSTR outfiles[128];
	DWORD size;
} symbol_table;

HANDLE rsem[2*MAXIMUM_WAIT_OBJECTS];
outfile_t outfiles[2*MAXIMUM_WAIT_OBJECTS];
symbol_table st;

VOID STinit() {
	st.size = 0;
}

DWORD STput(LPTSTR outfile) {
	DWORD index = st.size;
	st.outfiles[st.size] = (TCHAR*)malloc(_tcslen(outfile) * sizeof(TCHAR));
	_tcscpy(st.outfiles[st.size], outfile);
	st.outfiles[_tcslen(st.outfiles[st.size])] = '\0';
	st.size++;
	return index;
}

INT STsearch(LPTSTR outfile) {
	DWORD i;
	for (i = 0; i < st.size; i++) {
		if (!_tcscmp(st.outfiles[i], outfile))
			return i;
	}
	return -1;
}

typedef struct {
	DWORD count;
	HANDLE mutex;
} counter_t;

counter_t counter;

DWORD WINAPI Stat_thread(PVOID);
DWORD WINAPI Print_thread(PVOID);

int _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE me, *threads, printing_thread, hIn;
	DWORD *threadsId, printing_threadId;
	arg_t *arguments;
	printarg_t argument;
	DWORD N, M, i;
	LPTSTR S;

	if (argc < 4) {
		_ftprintf(stderr, _T("Invalid number of arguments\r\n")); return 1;
	}

	N = _tstoi(argv[2]);
	M = _tstoi(argv[3]);

	threads = (HANDLE*)malloc(N * sizeof(HANDLE));
	threadsId = (DWORD*)malloc(N * sizeof(DWORD));
	arguments = (arg_t*)malloc(N * sizeof(arg_t));

	me = CreateMutex(0, FALSE, NULL);
	if (me == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Invalid handle value\n")); return 2;
	}
	hIn = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Invalid handle value\n")); return 2;
	}

	for (i = 0; i < N; i++) {
		arguments[i].inputFile = hIn;
		arguments[i].me = me;
		arguments[i].M = M;
		threads[i] = CreateThread(0, 0, Stat_thread, &arguments[i], 0, &threadsId[i]);
	}

	/*i init all semaphores so that the WaitForMultipleObjects won't 
	exit the loop even if there are not so many outfiles*/

	for (i = 0; i < 2*MAXIMUM_WAIT_OBJECTS; i++)
		rsem[i] = CreateSemaphore(0, 0, 1, NULL);

	argument.M = M;
	argument.N = N;
	printing_thread = CreateThread(0, 0, Print_thread, &argument, 0, &printing_threadId);

	counter.mutex = CreateMutex(0, FALSE, 0);
	counter.count = 0;

	for (i = 0; i < N; i++) {
		WaitForSingleObject(threads[i], INFINITE);
		CloseHandle(threads[i]);
	}

	WaitForSingleObject(printing_thread, INFINITE);
	CloseHandle(printing_thread);

	CloseHandle(counter.mutex);

	for (i = 0; i < st.size; i++) {
		CloseHandle(outfiles[i].hIn);
		CloseHandle(outfiles[i].hOut);
		CloseHandle(outfiles[i].wlock);
	}

	for (i = 0; i < 2*MAXIMUM_WAIT_OBJECTS; i++)
		CloseHandle(rsem[i]);

	CloseHandle(me);
	CloseHandle(hIn);

	free(threads);
	free(threadsId);
	free(arguments);

	return 0;
}

DWORD WINAPI Stat_thread(PVOID args) {
	arg_t *arg = (arg_t*)args;
	HANDLE hIn = (*arg).inputFile;
	HANDLE me = (*arg).me;
	HANDLE searchHandle;
	DWORD nIn, M = (*arg).M, i;
	INT index;
	record_t record;
	out_record_t out_record;
	TCHAR completeDirname[255], new_completeDirname[255], c;
	WIN32_FIND_DATA findData;
	FILE *fp;

	while (1) {
		/*waiting for mutual exclusion on inputfile*/
		WaitForSingleObject(me, INFINITE);

		if (!ReadFile(hIn, &record, sizeof(record), &nIn, NULL) || nIn == 0) {
			/*EOF*/
			break;
		}
		_stprintf(completeDirname, _T("%s\\%s"), record.dirname, record.inpfile);

		index = STsearch(record.outfile);
		if (index < 0) {
			index = STput(record.outfile);
			outfiles[index].linesN = 0;
			outfiles[index].hIn = CreateFile(record.outfile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			outfiles[index].hOut = CreateFile(record.outfile, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
			/*used to write in mutual exclusion*/
			outfiles[index].wlock = CreateMutex(0, FALSE, NULL);
		}

		DWORD filesN = 0, charsN = 0, linesN = 0, n;
		TCHAR c;
		/*scan directory*/
		searchHandle = FindFirstFile(completeDirname, &findData);
		if (searchHandle == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("FindFirstFile failed\r\n")); continue;
		}
		do {
			filesN++;
			_stprintf(new_completeDirname, _T("%s\\%s"), record.dirname, findData.cFileName);
			fp = _tfopen(new_completeDirname, _T("r"));
			if (fp == NULL) continue;
			while (_ftscanf(fp, _T("%c"), &c) == 1) {
				if (c != _T(' ') && c != _T('\n'))
					charsN++;
				else if (c == _T('\n'))
					linesN++;
			}
			fclose(fp);
		} while (FindNextFile(searchHandle, &findData));

		FindClose(searchHandle);

		/*wait for mutual exclusion on output file*/
		WaitForSingleObject(outfiles[index].wlock, INFINITE);
		out_record.filesN = filesN;
		out_record.charsN = charsN;
		out_record.linesN = linesN;
		WriteFile(outfiles[index].hOut, &out_record, sizeof(out_record), &n, NULL);
		outfiles[index].linesN++;
		/*check if M condition is verified. If so, signal it to the printing_thread*/
		if (outfiles[index].linesN % M == 0)
			ReleaseSemaphore(rsem[index], 1, 0);
		ReleaseMutex(outfiles[index].wlock);

		ReleaseMutex(me);
	}

	WaitForSingleObject(counter.mutex, INFINITE);
	counter.count++;
	ReleaseMutex(counter.mutex);
	ReleaseSemaphore(rsem[0], 1, 0);

	ExitThread(0);
}


DWORD WINAPI Print_thread(PVOID args) {
	printarg_t *arg = (printarg_t*)args;
	DWORD M = (*arg).M, index;
	DWORD N = (*arg).N;
	out_record_t record;
	DWORD nIn, i;


	while (1) {
		DWORD offset = 0;
		while (1) {
			/* offset = 0 -> offset = (0+64) % 128 = 64
			   offset = 64 -> offset = (64 + 64 )% 128 = 0	
			*/
			offset = (offset + MAXIMUM_WAIT_OBJECTS) % 2*MAXIMUM_WAIT_OBJECTS;
			index = WaitForMultipleObjects(MAXIMUM_WAIT_OBJECTS, rsem + offset, FALSE, 500);
			if (index != WAIT_TIMEOUT)
				break;
		}
		WaitForSingleObject(counter.mutex, INFINITE);
		if (counter.count >= N) {
			/*all other threads terminated*/
			break;
		}
		ReleaseMutex(counter.mutex);
		index = index - WAIT_OBJECT_0;
		_ftprintf(stdout, _T("%s:\n"), st.outfiles[index]);
		for (i = 0; i < M; i++) {
			ReadFile(outfiles[index].hIn, &record, sizeof(out_record_t), &nIn, NULL);
			_ftprintf(stdout, _T("\tfilesN: %d - charsN: %d - linesN: %d\n"), record.filesN, record.charsN, record.linesN);
		}
	}

	ExitThread(0);
}


