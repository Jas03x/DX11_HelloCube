
// prevent redefinition of NTSTATUS messages
#define UMDF_USING_NTSTATUS

// prevent minwindef from defining min/max
#define NOMINMAX

#include <Windows.h>

#include <ntstatus.h>

#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>

#define _USE_MATH_DEFINES
#include <cmath>

#include <random>
#include <string>

enum {
	WIDTH  = 512,
	HEIGHT = 512
};

const CHAR* CLASS_NAME  = "DxClass";
const CHAR* WINDOW_NAME = "DirectX";

VOID WriteToConsole(const char* fmt, ...)
{
	enum { BUFFER_SIZE = 4096 };

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

namespace Matrix
{
	void ToIdentity(float* m)
	{
		ZeroMemory(m, sizeof(float) * 16);

		float(*_m)[4] = reinterpret_cast<float(*)[4]>(m);
		_m[0][0] = 1.0f;
		_m[1][1] = 1.0f;
		_m[2][2] = 1.0f;
		_m[3][3] = 1.0f;
	}

	void Copy(float* m0, const float* m1)
	{
		CopyMemory(m0, m1, sizeof(float) * 16);
	}

	void Multiply(const float* m0, const float* m1, float* m2)
	{
		const float(*_m0)[4] = reinterpret_cast<const float(*)[4]>(m0);
		const float(*_m1)[4] = reinterpret_cast<const float(*)[4]>(m1);
		float(*_m2)[4] = reinterpret_cast<float(*)[4]>(m2);

	#define _DOT4(x0, y0, z0, w0, x1, y1, z1, w1) ((x0) * (x1) + (y0) * (y1) + (z0) * (z1) + (w0) * (w1))
	#define _DOT(m, m0, m1, r, c) (m)[c][r] = _DOT4((m0)[0][r], (m0)[1][r], (m0)[2][r], (m0)[3][r], (m1)[c][0], (m1)[c][1], (m1)[c][2], (m1)[c][3])

		_DOT(_m2, _m0, _m1, 0, 0);
		_DOT(_m2, _m0, _m1, 0, 1);
		_DOT(_m2, _m0, _m1, 0, 2);
		_DOT(_m2, _m0, _m1, 0, 3);

		_DOT(_m2, _m0, _m1, 1, 0);
		_DOT(_m2, _m0, _m1, 1, 1);
		_DOT(_m2, _m0, _m1, 1, 2);
		_DOT(_m2, _m0, _m1, 1, 3);

		_DOT(_m2, _m0, _m1, 2, 0);
		_DOT(_m2, _m0, _m1, 2, 1);
		_DOT(_m2, _m0, _m1, 2, 2);
		_DOT(_m2, _m0, _m1, 2, 3);

		_DOT(_m2, _m0, _m1, 3, 0);
		_DOT(_m2, _m0, _m1, 3, 1);
		_DOT(_m2, _m0, _m1, 3, 2);
		_DOT(_m2, _m0, _m1, 3, 3);

	#undef _DOT
	#undef _DOT4
	}
}

namespace Data
{
	const char VERTEX_SHADER[] =
		"														\n"
		"	cbuffer MatrixBuffer : register(b0)					\n"
		"	{													\n"
		"		matrix model_matrix;							\n"
		"	};													\n"
		"														\n"
		"	struct VS_INPUT										\n"
		"	{													\n"
		"		float3 vertex : POSITION;						\n"
		"		float3 color  : COLOR0;							\n"
		"	};													\n"
		"														\n"
		"	struct VS_OUTPUT									\n"
		"	{													\n"
		"		float4 vertex : SV_POSITION;					\n"
		"		float4 color  : COLOR0;							\n"
		"	};													\n"
		"														\n"
		"	VS_OUTPUT main(VS_INPUT input)						\n"
		"	{													\n"
		"		VS_OUTPUT Output;								\n"
		"														\n"
		"		float4 vertex = float4(input.vertex, 1.0);		\n"
		"		vertex = mul(vertex, model_matrix);				\n"
		"														\n"
		"		Output.vertex = vertex;							\n"
		"		Output.color  = float4(input.color, 1.0);		\n"
		"		return Output;									\n"
		"	}													\n";

	const char PIXEL_SHADER[] =
		"														\n"
		"	struct PS_INPUT										\n"
		"	{													\n"
		"		float4 vertex : SV_POSITION;					\n"
		"		float4 color  : COLOR0;							\n"
		"	};													\n"
		"														\n"
		"	struct PS_OUTPUT									\n"
		"	{													\n"
		"		float4 color  : SV_TARGET;						\n"
		"	};													\n"
		"														\n"
		"	PS_OUTPUT main(PS_INPUT input)						\n"
		"	{													\n"
		"		PS_OUTPUT Output;								\n"
		"														\n"
		"		Output.color = input.color;						\n"
		"		return Output;									\n"
		"	}													\n";

	const float ClearColor[4] = { 1.0, 1.0, 1.0, 1.0 };

	struct MatrixBuffer
	{
		float model_matrix[16];
	};

	struct Vertex
	{
		float position[3];
		float color[3];
	};

	/*
	*      2 ______________ 3
	*       /|            /|
	*      / |           / |
	*   6 /__|__________/ 7|
	*    |	 |          |  |
	*    | 0 |__________|__| 1
	*    |	/           |  /
	*    | /            | /
	*    |/_____________|/
	*    4              5
	*/

	#define V0 { -0.5, -0.5, -0.5 }
	#define V1 { +0.5, -0.5, -0.5 }
	#define V2 { -0.5, +0.5, -0.5 }
	#define V3 { +0.5, +0.5, -0.5 }
	#define V4 { -0.5, -0.5, +0.5 }
	#define V5 { +0.5, -0.5, +0.5 }
	#define V6 { -0.5, +0.5, +0.5 }
	#define V7 { +0.5, +0.5, +0.5 }

	#define C0 { 1.0, 0.0, 0.0 }
	#define C1 { 0.0, 1.0, 0.0 }
	#define C2 { 0.0, 0.0, 1.0 }
	#define C3 { 0.5, 0.5, 0.0 }
	#define C4 { 0.5, 0.0, 0.5 }
	#define C5 { 0.0, 0.5, 0.5 }

	const Vertex Vertices[] =
	{
		// front
		{ V6, C0 }, { V5, C0 }, { V4, C0 },
		{ V5, C0 }, { V6, C0 }, { V7, C0 },

		// back
		{ V2, C1 }, { V1, C1 }, { V0, C1 },
		{ V1, C1 }, { V2, C1 }, { V3, C1 },

		// top
		{ V6, C2 }, { V2, C2 }, { V7, C2 },
		{ V2, C2 }, { V3, C2 }, { V7, C2 },

		// bottom
		{ V4, C3 }, { V0, C3 }, { V5, C3 },
		{ V0, C3 }, { V1, C3 }, { V5, C3 },

		// right
		{ V5, C4 }, { V7, C4 }, { V1, C4 },
		{ V1, C4 }, { V7, C4 }, { V3, C4 },

		// left
		{ V4, C5 }, { V6, C5 }, { V0, C5 },
		{ V0, C5 }, { V6, C5 }, { V2, C5 }
	};
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
	ID3D11Device*              m_pDevice;
	ID3D11DeviceContext*       m_pContext;
	IDXGISwapChain*            m_pSwapChain;
	ID3D11Texture2D*           m_pBackBuffer;
	ID3D11RenderTargetView*    m_pRenderTargetView;
	ID3D11Texture2D*           m_pDepthStencilBuffer;
	ID3D11DepthStencilView*    m_pDepthStencilView;
	ID3D11Debug*               m_pDebugInterface;
	ID3D11VertexShader*		   m_pVertexShader;
	ID3D11PixelShader*		   m_pPixelShader;
	ID3D11Buffer*              m_pVertexBuffer;
	ID3D11Buffer*              m_pMatrixBuffer;
	ID3D11InputLayout*         m_pVertexShaderInputLayout;

	unsigned int               m_VertexCount;
	
	unsigned int               m_FrameTracker;
	float                      m_RotationMatrix[16];
	Data::MatrixBuffer         m_MatrixBuffer;

	std::default_random_engine m_Generator;

public:
	Renderer();

	INT  Initialize(HWND hWnd);
	VOID Uninitialize();

	INT  Update();
	INT  Render();

private:
	INT CompileShader(const char* pSrcData, size_t SrcDataSize, const char* pSourceName, const char* pEntrypoint, const char* pTarget, ID3DBlob** ppCode);

	INT CompileShaders();
	INT GenerateBuffers();
};

Renderer::Renderer()
{
	m_pDevice = NULL;
	m_pContext = NULL;
	m_pSwapChain = NULL;
	m_pBackBuffer = NULL;
	m_pRenderTargetView = NULL;
	m_pDepthStencilBuffer = NULL;
	m_pDepthStencilView = NULL;
	m_pDebugInterface = NULL;
	m_pVertexShader = NULL;
	m_pPixelShader = NULL;
	m_pVertexBuffer = NULL;
	m_pMatrixBuffer = NULL;
	m_pVertexShaderInputLayout = NULL;

	m_VertexCount = 0;
	m_FrameTracker = 0xFFFFFFFF;

	Matrix::ToIdentity(m_RotationMatrix);
	Matrix::ToIdentity(m_MatrixBuffer.model_matrix);
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
		m_pDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&m_pDebugInterface));
	
		if (m_pDebugInterface == NULL)
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
		status = m_pDevice->CreateRenderTargetView(m_pBackBuffer, NULL, &m_pRenderTargetView);

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

	if (SUCCEEDED(status))
	{
		status = GenerateBuffers();
	}

	return status;
}

VOID Renderer::Uninitialize()
{
	if (m_pVertexShaderInputLayout != NULL)
	{
		m_pVertexShaderInputLayout->Release();
		m_pVertexShaderInputLayout = NULL;
	}

	if (m_pVertexBuffer != NULL)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = NULL;
	}

	if (m_pMatrixBuffer != NULL)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = NULL;
	}

	if (m_pDepthStencilView != NULL)
	{
		m_pDepthStencilView->Release();
		m_pDepthStencilView = NULL;
	}

	if (m_pDepthStencilBuffer != NULL)
	{
		m_pDepthStencilBuffer->Release();
		m_pDepthStencilBuffer = NULL;
	}

	if (m_pRenderTargetView != NULL)
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = NULL;
	}

	if (m_pBackBuffer != NULL)
	{
		m_pBackBuffer->Release();
		m_pBackBuffer = NULL;
	}

	if (m_pSwapChain != NULL)
	{
		m_pSwapChain->Release();
		m_pSwapChain = NULL;
	}

	if (m_pVertexShader != NULL)
	{
		m_pVertexShader->Release();
		m_pVertexShader = NULL;
	}

	if (m_pPixelShader != NULL)
	{
		m_pPixelShader->Release();
		m_pPixelShader = NULL;
	}

	if (m_pContext != NULL)
	{
		m_pContext->Release();
		m_pContext = NULL;
	}

	if (m_pDebugInterface != NULL)
	{
		m_pDebugInterface->Release();
		m_pDebugInterface = NULL;
	}

	if (m_pDevice != NULL)
	{
		m_pDevice->Release();
		m_pDevice = NULL;
	}
}

INT Renderer::CompileShader(const char* pSrcData, size_t SrcDataSize, const char* pSourceName, const char* pEntrypoint, const char* pTarget, ID3DBlob** ppCode)
{
	INT status = STATUS_SUCCESS;
	ID3DBlob* errorBlob = NULL;

	status = D3DCompile(pSrcData, SrcDataSize, pSourceName, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, pEntrypoint, pTarget, D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, ppCode, &errorBlob);

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

	// compile the vertex shader
	if (SUCCEEDED(status))
	{
		status = CompileShader(Data::VERTEX_SHADER, sizeof(Data::VERTEX_SHADER), "vertex_shader", "main", "vs_5_0", &vs_blob);
	}

	// create the vertex shader
	if (SUCCEEDED(status))
	{
		m_pDevice->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), NULL, &m_pVertexShader);
	}

	// create the vertex shader's input layout
	if (SUCCEEDED(status))
	{
		D3D11_INPUT_ELEMENT_DESC iaDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,                 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		status = m_pDevice->CreateInputLayout(iaDesc, ARRAYSIZE(iaDesc), vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &m_pVertexShaderInputLayout);
	}

	// create the buffer for the vertex shader's matrix constant buffer
	if (SUCCEEDED(status))
	{
		D3D11_BUFFER_DESC cbDesc;
		cbDesc.ByteWidth = sizeof(Data::MatrixBuffer);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.CPUAccessFlags = 0;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		status = m_pDevice->CreateBuffer(&cbDesc, NULL, &m_pMatrixBuffer);
	}

	// compile the pixel shader
	if (SUCCEEDED(status))
	{
		status = CompileShader(Data::PIXEL_SHADER, sizeof(Data::PIXEL_SHADER), "pixel_shader", "main", "ps_5_0", &ps_blob);
	}

	// create the pixel shader
	if (SUCCEEDED(status))
	{
		m_pDevice->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), NULL, &m_pPixelShader);
	}

	// free the blobs
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

INT Renderer::GenerateBuffers()
{
	INT status = STATUS_SUCCESS;

	D3D11_BUFFER_DESC vDesc;
	vDesc.ByteWidth = sizeof(Data::Vertices);
	vDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vDesc.Usage = D3D11_USAGE_DEFAULT;
	vDesc.CPUAccessFlags = 0;
	vDesc.MiscFlags = 0;
	vDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vData;
	vData.pSysMem = Data::Vertices;
	vData.SysMemPitch = 0;
	vData.SysMemSlicePitch = 0;

	m_VertexCount = ARRAYSIZE(Data::Vertices);

	if (SUCCEEDED(status))
	{
		status = m_pDevice->CreateBuffer(&vDesc, &vData, &m_pVertexBuffer);
	}

	return status;
}

INT Renderer::Update()
{
	INT status = STATUS_SUCCESS;

	const unsigned int ROTATION_INTERVAL = 180; // the rotation changes every 180 frames

	if (m_FrameTracker < ROTATION_INTERVAL)
	{
		float current_rotation[16];
		Matrix::Copy(current_rotation, m_MatrixBuffer.model_matrix);

		Matrix::Multiply(current_rotation, m_RotationMatrix, m_MatrixBuffer.model_matrix);

		m_FrameTracker++;
	}
	else
	{
		float r_x = (static_cast<float>(m_Generator()) / static_cast<float>(m_Generator.max())) * M_PI / ROTATION_INTERVAL;
		float r_y = (static_cast<float>(m_Generator()) / static_cast<float>(m_Generator.max())) * M_PI / ROTATION_INTERVAL;
		float r_z = (static_cast<float>(m_Generator()) / static_cast<float>(m_Generator.max())) * M_PI / ROTATION_INTERVAL;

		float c_x = std::cos(r_x), s_x = std::sin(r_x);
		float c_y = std::cos(r_y), s_y = std::sin(r_y);
		float c_z = std::cos(r_z), s_z = std::sin(r_z);

		float rotation[16] = {
			c_x * c_y, c_x * s_y * s_z - s_x * c_z, c_x * s_y * c_z + s_x * s_z, 0,
			s_x * c_y, s_x * s_y * s_z + c_x * c_z, s_x * s_y * c_z - c_x * s_z, 0,
			-s_y, c_y * s_z, c_y * c_z, 0,
			0, 0, 0, 1
		};

		Matrix::Copy(m_RotationMatrix, rotation);

		m_FrameTracker = 0;
	}

	return status;
}

INT Renderer::Render()
{
	INT status = STATUS_SUCCESS;

	m_pContext->UpdateSubresource(m_pMatrixBuffer, 0, NULL, &m_MatrixBuffer, 0, 0);

	m_pContext->ClearRenderTargetView(m_pRenderTargetView, Data::ClearColor);
	m_pContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0);

	m_pContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	UINT strides[] = { sizeof(Data::Vertex) };
	UINT offsets[] = { 0 };
	m_pContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, strides, offsets);
	m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pContext->IASetInputLayout(m_pVertexShaderInputLayout);

	m_pContext->VSSetShader(m_pVertexShader, NULL, 0);
	m_pContext->VSSetConstantBuffers(0, 1, &m_pMatrixBuffer);
	
	m_pContext->PSSetShader(m_pPixelShader,  NULL, 0);

	m_pContext->Draw(m_VertexCount, 0);

	m_pSwapChain->Present(1, 0);

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
				renderer.Update();
				renderer.Render();
			}
		}
	}

	renderer.Uninitialize();
	window.Uninitialize();

	return status;
}
