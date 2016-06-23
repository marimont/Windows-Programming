/*Comments and explanations of crucial pieces of code are provided close to them in form of comments.

The only important thing to specify here is that, when in debug mode, the program will generate another ACCOUNT file, 
called debug_ACCOUNT.txt, so that the original one is preserved for subsequent trials and I don't have to regenerate it
at any launch.
*/


/*With a single semaphore I penalize parallelism:
each thread locks the whole document.
One possibility is to have each thread locking only the line it wants to modify so that
only threads that are trying to modify the same record will compete on a semaphore
while threads that  want to modify different records will be allowed to do it in parallel
On this purpose, I would need an array of semaphores of the same dimension of the records number.

But on the other side this approach could become infeasible in terms of resource usage: in fact, its affordable for
little files, but it would be a waste of resources with big files: a huge array of semaphores should be
defined and there's the possibility that only a few of them would be used. Besides, I could break system limitations to the
maximum number of semaphore  handles that  could be opened.

So, I decided for a tradeoff: 
	- I defined a max number of semaphores (in this program it is of 10, but it could be even greater than that)
	- if the account file contains at most 10 records, I'm gonna use each semaphore to lock one single record
	- whereas if there are more than 10 records I define a section size by dividing the number of records by the maximum number 
		of semaphores and rounding up. In this way each semaphore protects a section composed of more than one record:
		- in this way, i reduce the parallelism level with respect to the 'one semaphore to each record' version, 
		but I still improve it with respect to the single semaphore version cause threads working on different sections won't 
		interfere with each other. On the other side I limit the resource consuption.
*/
#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define BUF_SIZE 30
#define MAX_SEM 10


typedef struct {
	INT id;
	LONG accountNum;
	TCHAR name[BUF_SIZE];
	TCHAR surname[BUF_SIZE];
	LONG amount;
} record;

HANDLE *semaphores;
DWORD SECTION_RECORDS;

typedef struct {
	HANDLE accountFileHandle;
	LPTSTR opFilename;
} arg_t;

DWORD WINAPI UpdateThread(PVOID args) {
	arg_t* arg = (arg_t*)args;
	LPTSTR opFilename = (*arg).opFilename;
	OVERLAPPED ov = { 0,0,0,0, NULL };
	LARGE_INTEGER file_pos;
	HANDLE hIn, accountFileHandle = (*arg).accountFileHandle;
	DWORD nIn, nOut, nIn2, lineN, semId;
	record recordIn, recordOut;
	LONG newAmount;

	hIn = CreateFile(opFilename, GENERIC_READ, FILE_SHARE_READ, NULL,			
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Can't get handle for file: %s\n"), opFilename); ExitThread(1);
	}
	__try {

		while (ReadFile(hIn, &recordIn, sizeof(recordIn), &nIn, NULL) && nIn > 0) {
			lineN = recordIn.id - 1;
			file_pos.QuadPart = lineN * sizeof(recordOut);
			ov.Offset = file_pos.LowPart;
			ov.OffsetHigh = file_pos.HighPart;
			/*The whole transaction must happen as an atomic operation to avoid anomalies:
			-if we don't lock read too, we risk a LOST UPDATE anomaly:
			- x initial value = 200
			-R1(x)R2(x)U2(x <- x+50)W2(x)U1(x <- x+100)W1(X)
			(R1/2 ::= read done by thread 1/2; W1/2 ::= write done by thread1/2; U1/2 ::= update)
			- x final value = 300
			- UPDATE PERFORMED BY THREAD 2 IS LOST!
			*/
			semId = lineN / SECTION_RECORDS;
			WaitForSingleObject(semaphores[semId], INFINITE);
			ReadFile(accountFileHandle, &recordOut, sizeof(recordOut), &nIn2, &ov);
			newAmount = recordOut.amount + recordIn.amount;
			recordOut.amount = newAmount;
			__try {
				WriteFile(accountFileHandle, &recordOut, sizeof(recordOut), &nOut, &ov);
			}
			__finally { /*we want to be absolutely sure that the semaphore after beeing acquired is always released to avoid stalls
						or undermined behaviours*/
				ReleaseSemaphore(semaphores[semId], 1, NULL);
			}
		}
	}
	__finally { /*No matter what happens, the handle must be closed here to avoid that another threads tries to
				open something that is already opened*/
		CloseHandle(hIn);
	}
	ExitThread(0);
}


int _tmain(DWORD argc, LPTSTR argv[]) {
	DWORD N, i, nIn;
	HANDLE *threads, accountFileHandle;
	DWORD *threadsId;
	arg_t *arguments;
	record record;
	DWORD lineN = 0;
	TCHAR accountFile[500];
	DWORD rec_size, file_size, NRECORDS, SEM_NUM;

	if (argc < 3) {
		_ftprintf(stderr, _T("Wrong number of parameters\n")); return 1;
	}


#ifdef _DEBUG

	_stprintf(accountFile, _T("debug_%s"), argv[1]);
	CopyFile(argv[1], accountFile, FALSE);
#else 
	_tcscpy(accountFile, argv[1]);
#endif //DEBUG

	N = argc - 2;
	threads = (HANDLE*)malloc(N * sizeof(HANDLE));
	threadsId = (DWORD*)malloc(N * sizeof(DWORD));
	arguments = (arg_t*)malloc(N * sizeof(arg_t));

	accountFileHandle = CreateFile(accountFile, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (accountFileHandle == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Can't get an handle to file %s\n"), argv[1]); return 3;
	}
	
	file_size = GetFileSize(accountFileHandle, NULL);
	rec_size = sizeof(record);
	NRECORDS = file_size / rec_size;
	if (NRECORDS <= MAX_SEM)
		SECTION_RECORDS = 1;		//i exploit parallelism to its maximum
	else
		SECTION_RECORDS = (NRECORDS + MAX_SEM - 1) / MAX_SEM;	/*to round up division: if I have 11 records, I don't want 1 
																record per section, but two!
																*/
	SEM_NUM = NRECORDS / SECTION_RECORDS;					//needed semaphores
	
	semaphores = (HANDLE*)malloc(NRECORDS * sizeof(HANDLE));
	for (i = 0; i < SEM_NUM; i++) {
		semaphores[i] = CreateSemaphore(NULL, 1, 1, NULL);
	}

	for (i = 0; i < N; i++) {
		arguments[i].accountFileHandle = accountFileHandle;
		arguments[i].opFilename = argv[i + 2];
		threads[i] = CreateThread(0, 0, UpdateThread, &arguments[i], 0, &threadsId[i]);
	}

	WaitForMultipleObjects(N, threads, TRUE, INFINITE);

	CloseHandle(accountFileHandle);
	accountFileHandle = CreateFile(accountFile, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	while (ReadFile(accountFileHandle, &record, sizeof(record), &nIn, NULL) && nIn > 0)
		_ftprintf(stdout, _T("%d %d %s %s %d\n"), record.id, record.accountNum, record.surname, record.name, record.amount);


	CloseHandle(accountFileHandle);

	for (i = 0; i < SEM_NUM	; i++)
		CloseHandle(semaphores[i]);
	for (i = 0; i < N; i++)
		CloseHandle(threads[i]);

	free(threads); free(threadsId); free(arguments); free(semaphores);
	return 0;
}

