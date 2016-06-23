/*I used 1 empty semaphore + 4 full semaphores because, since we want the 4 threads to actually work in parallel
the main thread needs to be signaled when each thread has finished its job (wait on empty semaphore 4 times) and needs an input value and has to know
which one finished, in order to be able to signal the correct thread that some output has been produced. And the only way
to achieve this goal is by means of an array of semaphores on which the main thread waits with  a WaitForMultipleObjects so that
at each time the thread identifier of the ready thread is returned and then used to signal back.

Moreover, I didn't use a mutex for the producer, because there's only one producer (the main thread) and no protection for
mutual exclusion is required

I've implemented two versions of the factorial function:

	- one is the classic recursive one
	- the other is a non-optimal algorithm that is capable of computing the factorial product of a big number. In fact,
	seems that LONGLONG data type can store values up to the factorial of 12. After that, we need another way to compute
	factorials because the system isn't able to deal with  that in a native way.

Same things could be applied to multiplication, since the multiplying thread multiplies in a way that could obtain huge results.
But, in this case, we're not able to determine it a priori. And, for that  reason, I'm using a custom product function too.

Consider that, in order to debug the program, I've used a series of integers that grow from 1 to 20, so, the overall result,
is computing the factorial of 20! It should be still representable, but, for not losing generality, I'm gonna use it for all products.

*/

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>

#define N_THREADS 4
#define MAX 500

typedef struct {
	DWORD tid;
	DWORD initValue;
} arg_t;

DWORD *queue, prodP, size;
HANDLE meC;
//HANDLE empty[N_THREADS], 
HANDLE empty, full[N_THREADS];

DWORD WINAPI ConsumerThread(PVOID);

DWORD summation(DWORD, DWORD, DWORD);
DWORD product(DWORD, DWORD*, DWORD, DWORD);
VOID factorial(DWORD, DWORD);
VOID largeFactorial(DWORD, DWORD);
VOID printValue(DWORD, DWORD);

int _tmain(DWORD argc, LPTSTR argv[]) {
	HANDLE hIn;
	DWORD tid, nIn, buffer, retCode[N_THREADS];
	DWORD REC_SIZE, FILE_SIZE;
	HANDLE threads[N_THREADS];
	DWORD threadsId[N_THREADS];
	arg_t arguments[N_THREADS];


	if (argc < 2) {
		_ftprintf(stderr, _T("Wrong number of input parameters\r\n")); return 1;
	}

	hIn = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		_ftprintf(stderr, _T("Invalid handle value\r\n")); return 2;
	}

	FILE_SIZE = GetFileSize(hIn, NULL);
	REC_SIZE = sizeof(DWORD);
	size = FILE_SIZE / REC_SIZE;

	queue = (DWORD*)malloc(size * sizeof(DWORD));

	meC = CreateMutex(NULL, FALSE, NULL);
	empty = CreateSemaphore(NULL, 4, 4, NULL);
	for (tid = 0; tid < N_THREADS; tid++) {
		
		full[tid] = CreateSemaphore(NULL, 0, 1, NULL);
		arguments[tid].tid = tid;
		threads[tid] = CreateThread(0, 0, ConsumerThread, &arguments[tid], 0, &threadsId[tid]);
	}

	/*I want to start in a alligned mode, because, if I start directly with the other loop there's still the risk that one thread
	starts, immediately finishes and the others won't start.*/

	prodP = -1;
	while(true){
		for (tid = 0; tid < N_THREADS; tid++)
			WaitForSingleObject(empty, INFINITE);
		//WaitForMultipleObjects(N_THREADS, empty, TRUE, INFINITE);
			if (ReadFile(hIn, &buffer, sizeof(buffer), &nIn, NULL) && nIn > 0) {
				prodP = (prodP + 1);
				queue[prodP] = buffer;
				_ftprintf(stdout, _T("[main thread] - %d added to the queue\r\n\n"), buffer); fflush(stdout);
				for(tid = 0; tid < N_THREADS; tid++)
					ReleaseSemaphore(full[tid], 1, NULL);
			} else {
				break;
			}
	}

	//input values are over: the  main thread has to terminate all threads and close all handles

	for (tid = 0; tid < N_THREADS; tid++) {
		TerminateThread(threads[tid], retCode[tid]);
		CloseHandle(threads[tid]);
		CloseHandle(full[tid]);
	}


	CloseHandle(empty);
	CloseHandle(hIn);
	CloseHandle(meC);
	free(queue);
	return 0;
}

DWORD summation(DWORD num, DWORD tot, DWORD tid) {

	_ftprintf(stdout, _T("[thread %d] --- overall summation updated. \n\tPrevious value: %d\n\tNew value: %d\r\n\n"), tid, tot, tot + num);
	return tot + (DWORD)num;
}

DWORD parseInput(DWORD n, DWORD *input) {
	DWORD i = 0;

	while (n) {
		input[i] = n % 10;	//last digit of n in the rightmost free position of the array
		n = n / 10;			//I consider next digit
		i++;
	}

	return i;
}

DWORD largeMultiply(DWORD n, DWORD *res, DWORD res_size) {
	DWORD prod, new_size, carry, carry1, i, j, digitsN, input[MAX], tmp[MAX];		//the input num is gonna be parsed so i can multiply each digit by each digit
																					// of the overall product

	digitsN = parseInput(n, input);

	/*i need a tmp array to store intermediate results*/
	for (i = 0; i < MAX; i++)
		tmp[i] = 0;

	i = 0;
	while (i < digitsN) {
		j = 0;
		carry = (carry1 = 0);
		/*I multiply each digit of the current tmp res by each digit of the  input value
		and I sum all tmp results up digit by digit. Remember that for each shift of one tens on the left on the input value
		I need to multiply by ten the current tmp res. Ex:
		44*4 ->
		1) 4*4 = 12 (m.1)
		2) 4*4 = 12 (m.2)
		res =  12*10 (one shift left) + 12 = 132
		*/
		DWORD val;

		while (j < res_size) {
			prod = res[j] * input[i] + carry;
			carry = prod / 10;
			val = (tmp[i + j] + carry1 + prod % 10) % 10;
			/*I can produce a carry also when summing up tmp res and I must consider it*/
			carry1 = (tmp[i + j] + carry1 + prod % 10) / 10;
			tmp[i + j] = val;
			j++;
		}
		/*last position where I wrote + 1*/
		new_size = i + j;
		/*saving last carry*/
		tmp[i + j] = carry + carry1; //i save it only if it is = 0 because at second next cycle on i it has to have a valid value
		if (tmp[i + j] != 0)
			new_size++;
		i++;
	}

	for (i = 0; i < new_size; i++)
		res[i] = tmp[i];

	return new_size;
}


DWORD product(DWORD num, DWORD* tot, DWORD res_size, DWORD tid) {
	DWORD i;
	res_size = largeMultiply(num, tot, res_size);
	_ftprintf(stdout, _T("[thread %d] --- overall product updated. \n\tNew value:"), tid); fflush(stdout);
	for (i = res_size; i > 0; i--)
		_ftprintf(stdout, _T("%d"), tot[i-1]);
	_ftprintf(stdout, _T("\r\n\n"));

	return res_size;
}  

LONGLONG factorial_recursive(DWORD num) {
	if (num == 0 || num == 1)
		return 1;
	
	return (LONGLONG)num * factorial_recursive(num - 1);
}

VOID factorial(DWORD num, DWORD tid) {
	_ftprintf(stdout, _T("[thread %d] --- factorial product of %d: %ld\r\n\n"),tid, num, factorial_recursive(num)); fflush(stdout);
}

// This function multiplies x with the number represented by res[].
// res_size is size of res[] or number of digits in the number represented
// by res[]. This function uses simple school mathematics for multiplication.
// This function may value of res_size and returns the new value of res_size
DWORD multiply(DWORD x, DWORD res[], DWORD res_size)
{
	DWORD i, carry = 0;  // Initialize carry

					// One by one multiply n with individual digits of res[]
	for (i = 0; i<res_size; i++)
	{
		DWORD prod = res[i] * x + carry;
		res[i] = prod % 10;  // Store last digit of 'prod' in res[]
		carry = prod / 10;    // Put rest in carry
	}

	// Put carry in res and increase result size
	while (carry)
	{
		res[res_size] = carry % 10;
		carry = carry / 10;
		res_size++;
	}
	return res_size;
}

// This function finds factorial of large numbers
VOID largeFactorial(DWORD n, DWORD tid)
{
	DWORD res[MAX], x, i;

	// Initialize result
	res[0] = 1;
	DWORD res_size = 1;

	// Apply simple factorial formula n! = 1 * 2 * 3 * 4...*n
	for (x = 2; x <= n; x++)
		res_size = multiply(x, res, res_size);

	_ftprintf(stdout, _T("[thread %d] --- factorial product of %d: "), tid, n); fflush(stdout);
	for (int i = res_size - 1; i >= 0; i--)
		_ftprintf(stdout, _T("%d"), res[i]); fflush(stdout);
	_ftprintf(stdout, _T("\r\n\n")); fflush(stdout);
}


VOID printValue(DWORD num, DWORD tid) {
	DWORD i;
	_ftprintf(stdout, _T("[thread %d] --- received value: %d -> "), tid, num); fflush(stdout);
	for (i = 0; i < num; i++)
		_ftprintf(stdout, _T("%d "), num); fflush(stdout);
	_ftprintf(stdout, _T("\r\n\n")); fflush(stdout);
}

DWORD WINAPI ConsumerThread(PVOID args) {
	arg_t *arg = (arg_t*)args;
	DWORD tid = (*arg).tid;
	DWORD buffer, tot = 0, res_size = 1, totP[MAX];

	totP[0] = 1;

	while(TRUE) {
		WaitForSingleObject(full[tid], INFINITE);
		WaitForSingleObject(meC, INFINITE);
		buffer = queue[prodP];
		switch (tid){
		case 0: 
			tot = summation(buffer, tot, tid);
			break;
		case 1:
			res_size = product(buffer, totP, res_size, tid);
			break;
		case 2: 
		/*The biggest number whose factorial LONGLONG - the biggest C data type - is capable to represent is 20
		so I need to define another function (not so optimized) to compute factorials of numbers larger than 20
		*/
			if (buffer <= 10)
				factorial(buffer, tid);
			else
				largeFactorial(buffer, tid);
			break;
		case 3:
			printValue(buffer, tid);
			break;
		default:
			break;
		}
		ReleaseMutex(meC);
		ReleaseSemaphore(empty, 1, NULL);
	}

}
