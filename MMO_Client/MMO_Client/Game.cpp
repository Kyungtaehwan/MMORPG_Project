#include "pch.h"
#include "MainApp.h"
#include "Timer_Manager.h"


LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
HWND g_hWnd;
HINSTANCE hInst;
CMainApp* g_pMainApp = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

	CoInitialize(nullptr);//D2D Com초기화

	MSG msg = {};
	WNDCLASS WndClass = {};
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.lpfnWndProc = WndProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = _T("MMORPG_MAIN_WINDOW");
	RegisterClass(&WndClass);

	RECT rc{ 0, 0, WINCX, WINCY };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hWnd = CreateWindow(
		_T("MMORPG_MAIN_WINDOW"),
		_T("MMO RPG"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		0,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (!hWnd)
		return FALSE;

	g_hWnd = hWnd;
	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	CMainApp MainApp;
	g_pMainApp = &MainApp;
	CTimer_Manager::Get_Instance()->Initialize();
	MainApp.Initialize();

	const float targetFPS = 144.f;
	const float targetFrameTime = 1.f / targetFPS;

	LARGE_INTEGER freq, frameStart, frameEnd;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&frameStart);

	timeBeginPeriod(1); 

	while (true)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				goto EXIT;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		CTimer_Manager::Get_Instance()->Update();
		float dt = CTimer_Manager::Get_Instance()->Get_DeltaTime();
		MainApp.Update(dt);
		MainApp.Late_Update(dt);
		MainApp.Render();

		while (true)
		{
			QueryPerformanceCounter(&frameEnd);
			float elapsed = float(frameEnd.QuadPart - frameStart.QuadPart) / float(freq.QuadPart);
			if (elapsed >= targetFrameTime)
				break;
			if ((targetFrameTime - elapsed) > 0.002f)
				Sleep(1);
		}
		frameStart = frameEnd;
	}

	EXIT:
	timeEndPeriod(1);

	CoUninitialize();//D2D COM 해제
	return (int)msg.wParam;
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	switch (iMsg)
	{
	case WM_CREATE:
		break;
	case WM_CHAR:
		if (g_pMainApp)
			g_pMainApp->On_Char((wchar_t)wParam);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam);
}