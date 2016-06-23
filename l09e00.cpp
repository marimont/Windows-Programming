/*Simple program to generate binary files starting from textual files
-it takes a list of N input files:
	-> N/2 are input files
	-> N/2 are output files 
*/

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define BUF_SIZE 30


INT _tmain(INT argc, LPTSTR argv[]) {
	FILE *fp;
	HANDLE hOut, hIn;
	errno_t err;
	DWORD nOut, nIn;
	INT buffer;
	LPTSTR fin, fout;

	int filen = 1;
	while (filen <= argc/2) {
		fin = argv[filen];
		fout = argv[filen + argc / 2];
		err = _tfopen_s(&fp, fin, _T("r"));
		if (err != 0) {
			_ftprintf(stderr, _T("Cannot open input file\n")); return 2;
		}

		hOut = CreateFile(fout, GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hOut == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Can't open output file\n")); return 3;
		}
		
		while (_ftscanf(fp, _T("%d"), &buffer) != EOF) {   //in this way the file is read in ANSI C
			WriteFile(hOut, &buffer, sizeof(buffer), &nOut, NULL);			//serialization performed: binary write on output file		
			if (nOut != sizeof(buffer)) {
				_ftprintf(stderr, _T("Can't write the correct number of bytes of output file\n")); return 4;
			}
		}
		filen++;
		fclose(fp);
		CloseHandle(hOut);

		hIn = CreateFile(fout, GENERIC_READ, FILE_SHARE_READ, NULL,			//i should provide new permissions and rewind the handler
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hIn == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Cannot open input file. Error: %x\n"), GetLastError());
			return 5;
		}
		while (ReadFile(hIn, &buffer, sizeof(buffer), &nIn, NULL) && nIn > 0) {
			_ftprintf(stdout, _T("%d "), buffer); fflush(stdout);
		}
		_ftprintf(stdout, _T("\n")); fflush(stdout);
		CloseHandle(hIn);
	}

	return 0;
}