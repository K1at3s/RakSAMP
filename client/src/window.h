/*
	Updated to 0.3.7 by P3ti
*/

// Child Window/Control IDs
#define IDC_LBLINFO             100
#define IDC_LSTCUSTOM           101
#define IDC_INPUTBOX			102

extern HWND hwnd, texthwnd, loghwnd, inputhwnd;
extern HINSTANCE g_hInst;

void SetUpWindow(HINSTANCE hInstance);

LRESULT CALLBACK TeleportMenuProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI TeleportMenuThread(PVOID);