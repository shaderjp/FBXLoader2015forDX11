// *********************************************************************************************************************
/// 
/// @file 		FBXLoaderSample.cpp
/// @brief		FBX描画サンプル
/// 
/// @author 	Masafumi Takahashi
/// @date 		2014/05/15
/// 
// *********************************************************************************************************************

#include "stdafx.h"

#include <windows.h>
#include <d3d11.h>
#include <directxmath.h>
#include "DDSTextureLoader.h"
#include "resource.h"
#include <d3dcompiler.h>

#include <SpriteFont.h>

#include "CFBXRendererDX11.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = NULL;
HWND                                g_hWnd = NULL;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = NULL;
ID3D11DeviceContext*                g_pImmediateContext = NULL;
IDXGISwapChain*                     g_pSwapChain = NULL;
ID3D11RenderTargetView*             g_pRenderTargetView = NULL;
ID3D11Texture2D*                    g_pDepthStencil = NULL;
ID3D11DepthStencilView*             g_pDepthStencilView = NULL;
ID3D11DepthStencilState*			g_pDepthStencilState = NULL;

XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;
XMFLOAT4                            g_vMeshColor(0.7f, 0.7f, 0.7f, 1.0f);


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();

// 追加記述
const DWORD	NUMBER_OF_MODELS = 3;

HRESULT InitApp();
void CleanupApp();
void UpdateApp();
HRESULT SetupTransformSRV();
void	SetMatrix();
FBX_LOADER::CFBXRenderDX11*	g_pFbxDX11[NUMBER_OF_MODELS];
char g_files[NUMBER_OF_MODELS][256] =
{
	"Assets\\model1.fbx",
	"Assets\\model2.fbx",
	"Assets\\model3.fbx",
};

struct CBFBXMATRIX
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProj;
	XMMATRIX mWVP;
};
ID3D11BlendState*				g_pBlendState = nullptr;
ID3D11RasterizerState*			g_pRS = nullptr;
ID3D11Buffer*					g_pcBuffer = nullptr;
ID3D11VertexShader*                 g_pvsFBX = nullptr;
ID3D11PixelShader*                  g_ppsFBX = nullptr;

// Instancing
bool	g_bInstancing = false;
struct SRVPerInstanceData
{
	XMMATRIX mWorld;
};
const uint32_t g_InstanceMAX = 32;
ID3D11VertexShader*             g_pvsFBXInstancing = nullptr;
ID3D11Buffer*					g_pTransformStructuredBuffer = nullptr;
ID3D11ShaderResourceView*		g_pTransformSRV = nullptr;

DirectX::SpriteBatch*		g_pSpriteBatch = nullptr;
DirectX::SpriteFont*		g_pFont = nullptr;

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupApp();
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}

	CleanupApp();
	CleanupDevice();

	return (int) msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR) IDI_FBX2015LOADER4DX11);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR) IDI_FBX2015LOADER4DX11);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 FBX Sample", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*) pErrorBlob->GetBufferPointer());
		if (pErrorBlob) pErrorBlob->Release();
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes [] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels [] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*) &pBackBuffer);
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	// Create depth stencil state
	D3D11_DEPTH_STENCIL_DESC descDSS;
	ZeroMemory(&descDSS, sizeof(descDSS));
	descDSS.DepthEnable = TRUE;
	descDSS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDSS.DepthFunc = D3D11_COMPARISON_LESS;
	descDSS.StencilEnable = FALSE;
	hr = g_pd3dDevice->CreateDepthStencilState(&descDSS, &g_pDepthStencilState);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT) width;
	vp.Height = (FLOAT) height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// Initialize the world matrices
	g_World = XMMatrixIdentity();

	hr = InitApp();
	if (FAILED(hr))
		return hr;
	hr = SetupTransformSRV();
	if (FAILED(hr))
		return hr;

	return S_OK;
}

// FBX描画用初期化
HRESULT InitApp()
{
	HRESULT hr = S_OK;

	// FBXの読み取り
	// 注：頂点バッファやインデックスバッファはこの時点で生成されるがInputLayoutは作成しない

	for (DWORD i = 0; i<NUMBER_OF_MODELS; i++)
	{
		g_pFbxDX11[i] = new FBX_LOADER::CFBXRenderDX11;
		hr = g_pFbxDX11[i]->LoadFBX(g_files[i], g_pd3dDevice);
	}
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"FBX Error", L"Error", MB_OK);
		return hr;
	}

	// Compile the vertex shader
	ID3DBlob* pVSBlob = NULL;
	hr = CompileShaderFromFile(L"simpleRenderVS.hlsl", "vs_main", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pvsFBX);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Compile the vertex shader
	pVSBlob->Release();
	hr = CompileShaderFromFile(L"simpleRenderInstancingVS.hlsl", "vs_main", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pvsFBXInstancing);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}


	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout [] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Todo: InputLayoutの作成には頂点シェーダが必要なのでこんなタイミングでCreateするのをなんとかしたい
	for (DWORD i = 0; i<NUMBER_OF_MODELS; i++)
	{
		hr = g_pFbxDX11[i]->CreateInputLayout(g_pd3dDevice, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), layout, numElements);
	}
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Compile the pixel shader
	ID3DBlob* pPSBlob = NULL;
	hr = CompileShaderFromFile(L"simpleRenderPS.hlsl", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(NULL,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_ppsFBX);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Create Constant Buffer
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(CBFBXMATRIX);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pcBuffer);
	if (FAILED(hr))
		return hr;

	//
	D3D11_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
	rsDesc.FillMode = D3D11_FILL_SOLID;
	rsDesc.CullMode = D3D11_CULL_BACK;
	rsDesc.FrontCounterClockwise = false;
	rsDesc.DepthClipEnable = FALSE;
	g_pd3dDevice->CreateRasterizerState(&rsDesc, &g_pRS);
	g_pImmediateContext->RSSetState(g_pRS);

	D3D11_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
	blendDesc.AlphaToCoverageEnable = false;
	blendDesc.IndependentBlendEnable = false;
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;      ///tryed D3D11_BLEND_ONE ... (and others desperate combinations ... )
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;     ///tryed D3D11_BLEND_ONE ... (and others desperate combinations ... )
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	g_pd3dDevice->CreateBlendState(&blendDesc, &g_pBlendState);

	// SpriteBatch
	g_pSpriteBatch = new DirectX::SpriteBatch(g_pImmediateContext);
	// SpriteFont
	g_pFont = new DirectX::SpriteFont(g_pd3dDevice, L"Assets\\Arial.spritefont");


	return hr;
}

//
HRESULT SetupTransformSRV()
{
	HRESULT hr = S_OK;

	const uint32_t count = g_InstanceMAX;
	const uint32_t stride = static_cast<uint32_t>(sizeof(SRVPerInstanceData));

	// Create StructuredBuffer
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = stride * count;
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = stride;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pTransformStructuredBuffer);
	if (FAILED(hr))
		return hr;

	// Create ShaderResourceView
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;   // 拡張されたバッファーであることを指定する
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.BufferEx.NumElements = count;                  // リソース内の要素の数

	// 構造化バッファーをもとにシェーダーリソースビューを作成する
	hr = g_pd3dDevice->CreateShaderResourceView(g_pTransformStructuredBuffer, &srvDesc, &g_pTransformSRV);
	if (FAILED(hr))
		return hr;

	return hr;
}

//
void CleanupApp()
{
	if (g_pSpriteBatch)
	{
		delete g_pSpriteBatch;
		g_pSpriteBatch = nullptr;
	}
	if (g_pFont)
	{
		delete g_pFont;
		g_pFont = nullptr;
	}

	if (g_pTransformSRV)
	{
		g_pTransformSRV->Release();
		g_pTransformSRV = nullptr;
	}
	if (g_pTransformStructuredBuffer)
	{
		g_pTransformStructuredBuffer->Release();
		g_pTransformStructuredBuffer = nullptr;
	}

	if (g_pBlendState)
	{
		g_pBlendState->Release();
		g_pBlendState = nullptr;
	}

	for (DWORD i = 0; i<NUMBER_OF_MODELS; i++)
	{
		if (g_pFbxDX11[i])
		{
			delete g_pFbxDX11[i];
			g_pFbxDX11[i] = nullptr;
		}
	}

	if (g_pRS)
	{
		g_pRS->Release();
		g_pRS = nullptr;
	}

	if (g_pvsFBXInstancing)
	{
		g_pvsFBX->Release();
		g_pvsFBX = nullptr;
	}

	if (g_pvsFBX)
	{
		g_pvsFBX->Release();
		g_pvsFBX = nullptr;
	}

	if (g_ppsFBX)
	{
		g_ppsFBX->Release();
		g_ppsFBX = nullptr;
	}
	if (g_pcBuffer)
	{
		g_pcBuffer->Release();
		g_pcBuffer = nullptr;
	}
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pDepthStencilState) g_pDepthStencilState->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_KEYUP:
		if (wParam == VK_F2)
		{
			g_bInstancing = !g_bInstancing;
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//
void SetMatrix()
{
	HRESULT hr = S_OK;
	const uint32_t count = g_InstanceMAX;
	const float offset = -(g_InstanceMAX*60.0f / 2.0f);
	XMMATRIX mat;

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	hr = g_pImmediateContext->Map(g_pTransformStructuredBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

	SRVPerInstanceData*	pSrvInstanceData = (SRVPerInstanceData*) MappedResource.pData;

	for (uint32_t i = 0; i<count; i++)
	{
		mat = XMMatrixTranslation(0, 0, i*60.0f + offset);
		pSrvInstanceData[i].mWorld = (mat);
	}

	g_pImmediateContext->Unmap(g_pTransformStructuredBuffer, 0);
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;


	// Initialize the world matrices
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 150.0f, -350.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 150.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT) height, 0.01f, 10000.0f);

	// Update our time
	static float t = 0.0f;
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float) XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
	}

	// Rotate cube around the origin
	g_World = XMMatrixRotationY(t);

	//
	// Clear the back buffer
	//
	float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red, green, blue, alpha
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
	//
	// Clear the depth buffer to 1.0 (max depth)
	//
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	float blendFactors[4] = { D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO };
	g_pImmediateContext->RSSetState(g_pRS);
	g_pImmediateContext->OMSetBlendState(g_pBlendState, blendFactors, 0xffffffff);
	g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilState, 0);

	// モデルを順番に描画
	for (DWORD i = 0; i<NUMBER_OF_MODELS; i++)
	{
		// FBX Modelのnode数を取得
		size_t nodeCount = g_pFbxDX11[i]->GetNodeCount();

		ID3D11VertexShader* pVS = g_bInstancing ? g_pvsFBXInstancing : g_pvsFBX;
		g_pImmediateContext->VSSetShader(pVS, NULL, 0);
		g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pcBuffer);
		g_pImmediateContext->PSSetShader(g_ppsFBX, NULL, 0);

		// 全ノードを描画
		for (size_t j = 0; j<nodeCount; j++)
		{

			XMMATRIX mLocal;
			g_pFbxDX11[i]->GetNodeMatrix(j, &mLocal.r[0].m128_f32[0]);	// このnodeのMatrix

			D3D11_MAPPED_SUBRESOURCE MappedResource;
			g_pImmediateContext->Map(g_pcBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

			CBFBXMATRIX*	cbFBX = (CBFBXMATRIX*) MappedResource.pData;

			// 左手系
			cbFBX->mWorld = (g_World);
			cbFBX->mView = (g_View);
			cbFBX->mProj = (g_Projection);

			cbFBX->mWVP = XMMatrixTranspose(mLocal*g_World*g_View*g_Projection);

			g_pImmediateContext->Unmap(g_pcBuffer, 0);

			SetMatrix();

			FBX_LOADER::MATERIAL_DATA material = g_pFbxDX11[i]->GetNodeMaterial(j);

			if (material.pMaterialCb)
				g_pImmediateContext->UpdateSubresource(material.pMaterialCb, 0, NULL, &material.materialConstantData, 0, 0);

			g_pImmediateContext->VSSetShaderResources(0, 1, &g_pTransformSRV);
			g_pImmediateContext->PSSetShaderResources(0, 1, &material.pSRV);
			g_pImmediateContext->PSSetConstantBuffers(0, 1, &material.pMaterialCb);
			g_pImmediateContext->PSSetSamplers(0, 1, &material.pSampler);

			if (g_bInstancing)
				g_pFbxDX11[i]->RenderNodeInstancing(g_pImmediateContext, j, g_InstanceMAX);
			else
				g_pFbxDX11[i]->RenderNode(g_pImmediateContext, j);
		}
	}

	// Text
	WCHAR wstr[512];
	g_pSpriteBatch->Begin();
	g_pFont->DrawString(g_pSpriteBatch, L"FBX Loader : F2 Change Render Mode", XMFLOAT2(0, 0), DirectX::Colors::Yellow, 0, XMFLOAT2(0, 0), 0.5f);

	if (g_bInstancing)
		swprintf_s(wstr, L"Render Mode: Instancing");
	else
		swprintf_s(wstr, L"Render Mode: Single Draw");

	g_pFont->DrawString(g_pSpriteBatch, wstr, XMFLOAT2(0, 16), DirectX::Colors::Yellow, 0, XMFLOAT2(0, 0), 0.5f);
	g_pSpriteBatch->End();

	g_pImmediateContext->VSSetShader(NULL, NULL, 0);
	g_pImmediateContext->PSSetShader(NULL, NULL, 0);

	//
	// Present our back buffer to our front buffer
	//
	g_pSwapChain->Present(0, 0);
}
