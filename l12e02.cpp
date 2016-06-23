/*The two threads contain both __try/__except block, nested into a __try/__finally block so that, even if 
an exception is caught and managed by the handler, the finally block assures that dynamic array are released 
and file handles closed.

Moreover, Thread1 (the one performing divisions) has two additional __try/__except blocks (one for int division
and one for floating point division) so that if a div by zero exception occurs, the thread can proceed 
to loop and try to perform another division.

Two issues arose:

-> FLOATING POINT EXCEPTIONS: even if it was not explicitely required to manage floating point divisions
I challenged myself trying to understand how they are managed. That's why I have performed both integer
and floating points division: just to stress how differently they are managed by the system. Integer div by
zero is not critical and it's thrown and caught  by the except block as any other kind of exception.
Floating point exceptions, instead, are not enabled by default. The system lets the hardware manage it in
and imprecise way and, with respect to div by zero, it returns an 'infinite' result.
So, if we want to explicitely be warned of these kind of exceptions we need to modify the FPU (floating point
unit) by unmasking the flags corresponding to the exceptions that we want to be alerted of. 
This is done by means of  _controfp(). One important thing to point out is that the floating-point exception 
flags are sticky, so when an exception flag is raised it will stay set until explicitly cleared. 
This means that if you choose not to enable floating-point exceptions you can still detect whether 
any have happened.

And – not so obviously – if the exception associated with a flag is enabled after 
the flag is raised then an exception will be triggered on the next FPU instruction, 
even if that is several weeks after the operation that raised the flag. 
Therefore it is critical that the exception flags be cleared each time before exceptions are enabled by means
of _clearfp() function.

Finally, when generating SSE code I've sometimes seen STATUS_FLOAT_MULTIPLE_TRAPS instead of 
STATUS_FLOAT_DIVIDE_BY_ZERO and that's why I'm gonna check both of those conditions.

-> ARRAY BOUNDS EXCEED EXCEPTION: the system allows to write out of bounds, then, when it releases memory, 
calling the 'free' function, it arises an exception which is quite difficult to be managed since it is a 
heap corruption issue.
					
From documentation: The thread attempts to access an array element that is out of bounds, and					
the underlying hardware supports bounds checking. -> So I need to check boundaries explicitely and
raise the  exception manually if I don't wan't to be 'bothered' with heap corruption issue.

Other comments are provided step by step, close to the code.
*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <float.h>

#define INVALID_HANDLE_EXCEPTION 0xE0000000
#define ODD_NUM_VALUES_EXCEPTION 0xE0000001
#define MEM_ALLOCATION_EXCEPTION 0xE0000002
#define CORRUPTED_FILE_EXCEPTION 0xE0000003

typedef struct {
	LPTSTR fileName;
} arg_t;

INT FilterExpression(DWORD);

DWORD WINAPI Thread1(PVOID);
DWORD WINAPI Thread2(PVOID);

INT _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE threads[2];
	DWORD i, nIn, threadsId[2];
	arg_t argument;
	
	/*No check on argc so I can generate an INVALID_HANDLE_EXCEPTION*/

	__try {
		argument.fileName = argv[1];
		threads[0] = CreateThread(0, 0, Thread1, &argument, 0, &threadsId[0]);
		threads[1] = CreateThread(0, 0, Thread2, &argument, 0, &threadsId[1]);
		if (threads[0] == INVALID_HANDLE_VALUE || threads[1] == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Can't get valid handles for threads\r\n")); return 2;
		}

		WaitForMultipleObjects(2, threads, TRUE, INFINITE);
	}
	__finally {
		
		for (i = 0; i < 2; i++)
			CloseHandle(threads[i]);

	}
	return 0;
}

INT FilterExpression(DWORD ExCode) {
	// Clear the exception or else every FP instruction will
	// trigger it again.
	_clearfp();
	if (ExCode == INVALID_HANDLE_EXCEPTION
		|| ExCode == EXCEPTION_INT_DIVIDE_BY_ZERO
		|| ExCode == ODD_NUM_VALUES_EXCEPTION
		|| ExCode == CORRUPTED_FILE_EXCEPTION
		|| ExCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED
		|| ExCode == EXCEPTION_ACCESS_VIOLATION
		|| ExCode == MEM_ALLOCATION_EXCEPTION
		|| ExCode == STATUS_FLOAT_DIVIDE_BY_ZERO
		|| ExCode == STATUS_FLOAT_MULTIPLE_TRAPS
		)
		return EXCEPTION_EXECUTE_HANDLER;
	else
		return EXCEPTION_CONTINUE_SEARCH;
}

typedef struct {
	DWORD n1;
	DWORD n2;
} input_t;

DWORD WINAPI Thread1(PVOID args) {
	arg_t *arg = (arg_t*)args;
	LPTSTR filename = (*arg).fileName;
	HANDLE hIn;
	DWORD nIn, exCode, FPOld, FPNew;
	input_t values;

	/*The floating-point exception flags are part of the processor state which means 
	that they are per-thread settings. Therefore, if you want exceptions enabled 
	everywhere you need to do it in each thread*/

	/* Save old control mask. */
	FPOld = _controlfp(0, 0);
	/* Enable floating-point exceptions. */
	_clearfp();
	/* Set new control mask. */
	_controlfp(0, EM_ZERODIVIDE); //to unmask an exception it has to be negated
	
	hIn = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
		0, OPEN_EXISTING, 0, NULL);
	__try{
		__try {
			if (hIn == INVALID_HANDLE_VALUE)
				RaiseException(INVALID_HANDLE_EXCEPTION, 0, 0, 0);
			while (ReadFile(hIn, &values, sizeof(input_t), &nIn, NULL) && nIn > 0) {
				if (nIn != sizeof(input_t))
					RaiseException(ODD_NUM_VALUES_EXCEPTION, 0, 0, 0);

				_try{
					DWORD int_ratio = values.n1 / values.n2;
				_ftprintf(stderr, _T("[Thread1] --- Received values: %d %d. Integer ratio: %d\r\n"), values.n1, values.n2, int_ratio);
				}
				__except(FilterExpression(exCode = GetExceptionCode())) {
					_ftprintf(stderr, _T("[Thread1] --- Exception occurred: "), values.n1, values.n2);
					if(exCode == EXCEPTION_INT_DIVIDE_BY_ZERO)
						_ftprintf(stderr, _T("integer divide by zero\r\n"));
				}

				__try {

					DOUBLE n1 = (DOUBLE)values.n1;
					DOUBLE n2 = (DOUBLE)values.n2;
					DOUBLE ratio = n1 / n2;
					_ftprintf(stderr, _T("[Thread1] --- Received values: %d %d. Floating point ratio: %.3f\r\n"), values.n1, values.n2, ratio);
				}__except(FilterExpression(exCode = GetExceptionCode())){
					_ftprintf(stderr, _T("[Thread1] --- Exception occurred: "), values.n1, values.n2);
					if (exCode == STATUS_FLOAT_MULTIPLE_TRAPS)
						_ftprintf(stderr, _T("multiple traps (floating point divide by zero)\r\n"));
					else if (exCode == STATUS_FLOAT_DIVIDE_BY_ZERO)
						_ftprintf(stderr, _T("floating point divide by zero\r\n"));

				}
			}

		}
		__except (FilterExpression(exCode = GetExceptionCode())){
			_ftprintf(stderr, _T("[Thread1] --- Exception occurred: "));
			if (exCode == INVALID_HANDLE_EXCEPTION)
				_ftprintf(stderr, _T("invalid file handle\r\n"));
			else if (exCode == ODD_NUM_VALUES_EXCEPTION)
				_ftprintf(stderr, _T("odd number of input values\r\n"));

		}
		

	} __finally {
		CloseHandle(hIn); 
		/* Restore the old mask value. */
		_clearfp();
		_controlfp(FPOld, MCW_EM);
	}

	ExitThread(0);
}

DWORD WINAPI Thread2(PVOID args) {
	arg_t *arg = (arg_t*)args;
	LPTSTR filename = (*arg).fileName;
	HANDLE hIn;
	DWORD nIn, exCode, i;
	DWORD num, size, *queue = NULL;

	
	hIn = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
		0, OPEN_EXISTING, 0, NULL);

		__try {
			__try {
				if (hIn == INVALID_HANDLE_VALUE)
					RaiseException(INVALID_HANDLE_EXCEPTION, 0, 0, 0);
				if (!ReadFile(hIn, &size, sizeof(DWORD), &nIn, NULL) || nIn != sizeof(DWORD))
					RaiseException(CORRUPTED_FILE_EXCEPTION, 0, 0, 0);
				queue = (DWORD*)malloc(size * sizeof(DWORD));
				if (queue == NULL)
					RaiseException(MEM_ALLOCATION_EXCEPTION, 0, 0, 0);
				i = 0;
				while (ReadFile(hIn, &num, sizeof(DWORD), &nIn, NULL) && nIn > 0) {
					if (i >= size)
						RaiseException(EXCEPTION_ARRAY_BOUNDS_EXCEEDED, 0, 0, 0);
					queue[i] = num;
					_ftprintf(stderr, _T("[Thread2] ---                      %d added to the queue\r\n"), queue[i]);
					i++;
				}

			}
			__except (FilterExpression(exCode = GetExceptionCode())) {
				_ftprintf(stderr, _T("[Thread2] --- Exception occurred: "));
				if (exCode == INVALID_HANDLE_EXCEPTION) {
					_ftprintf(stderr, _T("invalid file handle\r\n")); fflush(stdout);
				} 
				else if (exCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED) {
					_ftprintf(stderr, _T("array boundaries exceeded\r\n")); fflush(stdout);
				}
				else if (exCode == MEM_ALLOCATION_EXCEPTION) {
					_ftprintf(stderr, _T("memory allocation\r\n")); fflush(stdout);
				}
				else if (exCode == CORRUPTED_FILE_EXCEPTION) {
					_ftprintf(stderr, _T("empty or wcorrupted file\r\n")); fflush(stdout);
				}
				else if (exCode == EXCEPTION_ACCESS_VIOLATION) {
					_ftprintf(stderr, _T("memory access violation\r\n")); fflush(stdout);
				}

			}


		}
		__finally {
			CloseHandle(hIn); 
			/*If the thread execution terminated before the array allocation (exception occurred)
			freeing this structure would be forbidden and therefore some annoying exceptions would be 
			thrown*/
			if (queue != NULL) free(queue);
		
		}
	

	ExitThread(0);
}