/*Comments and explanations of the most tricky pieces of code are provided in form of comments next to them. 
The only thing I want to specify here is that this code will produce some log files to track thread behaviour.
These files are named according to this rule: Log<thread_id>.txt since each thread will produce its own log file.
*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <stdio.h>
#include <tchar.h>

#define TYPE_FILE 1
#define TYPE_DIR 2
#define TYPE_DOT 3

#define TERMINATED 1
#define RUNNING 2
#define KILLED 3

#define MAX 500

typedef struct {
	TCHAR path[MAX];
	DWORD tid;
	DWORD status;	/*{terminated, running} -> setted by the visiting threads before sending a signal on the semaphore and terminating.
					The comparing thread is gonna be awaken by a semaphore release and it's gonna check whether the visiting thread signaled
					it because it found a new entry or terminated. Otherwise, the comparing thread is gonna wait forever!
					*/
	TCHAR filename[MAX];
} arg_t;


typedef struct {
	arg_t *arguments;
	BOOL result;
} compArg_t;

DWORD WINAPI TraverseThread(PVOID);
DWORD WINAPI CompareThread(PVOID);
static DWORD RecursiveTraverse(arg_t*, DWORD, FILE*);
DWORD FindType(LPWIN32_FIND_DATA);



HANDLE semA, semB;
DWORD NTHREADS;

int _tmain(DWORD argc, LPTSTR argv[]) {

	TCHAR path[MAX], prefix[4], fullpath[MAX];
	HANDLE *threads, comparingHandle;
	DWORD *threadsId, comparingThreadId;
	arg_t *arguments;
	compArg_t compArg;

	NTHREADS = argc - 1;
	GetCurrentDirectory(MAX, path);

	threads = (HANDLE*)malloc(NTHREADS * sizeof(HANDLE));
	threadsId = (DWORD*)malloc(NTHREADS * sizeof(DWORD));
	arguments = (arg_t*)malloc(NTHREADS * sizeof(arg_t));

	DWORD i;

	/*its initialized to zero so that the comparing thread 
	will be blocked in wait status as it starts running*/

	semA = CreateSemaphore(NULL, 0, NTHREADS, NULL);
	semB = CreateSemaphore(NULL, 0, NTHREADS, NULL);
	if (semA == INVALID_HANDLE_VALUE || semB == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Can't create semaphore\n")); return 1;
	}

	compArg.arguments = arguments;
	comparingHandle = CreateThread(0, 0, CompareThread, &compArg, 0, &comparingThreadId);
	if (comparingHandle == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Can't create comparing thread\n")); return 3;
	}

	for (i = 0; i < NTHREADS; i++) {
		_tcsncpy(prefix, argv[i + 1], 3);
		prefix[3] = '\0';
		if (_tcscmp(prefix, _T("C:\\")))
			_stprintf(fullpath, _T("%s\\%s"), path, argv[i + 1]);								//relative path
		else
			_tcscpy(fullpath, argv[i + 1]);

		_tcscpy(arguments[i].path, fullpath);
		arguments[i].tid = i;
		arguments[i].status = RUNNING;
		threads[i] = CreateThread(0, 0, TraverseThread, &arguments[i], 0, &threadsId[i]);
		if (threads[i] == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Can't create thread %d\n"), i); return 2;
		}
	}


	WaitForMultipleObjects(NTHREADS, threads, TRUE, INFINITE);
	WaitForSingleObject(comparingHandle, INFINITE);
	for (i = 0; i < NTHREADS; i++)
		CloseHandle(threads[i]);
	CloseHandle(comparingHandle);
	CloseHandle(semA); CloseHandle(semB);

	if (compArg.result)
		_ftprintf(stdout, _T("Provided paths are equal\n"));
	else 
		_ftprintf(stdout, _T("Provided paths are not equal\n"));

	free(threads); free(threadsId); free(arguments);
	return 0;
}

static DWORD RecursiveTraverse(arg_t* arg, DWORD level, FILE *fpout) {
	TCHAR ppath[MAX], *path = (*arg).path;
	TCHAR fullpath[MAX], searchString[MAX];
	HANDLE searchHandle;
	WIN32_FIND_DATA  findData;
	DWORD type, l, i, tid = (*arg).tid;


	/*I am at the beginning of a recursive call.
	This means that I have just entered a directory*/
	for (i = 0; i < level; i++)
		_ftprintf(fpout, _T(" "));
	_ftprintf(fpout, _T("[thread %d] - Visiting directory ---> %s\n"), tid, path);
	level++;

	l = _tcslen(path);
	if (path[l - 1] == '\\')
		_stprintf(searchString, _T("%s*"), path);
	else
		_stprintf(searchString, _T("%s\\*"), path);
	searchHandle = FindFirstFile(searchString, &findData);
	if (searchHandle == INVALID_HANDLE_VALUE) {
		_ftprintf(fpout, _T("ERROR: FindFirstFile failed!\n")); return 1;		//1 corresponds to failure, 0 to success
	}
	do {
		type = FindType(&findData);
		if (type != TYPE_DOT) {
			_tcscpy((*arg).filename, findData.cFileName);
			ReleaseSemaphore(semA, 1, NULL);
			WaitForSingleObject(semB, INFINITE);

			/*If the comparing thread discovered that paths are not equal, it killed al threads, so they can return*/

			if ((*arg).status == KILLED) {
				_ftprintf(fpout, _T("Killed by comparing thread!\n"));
				break;
			}
			if (type == TYPE_FILE) {
				for (i = 0; i < level; i++)
					_ftprintf(fpout, _T(" "));
				_ftprintf(fpout, _T("[thread %d] - File: %s\n"), tid, findData.cFileName);
			} else if (type == TYPE_DIR) {
				l = _tcslen(path);
				if (path[l - 1] == '\\')
					_stprintf(fullpath, _T("%s%s"), path, findData.cFileName);
				else {
					_stprintf(fullpath, _T("%s\\%s"), path, findData.cFileName);
				}
				
				/*Anytime I come back from recursion I have to restore current path
				otherwise I won't be able to visit other branches in the tree since 
				following folder names will be attached to the previous ones, on the same
				level of recursion:
					- path = \DIR\
					- DIR content = subdir1, subdir2
					- recursion on subdir1 -> path = \DIR\subdir1
					- when I return from recursion (i.e. i end up visiting folder subdir1)
					path is still = \DIR\subdir1, so new path will be \DIR\subdir1\subdir2 if I don't restore the old path!
				*/
				_tcscpy(ppath, (*arg).path);
				_tcscpy((*arg).path, fullpath);
				RecursiveTraverse(arg, level, fpout);
				_tcscpy((*arg).path, ppath);
			}
		}

	} while (FindNextFile(searchHandle, &findData));

	return 0;
}

DWORD WINAPI TraverseThread(PVOID args) {
	arg_t *arg = (arg_t*)args;
	FILE *fp;
	TCHAR filename[MAX];

	/*I gonna produce some log files to track each thread behaviour*/
	_stprintf(filename, _T("Log%d.txt"), (*arg).tid);
	fp = _tfopen(filename, _T("w"));
	RecursiveTraverse(arg, 0, fp);
	
	/*Before exiting it has to change its status to TERMINATED and signal the comparing thread that is blocked on waiting*/
	if ((*arg).status != KILLED) {
		/* KILLED status is setted by the comparing thread if it finds inequality, so it is  already terminated -> NO NEED TO WAKE IT UP
		*/
		(*arg).status = TERMINATED;		
		ReleaseSemaphore(semA, 1, NULL);
	}
	fclose(fp);
	ExitThread(0);
}

DWORD WINAPI CompareThread(PVOID args) {
	compArg_t *compArg = (compArg_t*)args;
	arg_t *arguments = (*compArg).arguments;
	DWORD i, k, terminated = 0;
	BOOL equal = true;
	/*I wait to be signaled by all threads. When I I wake up there are two possibilities:
		- all threads returned something to be compared
		- one or more threads terminated and I need to check it before comparing objects
	*/while (equal ) {
		i = NTHREADS;
		while (i > 0) {
			WaitForSingleObject(semA, INFINITE); i--;
		}
		i = 0;
		while (i < NTHREADS) {
			/*There are two cases of inequality:
				- different filenames
				- one has terminated and the  other not -> in this case the status field, within the argument structure, will
				have differente values in for two subsequent structures (TERMINATED and RUNNING);
			*/
			if (i != NTHREADS - 1) {	//i need i to go to NTHREADS to correctly manage termination
				if (arguments[i].status != arguments[i + 1].status || _tcscmp(arguments[i].filename, arguments[i + 1].filename)) {
					for (k = 0; k < NTHREADS; k++)
						arguments[k].status = KILLED;
					equal = false; break;
				}
			}
			if (arguments[i].status == TERMINATED)
				terminated++;
			i++;
		}

		ReleaseSemaphore(semB, NTHREADS, NULL);
		if (terminated == NTHREADS)
			break;
	}

	(*compArg).result = equal;
	ExitThread(0);
}


static DWORD FindType(LPWIN32_FIND_DATA pFileData){
	BOOL IsDir;
	DWORD FType;
	FType = TYPE_FILE;
	IsDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (IsDir)
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0
			|| lstrcmp(pFileData->cFileName, _T("..")) == 0)
			FType = TYPE_DOT;
		else FType = TYPE_DIR;
		return FType;
}

