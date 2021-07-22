
// prevent redefinition of NTSTATUS messages
#define UMDF_USING_NTSTATUS

#include <Windows.h>

#include <ntstatus.h>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>

#include <string>

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

namespace Data
{
	const char* VERTEX_SHADER =
		"														"
		"	cbuffer MatrixBuffer : register(b0)					"
		"	{													"
		"		matrix mvp;										"
		"	};													"
		"														"
		"	struct VS_INPUT										"
		"	{													"
		"		float3 vertex : POSITION;						"
		"	};													"
		"														"
		"	struct VS_OUTPUT									"
		"	{													"
		"		float3 vertex : SV_POSITION;					"
		"	};													"
		"														"
		"	VS_OUTPUT main(VS_INPUT input)						"
		"	{													"
		"		VS_OUTPUT Output;								"
		"														"
		"		float4 vertex = float4(input.vertex, 1.0);		"
		"		vertex = mul(vertex, mvp);						"
		"														"
		"		Output.vertex = vertex;							"
		"		return Output;									"
		"	}													";

	const char* PIXEL_SHADER =
		"														"
		"	struct PS_INPUT										"
		"	{													"
		"		float3 vertex : SV_POSITION;					"
		"	};													"
		"														"
		"	struct PS_OUTPUT									"
		"	{													"
		"		float3 color  : SV_TARGET;						"
		"	};													"
		"														"
		"	PS_OUTPUT main(PS_INPUT input)						"
		"	{													"
		"		PS_OUTPUT Output;								"
		"														"
		"		Output.color  = float3(1,0,1);					"
		"		return Output;									"
		"	}													";
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
	INT status = STATUS_SUCCESS;

	m_Instance = this;
	m_hInstance = GetModuleHandleW(NULL);

	if (SUCCEEDED(status))
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

	if (SUCCEEDED(status))
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

	if (SUCCEEDED(status))
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
	ID3D11Device*           m_pDevice;
	ID3D11DeviceContext*    m_pContext;
	IDXGISwapChain*         m_pSwapChain;
	ID3D11Texture2D*        m_pBackBuffer;
	ID3D11RenderTargetView* m_pRenderTarget;
	ID3D11Texture2D*        m_pDepthStencilBuffer;
	ID3D11DepthStencilView* m_pDepthStencilView;
	ID3D11Debug*            m_DebugInterface;
	ID3D11VertexShader*		m_VertexShader;
	ID3D11PixelShader*		m_PixelShader;

public:
	Renderer();

	INT  Initialize(HWND hWnd);
	VOID Uninitialize();

private:
	INT CompileShaders();
	INT CompileShader(const char* pSrcData, size_t SrcDataSize, const char* pSourceName, const char* pEntrypoint, const char* pTarget, ID3DBlob** ppCode);
};

Renderer::Renderer()
{
	m_pDevice = NULL;
	m_pContext = NULL;
	m_pSwapChain = NULL;
	m_pBackBuffer = NULL;
	m_pRenderTarget = NULL;
	m_pDepthStencilBuffer = NULL;
	m_pDepthStencilView = NULL;
	m_DebugInterface = NULL;
	m_VertexShader = NULL;
	m_PixelShader = NULL;
}

INT Renderer::Initialize(HWND hWnd)
{
	INT status = STATUS_SUCCESS;

	const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
	
	D3D_FEATURE_LEVEL level;
	D3D11_TEXTURE2D_DESC bbDesc = {};

	if (SUCCEEDED(status))
	{
		status = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pContext);

		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not create device/context\n", status);
		}
	}

	if (SUCCEEDED(status))
	{
		m_pDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&m_DebugInterface));
	
		if (m_DebugInterface == NULL)
		{
			status = STATUS_UNSUCCESSFUL;
			WriteToConsole("error 0x%X: could not get the dx11 debug interface\n", status);
		}
	}

	if (SUCCEEDED(status))
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

		IDXGIDevice*  pDXGIDevice = NULL;
		IDXGIAdapter* pDXGIAdapter = NULL;
		IDXGIFactory* pIDXGIFactory = NULL;

		m_pDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&pDXGIDevice));
		status = pDXGIDevice->GetAdapter(&pDXGIAdapter);

		if (SUCCEEDED(status))
		{
			pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pIDXGIFactory));

			status = pIDXGIFactory->CreateSwapChain(m_pDevice, &desc, &m_pSwapChain);
			if (FAILED(status))
			{
				WriteToConsole("error 0x%X: could not create the swap chain\n", status);
			}
		}
		else
		{
			WriteToConsole("error 0x%X: could not get the dxgi device's adapter\n", status);
		}

		pIDXGIFactory->Release();
		pDXGIAdapter->Release();
		pDXGIDevice->Release();
	}

	if (SUCCEEDED(status))
	{
		status = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_pBackBuffer));

		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not get the swap chain back buffer\n", status);
		}
	}

	if (SUCCEEDED(status))
	{
		status = m_pDevice->CreateRenderTargetView(m_pBackBuffer, NULL, &m_pRenderTarget);

		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not create the render target view\n", status);
		}
	}

	if (SUCCEEDED(status))
	{
		m_pBackBuffer->GetDesc(&bbDesc);

		D3D11_TEXTURE2D_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthStencilDesc.Width = bbDesc.Width;
		depthStencilDesc.Height = bbDesc.Height;
		depthStencilDesc.ArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		depthStencilDesc.CPUAccessFlags = 0;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
		depthStencilDesc.MiscFlags = 0;

		status = m_pDevice->CreateTexture2D(&depthStencilDesc, NULL, &m_pDepthStencilBuffer);

		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not create depth buffer\n", status);
		}
	}

	if (SUCCEEDED(status))
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
		depthStencilViewDesc.Texture2D.MipSlice = 0;
		depthStencilViewDesc.Format = DXGI_FORMAT_UNKNOWN;
		depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		depthStencilViewDesc.Flags = 0;

		status = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView);
		
		if (FAILED(status))
		{
			WriteToConsole("error 0x%X: could not create depth buffer view\n", status);
		}
	}

	if (SUCCEEDED(status))
	{
		D3D11_VIEWPORT viewport;
		ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
		viewport.Height = (float) bbDesc.Height;
		viewport.Width = (float) bbDesc.Width;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		m_pContext->RSSetViewports(1, &viewport);
	}

	if (SUCCEEDED(status))
	{
		status = CompileShaders();
	}

	return status;
}

VOID Renderer::Uninitialize()
{
	m_pDepthStencilView->Release();
	m_pDepthStencilBuffer->Release();
	m_pRenderTarget->Release();
	m_pBackBuffer->Release();
	m_pSwapChain->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_pContext->Release();
	m_DebugInterface->Release();
	m_pDevice->Release();
}

INT Renderer::CompileShader(const char* pSrcData, size_t SrcDataSize, const char* pSourceName, const char* pEntrypoint, const char* pTarget, ID3DBlob** ppCode)
{
	INT status = STATUS_SUCCESS;
	ID3DBlob* errorBlob = NULL;

	status = D3DCompile(pSrcData, SrcDataSize, pSourceName, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, ppCode, &errorBlob);;

	if (FAILED(status))
	{
		if (errorBlob != NULL)
		{
			WriteToConsole("%s\n", errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
	}

	return status;
}

INT Renderer::CompileShaders()
{
	INT status = STATUS_SUCCESS;

	ID3DBlob* vs_blob = NULL;
	ID3DBlob* ps_blob = NULL;

	if (SUCCEEDED(status))
	{
		status = CompileShader(Data::VERTEX_SHADER, sizeof(Data::VERTEX_SHADER), "vertex_shader", "main", "vs_5_0", &vs_blob);
	}

	if (SUCCEEDED(status))
	{
		status = CompileShader(Data::PIXEL_SHADER, sizeof(Data::PIXEL_SHADER), "pixel_shader", "main", "ps_5_0", &ps_blob);
	}

	if (SUCCEEDED(status))
	{
		m_pDevice->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, &m_VertexShader);
	}

	if (SUCCEEDED(status))
	{
		m_pDevice->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, &m_PixelShader);
	}

	if (FAILED(status))
	{
		if (vs_blob != NULL)
		{
			vs_blob->Release();
		}

		if (ps_blob != NULL)
		{
			ps_blob->Release();
		}
	}

	return status;
}

INT main(INT argc, CHAR* argv[])
{
	INT status = STATUS_SUCCESS;

	Window window;
	Renderer renderer;

	status = window.Initialize();

	if (SUCCEEDED(status))
	{
		status = renderer.Initialize(window.GetWindowHandle());
	}

	if (SUCCEEDED(status))
	{
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
	}

	renderer.Uninitialize();
	window.Uninitialize();

	return status;
}
