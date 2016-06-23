#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#define MAX_BUF 10
#define BUF_SIZE 30

typedef struct {
	INT id;
	LONG regNum;
	_TCHAR name[BUF_SIZE];
	_TCHAR surname[BUF_SIZE];
	INT mark;
} record;

INT _tmain(INT argc, LPTSTR argv[]) {
	HANDLE handle;
	DWORD nIn;
	_TCHAR input_str[MAX_BUF];
	_TCHAR choise;
	INT id;
	BOOL found;
	OVERLAPPED ov = { 0,0,0,0, NULL };
	LARGE_INTEGER file_pos, file_reserved;

	record buffer, buffer1;

	if (argc != 2) {
		_ftprintf(stderr, _T("Wrong number of parameters\n")); return 1;
	}

	while (1) {
		_ftprintf(stdout, _T("Type one of the following options:\n- R <id>: Read the data of a student in a random way\n- W <id>: Write (or modify) the data of a student in random way\n- E: End the program\n> "));
		_fgetts(input_str, MAX_BUF - 2, stdin);
		if (_stscanf(input_str, _T("%c %d"), &choise, &id) != 2) {			//if only one input char -> or 'E' or error
			if (choise == 'E') {
				_ftprintf(stdout, _T("Exiting... Bye bye!\n")); break;
			}
			else {
				_ftprintf(stderr, _T("Invalid command\n")); continue;
			}
		}

		handle = CreateFile(argv[1], FILE_READ_ACCESS | FILE_WRITE_ACCESS, FILE_SHARE_READ, NULL,		//I need to open and close the handle anytime in order to rewind the file at any cycle
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		switch (choise) {
		case 'R':
			found = FALSE;
			file_pos.QuadPart = (id - 1) * sizeof(record);	//id-1 = the offset in the file starts from 0 while id's start from 1
			file_reserved.QuadPart = sizeof(record);
			ov.Offset = file_pos.LowPart;
			ov.OffsetHigh = file_pos.HighPart;
			LockFileEx(handle, 0, 0, file_reserved.HighPart, file_reserved.HighPart, &ov);
			if (ReadFile(handle, &buffer, sizeof(record), &nIn, &ov) && nIn > 0) {		//the record exists at the given position
				_ftprintf(stdout, _T("%d %ld %s %s %d\n"), buffer.id, buffer.regNum, buffer.name, buffer.surname, buffer.mark);
				found = TRUE; break;

			}
			UnlockFileEx(handle, 0, file_reserved.HighPart, file_reserved.HighPart, &ov);
			if (!found)
				_ftprintf(stdout, _T("Student %d not found\n"), id);
			break;
		case 'W':
			found = FALSE;
			_TCHAR input[255];
			DWORD nOut;

			file_pos.QuadPart = (id - 1) * sizeof(record);	//id-1 = the offset in the file starts from 0 while id's start from 1
			ov.Offset = file_pos.LowPart;
			ov.OffsetHigh = file_pos.HighPart;
			LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, file_reserved.HighPart, file_reserved.HighPart, &ov);		//since I'm gonna write I lock this area with an exclusive lock 
			if (ReadFile(handle, &buffer, sizeof(record), &nIn, &ov) && nIn > 0) {		//the record exists at the given position
				_ftprintf(stdout, _T("%d %ld %s %s %d\n"), buffer.id, buffer.regNum, buffer.name, buffer.surname, buffer.mark);

				_ftprintf(stdout, _T("New record (id reg_num name surname mark): "));
				_fgetts(input, 253, stdin);
				if (_stscanf(input, _T("%d %ld %s %s %d"), &buffer1.id, &buffer1.regNum, buffer1.name, buffer1.surname, &buffer1.mark) != 5) { //new record input
					_ftprintf(stderr, _T("Invalid input string\n")); break;
				}
				WriteFile(handle, &buffer1, nIn, &nOut, &ov);
				UnlockFileEx(handle, 0, file_reserved.HighPart, file_reserved.HighPart, &ov);
				_ftprintf(stdout, _T("Record overwritten\n"));
				found = TRUE; break;

			}
			if (!found) {

				_ftprintf(stdout, _T("New record (id reg_num name surname mark): "));		//the user requires for a brand new record to be inserted
				_fgetts(input, 253, stdin);
				if (_stscanf(input, _T("%d %ld %s %s %d"), &buffer1.id, &buffer1.regNum, buffer1.name, buffer1.surname, &buffer1.mark) != 5) {
					_ftprintf(stderr, _T("Invalid input string\n")); break;
				}
				WriteFile(handle, &buffer1, sizeof(record), &nOut, &ov);								//the offset is still ok!
				UnlockFileEx(handle, 0, file_reserved.HighPart, file_reserved.HighPart, &ov);
				_ftprintf(stdout, _T("New record added\n"));
			}
			break;
		default:
			_ftprintf(stderr, _T("Invalid command\n"));
			break;
		}

		CloseHandle(handle);
	}
	return 0;
}