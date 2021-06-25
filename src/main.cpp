
#include <Windows.h>

#include <wrl.h>
#include <d3d11.h>
#include <dxgi1_3.h>

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

	HWND GetWindowHandle();
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

HWND Window::GetWindowHandle()
{
	return m_hWindow;
}

class Renderer
{
private:
	Microsoft::WRL::ComPtr<ID3D11Device>        m_pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext; // immediate context
	Microsoft::WRL::ComPtr<IDXGISwapChain>      m_pSwapChain;

public:
	Renderer();

	INT  Initialize(HWND hWnd);
	VOID Uninitialize();
};

Renderer::Renderer()
{
}

INT Renderer::Initialize(HWND hWnd)
{
	INT status = 0;

	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
	D3D_FEATURE_LEVEL level;

	if (status == 0)
	{
		status = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pContext);

		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not create device/context\n", status);
		}
	}

	if (status == 0)
	{
		DXGI_SWAP_CHAIN_DESC desc;
		ZeroMemory(&desc, sizeof(DXGI_SWAP_CHAIN_DESC));
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.OutputWindow = hWnd;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

		Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIFactory> factory;

		m_pDevice.As(&dxgiDevice);
		status = dxgiDevice->GetAdapter(&adapter);

		if (SUCCEEDED(status))
		{
			adapter->GetParent(IID_PPV_ARGS(&factory));

			status = factory->CreateSwapChain(m_pDevice.Get(), &desc, &m_pSwapChain);
			if (FAILED(status))
			{
				WriteToConsole("error 0x%X: could not create swap chain\n");
			}
		}
		else
		{
			WriteToConsole("error 0x%X: could not get adapter\n");
		}
	}

	if (status == 0)
	{

	}

	return status;
}

VOID Renderer::Uninitialize()
{

}

INT main(INT argc, CHAR* argv[])
{
	INT status = 0;
	
	Window window;
	status = window.Initialize();

	

	MSG msg = {};
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
