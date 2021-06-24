
#include <Windows.h>

#include <cstdio>

enum {
	WIDTH  = 1280,
	HEIGHT =  720
};

const CHAR* CLASS_NAME  = "DxClass";
const CHAR* WINDOW_NAME = "DirectX";

VOID WriteToConsole(const char* fmt, ...)
{
	enum { BUFFER_SIZE = 256 };

	DWORD length = 0;
	CHAR  buffer[BUFFER_SIZE];

	va_list args;
	va_start(args, fmt);
	length = std::vsnprintf(buffer, BUFFER_SIZE, fmt, args);
	va_end(args);

	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdOut != NULL && stdOut != INVALID_HANDLE_VALUE)
	{
		WriteConsoleA(stdOut, buffer, length, NULL, NULL);
	}
}

class Window
{
private:
	HINSTANCE      m_hInstance;
	HWND           m_hWindow;

	static Window* m_Instance;

private:
	RECT CalculateWindowRect();

	static LRESULT WINAPI WindowProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);

public:
	Window();

	INT  Initialize();
	VOID Uninitialize();
};

Window* Window::m_Instance = NULL;

Window::Window()
{
	m_hInstance = NULL;
	m_hWindow = NULL;
}

RECT Window::CalculateWindowRect()
{
	HWND hDesktop = GetDesktopWindow();
	RECT rWindow = {};

	GetClientRect(hDesktop, &rWindow);
	rWindow.left   = (rWindow.right  - WIDTH)  / 2;
	rWindow.top    = (rWindow.bottom - HEIGHT) / 2;
	rWindow.right  = rWindow.left + WIDTH;
	rWindow.bottom = rWindow.top  - HEIGHT;

	return rWindow;
}

INT Window::Initialize()
{
	INT status = 0;

	m_Instance = this;
	m_hInstance = GetModuleHandleW(NULL);

	if (status == 0)
	{
		WNDCLASSEXA wndClass;
		wndClass.cbSize = sizeof(WNDCLASSEXA);
		wndClass.style = 0;
		wndClass.lpfnWndProc = WindowProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = m_hInstance;
		wndClass.hIcon = NULL;
		wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = CLASS_NAME;
		wndClass.hIconSm = NULL;

		if (!RegisterClassExA(&wndClass))
		{
			DWORD dwError = GetLastError();
			if (dwError != ERROR_CLASS_ALREADY_EXISTS)
			{
				status = HRESULT_FROM_WIN32(dwError);
				WriteToConsole("error 0x%X: could not register window class\n", dwError);
			}
		}
	}

	if (status == 0)
	{
		RECT r = CalculateWindowRect();
		
		m_hWindow = CreateWindowExA(0, CLASS_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW, r.left, r.top, r.right - r.left, r.top - r.bottom, NULL, NULL, m_hInstance, NULL);

		if (m_hWindow == NULL)
		{
			DWORD dwError = GetLastError();

			status = HRESULT_FROM_WIN32(dwError);
			WriteToConsole("error 0x%X: could not create window\n", status);
		}
	}

	if (status == 0)
	{
		ShowWindow(m_hWindow, SW_SHOW);
	}
	else
	{
		Uninitialize();
	}

	return status;
}

LRESULT WINAPI Window::WindowProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CLOSE:
		{
			m_Instance->Uninitialize();
			return 0;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	}

	return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

VOID Window::Uninitialize()
{
	HMENU hMenu = GetMenu(m_hWindow);
	if (hMenu != NULL)
	{
		DestroyMenu(hMenu);
	}

	DestroyWindow(m_hWindow);
	UnregisterClassA(CLASS_NAME, m_hInstance);
}

INT main(INT argc, CHAR* argv[])
{
	INT status = 0;
	
	Window window;
	status = window.Initialize();

	MSG  msg = {};
	while (true)
	{
		if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE) != 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if (msg.message == WM_QUIT)
			{
				break;
			}
		}
		else
		{
			// render
		}
	}

	return status;
}
