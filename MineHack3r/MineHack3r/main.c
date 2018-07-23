#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(lib, "ComCtl32.lib")

#define NAME L"dTm's MineHack3r v1.0"

#define IDM_FILE_EXIT 1000
#define IDM_HELP_ABOUT 1500

#define ID_SHOW_MINES 1
#define ID_HIDE_MINES 2
#define ID_FREEZE_FLAGS 3
#define ID_UNFREEZE_FLAGS 4
#define ID_FREEZE_TIMER 5
#define ID_UNFREEZE_TIMER 6

#define ADDR_MINE_FIELD_START 0x1005340
#define ADDR_MINE_FIELD_END 0x10056A0
#define ADDR_FLAGS 0x100346A
#define ADDR_TIMER 0x1002FF5

#define db(x) __emit(x)

#define CELL_EMPTY L" "
#define CELL_NOT_MINE L"o"
#define CELL_MINE L"x"
#define CELL_FLAG L"!"
#define CELL_UNKNOWN L"?"
#define CELL_BORDER L"*"

#define CELL_DIGIT_COLOUR FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define CELL_NOT_MINE_COLOUR FOREGROUND_GREEN
#define CELL_MINE_COLOUR FOREGROUND_RED | FOREGROUND_INTENSITY
#define CELL_FLAG_COLOUR FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define CELL_UNKNOWN_COLOUR FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define CELL_BORDER_COLOUR FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY

HWND hWnd = NULL;
HWND hShowMinesButton = NULL;
HWND hHideMinesButton = NULL;
HWND hFreezeFlagsButton = NULL;
HWND hUnfreezeFlagsButton = NULL;
HWND hFreezeTimerButton = NULL;
HWND hUnfreezeTimerButton = NULL;

HANDLE hMineFieldThread = NULL;
BOOL bThreadActive = FALSE;

VOID __declspec(naked) FreezeFlagsShellcodeStart(VOID) {
	__asm {
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
	}
}

VOID FreezeFlagsShellcodeEnd(VOID) {

}

VOID __declspec(naked) UnfreezeFlagsShellcodeStart(VOID) {
	__asm {
		mov		eax, DWORD PTR [esp + 4]
		db(0x01)
		db(0x05)
		db(0x94)
		db(0x51)
		db(0x00)
		db(0x01)
	}
}

VOID __declspec(naked) FreezeTimerShellcodeStart(VOID) {
	__asm {
		nop
		nop
		nop
		nop
		nop
		nop
	}
}

VOID FreezeTimerShellcodeEnd(VOID) {
	
}

VOID __declspec(naked) UnfreezeTimerShellcodeStart(VOID) {
	__asm {
		db(0xFF)
		db(0x05)
		db(0x9C)
		db(0x57)
		db(0x00)
		db(0x01)
	}
}

VOID Error(LPWSTR s) {
	WCHAR szErrBuf[BUFSIZ];
	wsprintf(szErrBuf, L"%s error: %lu", s, GetLastError());

	MessageBox(NULL, szErrBuf, NAME, MB_OK | MB_ICONERROR);
}

VOID InitialiseMenu(HWND hWnd) {
	HMENU hMenuBar = CreateMenu();
	HMENU ghMenu = CreateMenu();
	AppendMenu(ghMenu, MF_STRING, IDM_FILE_EXIT, L"&Exit");
	AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)ghMenu, L"&File");

	HMENU hMenu = CreateMenu();
	AppendMenu(hMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
	AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hMenu, L"&Help");

	SetMenu(hWnd, hMenuBar);
}

VOID CreateButtons(HWND hWnd) {
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&iccex);

	hShowMinesButton = CreateWindowEx(0, L"Button", L"Show mines", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 0, 220, 32, hWnd, (HMENU)ID_SHOW_MINES, NULL, NULL);
	hHideMinesButton = CreateWindowEx(0, L"Button", L"Hide mines", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 0, 220, 32, hWnd, (HMENU)ID_HIDE_MINES, NULL, NULL);
	ShowWindow(hHideMinesButton, SW_HIDE);
	hFreezeFlagsButton = CreateWindowEx(0, L"Button", L"Freeze flags", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 32, 220, 32, hWnd, (HMENU)ID_FREEZE_FLAGS, NULL, NULL);
	hUnfreezeFlagsButton = CreateWindowEx(0, L"Button", L"Unfreeze flags", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 32, 220, 32, hWnd, (HMENU)ID_UNFREEZE_FLAGS, NULL, NULL);
	ShowWindow(hUnfreezeFlagsButton, SW_HIDE);
	hFreezeTimerButton = CreateWindowEx(0, L"Button", L"Freeze timer", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 64, 220, 32, hWnd, (HMENU)ID_FREEZE_TIMER, NULL, NULL);
	hUnfreezeTimerButton = CreateWindowEx(0, L"Button", L"Unfreeze timer", WS_VISIBLE | WS_CHILD | BS_PUSHLIKE | BS_DEFPUSHBUTTON | WS_BORDER, 0, 64, 220, 32, hWnd, (HMENU)ID_UNFREEZE_TIMER, NULL, NULL);
	ShowWindow(hUnfreezeTimerButton, SW_HIDE);
}

VOID AlertWindow(HWND hWnd) {
	FLASHWINFO fwi;
	fwi.cbSize = sizeof(fwi);
	fwi.dwFlags = FLASHW_ALL;
	fwi.dwTimeout = 0;
	fwi.hwnd = hWnd;
	fwi.uCount = 3;

	FlashWindowEx(&fwi);
}

VOID PrintDigit(int nDigit) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), CELL_DIGIT_COLOUR);

	wprintf(L"%d", nDigit);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), csbi.wAttributes);
}

VOID PrintCell(LPWSTR c, WORD wAttributes) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wAttributes);

	wprintf(L"%s", c);

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), csbi.wAttributes);
}

HANDLE GetWindowProcessHandle(LPWSTR lpWindowName, DWORD dwDesiredAccess) {
	DWORD dwProcessId = 0;
	HWND hFindWnd = FindWindow(NULL, lpWindowName);
	if (hFindWnd == NULL)
		return NULL;
	GetWindowThreadProcessId(hFindWnd, &dwProcessId);
	return OpenProcess(dwDesiredAccess, FALSE, dwProcessId);
}

BOOL ReadMemory(LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize) {
	HANDLE hWinMineProc = GetWindowProcessHandle(L"Minesweeper", PROCESS_VM_READ);
	if (hWinMineProc == NULL)
		return Error(L"Find Minesweeper window"), FALSE;

	DWORD dwRead = 0;
	BOOL bRet = ReadProcessMemory(hWinMineProc, lpBaseAddress, lpBuffer, nSize, &dwRead);
	
	CloseHandle(hWinMineProc);

	return bRet;
}

VOID PrintMineField(VOID) {
	BYTE bCell = 0;
	DWORD dwMineFieldSize = ADDR_MINE_FIELD_END - ADDR_MINE_FIELD_START;
	LPBYTE lpMineFieldBuffer = malloc(dwMineFieldSize);
	while (bThreadActive == TRUE) {
		BOOL bSuccess = ReadMemory((LPVOID)ADDR_MINE_FIELD_START, lpMineFieldBuffer, dwMineFieldSize);
		if (bSuccess == FALSE) {
			SendMessage(hWnd, WM_COMMAND, ID_HIDE_MINES, 0);
			return;
		}

		for (DWORD j = 0, i = 0, bCell = lpMineFieldBuffer[i]; i < dwMineFieldSize; i++, j++, bCell = lpMineFieldBuffer[i]) {
			if (j == 0x20) {
				wprintf(L"\n");
				j = 0;
			}

			int nDigit = bCell & 0x1F;
			if (bCell == 0x8F || bCell == 0x8D)
				PrintCell(CELL_MINE, CELL_MINE_COLOUR);
			else if (bCell == 0x10)
				PrintCell(CELL_BORDER, CELL_BORDER_COLOUR);
			else if (bCell == 0x8E || bCell == 0x0E)
				PrintCell(CELL_FLAG, CELL_FLAG_COLOUR);
			else if (bCell == 0x0D)
				PrintCell(CELL_UNKNOWN, CELL_UNKNOWN_COLOUR);
			else if (nDigit == 0x0)
				PrintCell(CELL_EMPTY, CELL_NOT_MINE_COLOUR);
			else if (nDigit >= 0x1 && nDigit <= 0x9)
				PrintDigit(nDigit);
			else
				PrintCell(CELL_NOT_MINE, CELL_NOT_MINE_COLOUR);
		}

		Sleep(1000);
		///////////////////////////////
		///////    YUCK!!!!    ////////
		///////////////////////////////
		system("cls");
	}

	free(lpMineFieldBuffer);
}

BOOL WriteMemory(LPVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, BOOL bAllocMemory) {
	HANDLE hWinMineProc = GetWindowProcessHandle(L"Minesweeper", PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_VM_OPERATION);
	if (hWinMineProc == NULL)
		Error(L"Find Minesweeper window");

	DWORD dwWritten = 0;
	BOOL bRet = FALSE;
	if (bAllocMemory == TRUE) {
		LPVOID lpAddress = VirtualAllocEx(hWinMineProc, lpBaseAddress, nSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (lpAddress == NULL)
			return Error(L"Virtual alloc"), FALSE;
		DWORD dwOldProt = 0;
		bRet = VirtualProtectEx(hWinMineProc, lpAddress, nSize, PAGE_EXECUTE_READWRITE, &dwOldProt);
		if (bRet == FALSE)
			return Error(L"Virtual protect"), FALSE;
		bRet = WriteProcessMemory(hWinMineProc, lpAddress, lpBuffer, nSize, &dwWritten);
	} else
		bRet = WriteProcessMemory(hWinMineProc, lpBaseAddress, lpBuffer, nSize, &dwWritten);

	CloseHandle(hWinMineProc);

	return bRet;
}

BOOL FreezeFlags(BOOL bFreeze) {
	DWORD dwShellcodeSize = 10;// (DWORD)InjectFlagsShellcodeEnd - (DWORD)InjectFlagsShellcodeStart;
	LPBYTE lpShellcode = malloc(dwShellcodeSize);
	if (bFreeze == TRUE)
		memcpy(lpShellcode, FreezeFlagsShellcodeStart, dwShellcodeSize);
	else
		memcpy(lpShellcode, UnfreezeFlagsShellcodeStart, dwShellcodeSize);
	BOOL bSuccess = WriteMemory((LPVOID)ADDR_FLAGS, lpShellcode, dwShellcodeSize, FALSE);
	if (bSuccess == FALSE)
		return Error(L"Inject flags shellcode"), FALSE;
	free(lpShellcode);

	return TRUE;
}

BOOL FreezeTimer(BOOL bFreeze) {
	DWORD dwShellcodeSize = 6;// (DWORD)FreezeTimerShellcodeEnd - (DWORD)FreezeTimerShellcodeStart
	LPBYTE lpShellcode = malloc(dwShellcodeSize);
	if (bFreeze == TRUE)
		memcpy(lpShellcode, FreezeTimerShellcodeStart, dwShellcodeSize);
	else
		memcpy(lpShellcode, UnfreezeTimerShellcodeStart, dwShellcodeSize);
	BOOL bSuccess = WriteMemory((LPVOID)ADDR_TIMER, lpShellcode, dwShellcodeSize, FALSE);
	if (bSuccess == FALSE)
		return Error(L"Inject timer shellcode"), FALSE;
	free(lpShellcode);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CLOSE:
			FreeConsole();
			CloseHandle(hMineFieldThread);
			DestroyWindow(hWnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_CREATE:
			InitialiseMenu(hWnd);
			CreateButtons(hWnd);
			if (GetActiveWindow() != hWnd) {
				AlertWindow(hWnd);
			}
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDM_FILE_EXIT:
					PostMessage(hWnd, WM_CLOSE, 0, 0);
					break;
				case IDM_HELP_ABOUT:
					MessageBox(hWnd, L"MineHack3r v1.0\r\nby dTm", L"About MineHack3r", MB_OK);
					break;
				case ID_SHOW_MINES:
					AllocConsole();
					AttachConsole(GetCurrentProcessId());
					SetConsoleTitle(NAME);
					freopen("CON", "w", stdout);
					ShowWindow(hShowMinesButton, SW_HIDE);
					ShowWindow(hHideMinesButton, SW_SHOW);
					hMineFieldThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PrintMineField, NULL, 0, NULL);
					bThreadActive = TRUE;
					//PrintMineField();
					break;
				case ID_HIDE_MINES:
					FreeConsole();
					bThreadActive = FALSE;
					ShowWindow(hShowMinesButton, SW_SHOW);
					ShowWindow(hHideMinesButton, SW_HIDE);
					break;
				case ID_FREEZE_FLAGS:
					if (FreezeFlags(TRUE) == FALSE) break;
					ShowWindow(hUnfreezeFlagsButton, SW_SHOW);
					ShowWindow(hFreezeFlagsButton, SW_HIDE);
					break;
				case ID_UNFREEZE_FLAGS:
					FreezeFlags(FALSE);
					ShowWindow(hFreezeFlagsButton, SW_SHOW);
					ShowWindow(hUnfreezeFlagsButton, SW_HIDE);
					break;
				case ID_FREEZE_TIMER:
					if (FreezeTimer(TRUE) == FALSE) break;
					ShowWindow(hUnfreezeTimerButton, SW_SHOW);
					ShowWindow(hFreezeTimerButton, SW_HIDE);
					break;
				case ID_UNFREEZE_TIMER:
					FreezeTimer(FALSE);
					ShowWindow(hFreezeTimerButton, SW_SHOW);
					ShowWindow(hUnfreezeTimerButton, SW_HIDE);
					break;
			}
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc;

	// register window class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPLICATION));
	wc.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APPLICATION), IMAGE_ICON, 16, 16, 0);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_3DFACE);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"Window";

	if (RegisterClassEx(&wc) == 0)
		return Error(L"Window registration"), 1;

	// create window
	hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, wc.lpszClassName, NAME, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 230, 152, NULL, NULL, hInstance, NULL);
	if (hWnd == NULL)
		return Error(L"Window creation"), 1;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	return 0;
}