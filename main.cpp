
/*
8-bit vs 10-bit colour test.
Windows 7: No DXGI 1.2, no DX11.1
Windows 10/11 is absolute shit as suspected. "fullscreen optimizations" nonsense preventing 10-bit.

~ DISABLEDXMAXIMIZEDWINDOWEDMODE HIGHDPIAWARE
Computer\HKEY_CURRENT_USER\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers
*/


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")


#define NOCRYPT
#define NOSOUND
#define NOMENU
#define NOCOMM
#define NOHELP
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN

#include <Windows.h>
#include <iostream>
#include <string>
#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <VersionHelpers.h>


const DXGI_MODE_SCALING SCALING_MODE = DXGI_MODE_SCALING_UNSPECIFIED; //DXGI_MODE_SCALING_CENTERED DXGI_MODE_SCALING_UNSPECIFIED
const DXGI_SCALING SCALING = DXGI_SCALING_STRETCH; //DXGI_SCALING_NONE
const DXGI_FORMAT FORMAT = DXGI_FORMAT_R10G10B10A2_UNORM;	//DXGI_FORMAT_R16G16B16A16_FLOAT;

//Global Direct3D values

DWORD FullscreenState = 0; //1==to fs, 2==back to window


IDXGIFactory2* factory;	//latest and greatest. Direct2D support at 10-bit theoretically possible using BRGA
ID3D11Device1* device1;		//query interface
IDXGISwapChain1* swapChain1;
ID3D11DeviceContext1* context1;
IDXGIOutput1* output1;
IDXGIOutput* output;
IDXGIAdapter1* card;
ID3D11DeviceContext* context;
ID3D11Device* device;
D3D_FEATURE_LEVEL selectedlevel;
ID3D11RenderTargetView* renderTarget;
ID3D11Texture2D* pBackBuffer;
ID3D11VertexShader* myvertex;
ID3D11VertexShader* myvertex2;
ID3D11PixelShader* mypixel;
ID3D11PixelShader* mypixel2;
ID3DBlob* vertexData;
ID3DBlob* vertexData2;
ID3DBlob* pixelData;
ID3DBlob* pixelData2;
ID3D11InputLayout* inputlayout;
ID3D11InputLayout* inputlayout2;
ID3D11Buffer* vertexBuffer;
ID3D11Buffer* indexBuffer;
ID3D11Buffer* cBuffer;

//Create writing using texture
ID3D11Texture2D* texture1;
ID3D11ShaderResourceView* resourceview;
ID3D11Resource* resource;
ID3D11SamplerState* sampler;

DWORD height = 1080;
DWORD width = 1920;

bool UpdateRegistry()
{
	if (IsWindows8OrGreater() == false)	//windows 10.11
	{
		return(false);
	}
	char buffer[256] = { 0 };
	char data[256] = { 0 };
	char setting[] = "~ DISABLEDXMAXIMIZEDWINDOWEDMODE HIGHDPIAWARE";
	GetModuleFileNameA(NULL, (LPSTR)buffer, 256);
	HKEY key;
	DWORD size = 0;
	DWORD type = REG_SZ;
	LSTATUS err;
	err = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",0, KEY_READ | KEY_QUERY_VALUE | KEY_SET_VALUE,&key);
	if (err != ERROR_SUCCESS)
	{
		MessageBoxA(0, std::to_string(err).c_str(), "Error open reg", 0);
		RegCloseKey(key);
		return(false);
	}
	RegQueryValueExA(key, buffer, 0, &type, 0, &size);
	RegQueryValueExA(key, buffer, 0, &type, (LPBYTE)data, &size);
	if (strncmp(setting, data, sizeof(setting)-1) != 0)
	{
		RegSetValueExA(key, buffer, 0, REG_SZ, (BYTE*)&setting[0], sizeof(setting));
		RegFlushKey(key);
		RegCloseKey(key);
		return(true);
	}
	RegCloseKey(key);
	return(false);
}


bool InitTexture()
{
	HANDLE h = NULL;
	HRESULT hr;
	bool state = false;
	UINT width = 0;
	UINT height = 0;
	UINT bpp = 24;
	UINT size = 0;
	UINT pitch = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	WICPixelFormatGUID wicFormatGUID = GUID_WICPixelFormatUndefined;
	IWICImagingFactory* imageFactory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* framedecode = nullptr;
	IWICBitmapSource* source = nullptr;
	IWICFormatConverter* converter = nullptr;

	ID3D11InfoQueue* info= nullptr;
	D3D11_MESSAGE* msg = (D3D11_MESSAGE*)_aligned_malloc(4096, 16);
	
	device1->QueryInterface(&info);

	BYTE* buffer = (BYTE*)_aligned_malloc(4 * 512 * 512, 32);
	if (buffer == nullptr)
		return(false);
	hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);//start COM
	if (hr != S_OK)
	{
		MessageBoxA(0, "COM fail", 0, 0);
		goto Done;
	}
	hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imageFactory));	// Create WIC factory
	if (FAILED(hr))
	{
		MessageBoxA(0, "Create ImageFactory Fail", 0, 0);
		goto Done;
	}
	hr = imageFactory->CreateFormatConverter(&converter);	//Create Converter
	if (FAILED(hr))
	{
		MessageBoxA(0, "Create Converter Fail.", 0, 0);
		goto Done;
	}
	h = CreateFile(L"8.jpg", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE)
		goto Done;
	hr = imageFactory->CreateDecoderFromFileHandle((ULONG_PTR)h,NULL,WICDecodeMetadataCacheOnLoad,&decoder);//load image into decoder
	if (FAILED(hr))
	{
		MessageBoxA(0, "Create Decoder Fail", 0, 0);
		goto Done;
	}
	hr = decoder->GetFrame(0,&framedecode);	//ok
	if (hr != S_OK)
	{
		MessageBoxA(0, "GetFrame Fail", 0, 0);
		goto Done;
	}
	hr = framedecode->GetSize(&width, &height);//get dimensions of image
	if (hr != S_OK)
	{
		MessageBoxA(0, "GetSize Fail", 0, 0);
		goto Done;
	}
	hr = framedecode->QueryInterface(&source);	//create source from frame
	if (hr != S_OK)
	{
		MessageBoxA(0, "Query source Fail", 0, 0);
		goto Done;
	}
	hr = converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,NULL,0.0f, WICBitmapPaletteTypeCustom);
	if (hr != S_OK)
	{
		MessageBoxA(0, "Converter Fail", 0, 0);
		goto Done;
	}

	bpp = 32;
	pitch = width * bpp / 8;	//bytes per row
	unsigned int mod = pitch % 4;
	if (mod)
	{
		pitch = pitch + (4 - mod);	//alignment dword
	}
	size = pitch * height;		//image size

	hr = converter->CopyPixels(NULL, pitch, size, buffer);	//copy into buffer
	if (hr != S_OK)
	{
		MessageBoxA(0, "Copy to buffer Fail", 0, 0);
		goto Done;
	}
	converter->GetPixelFormat(&wicFormatGUID);	//new converted pixel format

	//Convert image to compatible format
	if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) format = DXGI_FORMAT_R16G16B16A16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) format = DXGI_FORMAT_R8G8B8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) format = DXGI_FORMAT_B8G8R8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) format = DXGI_FORMAT_B8G8R8X8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) format = DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) format = DXGI_FORMAT_R10G10B10A2_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) format = DXGI_FORMAT_B5G5R5A1_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) format = DXGI_FORMAT_B5G6R5_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) format = DXGI_FORMAT_R32_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) format = DXGI_FORMAT_R16_FLOAT;
	else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) format = DXGI_FORMAT_R16_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) format = DXGI_FORMAT_R8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) format = DXGI_FORMAT_A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat24bppRGB) format = DXGI_FORMAT_R8G8B8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat24bppBGR) format = DXGI_FORMAT_R8G8B8A8_UNORM;
	else if (wicFormatGUID == GUID_WICPixelFormat24bpp3Channels) format = DXGI_FORMAT_R8G8B8A8_UNORM;
	else
	{
		MessageBoxA(0, "Bad Format Fail", 0, 0);
		goto Done;
	}

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1; //fails if set to 0
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;// D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;// D3D11_BIND_SHADER_RESOURCE D3D11_BIND_RENDER_TARGET D3D11_BIND_DEPTH_STENCIL D3D11_BIND_DECODER D3D11_BIND_VERTEX_BUFFER
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	D3D11_SUBRESOURCE_DATA subres;
	subres.pSysMem = buffer;
	subres.SysMemPitch = width*4;	//width*bpp ?
	subres.SysMemSlicePitch = size;//3D texture only
	hr = device1->CreateTexture2D(&desc,&subres,&texture1);
	if (hr != S_OK)
	{
		MessageBoxA(0, "Create Texture Fail", 0, 0);
		goto Done;
	}
	

	context1->UpdateSubresource1(texture1,0,NULL,buffer,width*4,1, D3D11_COPY_DISCARD); 	//current stumbling block D3D11_COPY_DISCARD

	D3D11_SHADER_RESOURCE_VIEW_DESC rdesc;
	ZeroMemory(&rdesc, sizeof(rdesc));
	rdesc.Format = format;
	rdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	rdesc.Buffer.ElementOffset = 0;
	rdesc.Buffer.ElementWidth = size;
	rdesc.Buffer.FirstElement = 0;
	rdesc.Buffer.NumElements = 1;
	rdesc.Texture2D.MipLevels = 1;
	rdesc.Texture2D.MostDetailedMip =  0;
	hr = device1->CreateShaderResourceView(texture1, &rdesc, &resourceview);
	if (hr != S_OK)
	{
		MessageBoxA(0, std::string("Create ResourceView Fail" + std::to_string(GetLastError())).c_str(), 0, 0);
		size_t l = 0;

		MessageBoxA(0, std::string("messages " + std::to_string(info->GetNumStoredMessagesAllowedByRetrievalFilter())).c_str(), 0, 0);
		info->GetMessageW(0, NULL, &l);
		info->GetMessageW(0, msg, &l);
		std::string output = msg->pDescription;
		MessageBoxA(0, output.c_str(), 0, 0);
		goto Done;
	}

	context1->GenerateMips(resourceview);

	D3D11_SAMPLER_DESC sdesc;
	
	sdesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;	//D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sdesc.MipLODBias = 0.0f;
	sdesc.MaxAnisotropy = 8;
	sdesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.BorderColor[0] = 0.0f; //RGB
	sdesc.BorderColor[1] = 0.0f;
	sdesc.BorderColor[2] = 0.0f;
	sdesc.BorderColor[3] = 1.0f;
	sdesc.MinLOD = 0.0f;
	sdesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device1->CreateSamplerState(&sdesc,&sampler);
	if (hr != S_OK)
	{
		MessageBoxA(0, "SamplerState failed.", 0, 0);
		goto Done;
	}
	state = true;

Done:
	if(h != NULL)
	CloseHandle(h);
	if (buffer != nullptr)
	_aligned_free(buffer);
	if (msg)
		_aligned_free(msg);
	if (converter)
		converter->Release();
	if(framedecode)
	framedecode->Release();
	if(decoder)
	decoder->Release();
	if(source)
	source->Release();
	if(imageFactory)
	imageFactory->Release();
	return(state);
}

bool InitD3D(HWND wnd)
{
	bool state = false;
	HRESULT hr;

	//Debugging Setup
	//ID3D11InfoQueue* info;
	//D3D11_MESSAGE* msg = (D3D11_MESSAGE*)_aligned_malloc(4096, 16);


	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&factory);	//factory2
	if (hr != S_OK)
	{
		MessageBoxA(0, "Factory_2 fail", 0, 0);
		goto Final;
	}


	
	DXGI_SWAP_CHAIN_DESC1 swapchaindesc1;
	swapchaindesc1.Width = width;
	swapchaindesc1.Height = height;
	swapchaindesc1.Format = FORMAT;// DXGI_FORMAT_R10G10B10A2_UNORM DXGI_FORMAT_R8G8B8A8_UNORM  DXGI_FORMAT_R16G16B16A16_FLOAT
	swapchaindesc1.Stereo = false;
	swapchaindesc1.SampleDesc = DXGI_SAMPLE_DESC{ 1,0 };//samples,quality
	swapchaindesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	//DXGI_USAGE_BACK_BUFFER
	swapchaindesc1.BufferCount = 2;
	swapchaindesc1.Scaling = SCALING; //DXGI_SCALING_NONE DXGI_SCALING_STRETCH
	swapchaindesc1.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
	swapchaindesc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE; //DXGI_ALPHA_MODE_PREMULTIPLIED
	swapchaindesc1.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;// DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	//DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE
	D3D_FEATURE_LEVEL featurelevels[1] = { D3D_FEATURE_LEVEL_11_0 };
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fulldesc;
	fulldesc.RefreshRate.Denominator = 1;	//Creel mistake
	fulldesc.RefreshRate.Numerator = 60;
	fulldesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED; //DXGI_MODE_SCALING_STRETCHED
	fulldesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	fulldesc.Windowed = true; //windowed?

	hr = D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,	//D3D_DRIVER_TYPE_HARDWARE
		(D3D11_CREATE_DEVICE_SINGLETHREADED),	// D3D11_CREATE_DEVICE_DEBUG
		featurelevels,
		1,D3D11_SDK_VERSION,&device,NULL,&context);
	if (hr != S_OK)
	{
		MessageBoxA(0, "Device fail", 0, 0);
		goto Final;
	}

	hr = device->QueryInterface(&device1);	//Obtain device1 interface using query
	if (hr != S_OK)
	{
		MessageBoxA(0, "Query Device1 fail", 0, 0);
		goto Final;
	}

	hr = context->QueryInterface(&context1);
	if (hr != S_OK)
	{
		MessageBoxA(0, "Query Conext1 fail", 0, 0);
		goto Final;
	}

	//hr = device1->QueryInterface(&info);//DEBUG
	if (hr != S_OK)
	{
		MessageBoxA(0, "Debug fail", 0, 0);
		goto Final;
	}

	hr = factory->CreateSwapChainForHwnd(device1, wnd, &swapchaindesc1, &fulldesc, 0, &swapChain1);	//improved swapchain
	if (hr != S_OK)
	{
		MessageBoxA(0, "SwapChain fail", 0, 0);
		goto Final;
	}
	hr = swapChain1->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (hr != S_OK)
	{
		MessageBoxA(0, "Get Buffer fail", 0, 0);
		goto Final;
	}

	hr = device1->CreateRenderTargetView(pBackBuffer, NULL, &renderTarget);
	if (hr != S_OK)
	{
		MessageBoxA(0, "RenderTargetView fail", 0, 0);
		goto Final;
	}
	context1->OMSetRenderTargets(1, &renderTarget, NULL);

	
	IDXGIFactory* parent;
	swapChain1->GetParent(__uuidof(IDXGIFactory),(void**)&parent);
	parent->MakeWindowAssociation(wnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER); //disable auto full-screen

	D3D11_VIEWPORT viewPort;
	ZeroMemory(&viewPort, sizeof(D3D11_VIEWPORT));
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = width;
	viewPort.Height = height;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;
	context1->RSSetViewports(1, &viewPort);
	ID3D11BlendState1* blendState = 0;
	D3D11_BLEND_DESC1 bdesc;
	bdesc.AlphaToCoverageEnable = false;
	bdesc.IndependentBlendEnable = false;
	bdesc.RenderTarget->BlendEnable = false;
	bdesc.RenderTarget->LogicOpEnable = false;
	bdesc.RenderTarget->SrcBlend = D3D11_BLEND_ONE; //D3D11_BLEND_ONE
	bdesc.RenderTarget->DestBlend = D3D11_BLEND_ZERO; //D3D11_BLEND_ZERO
	bdesc.RenderTarget->BlendOp = D3D11_BLEND_OP_ADD; //D3D11_BLEND_OP_ADD
	bdesc.RenderTarget->SrcBlendAlpha = D3D11_BLEND_ONE; //D3D11_BLEND_ONE
	bdesc.RenderTarget->DestBlendAlpha = D3D11_BLEND_ZERO; //D3D11_BLEND_ZERO
	bdesc.RenderTarget->BlendOpAlpha = D3D11_BLEND_OP_ADD; //D3D11_BLEND_OP_ADD
	bdesc.RenderTarget->LogicOp = D3D11_LOGIC_OP_NOOP; //D3D11_LOGIC_OP_NOOP
	bdesc.RenderTarget->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = device1->CreateBlendState1(&bdesc, &blendState);
	if (hr != S_OK)
		goto Final;
	ID3D11RasterizerState* rasterState = 0;
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	rasterDesc.DepthClipEnable = false;
	rasterDesc.ScissorEnable = false;	//requires rectangle
	rasterDesc.MultisampleEnable = false;
	rasterDesc.AntialiasedLineEnable = false;
	hr = device1->CreateRasterizerState(&rasterDesc, &rasterState);
	if (hr != S_OK)
		goto Final;

	context1->RSSetState(rasterState);
	float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	context1->OMSetBlendState(blendState, blendFactor, 0xffffffff);

	hr = D3DReadFileToBlob(L"PixelShader2.cso", &pixelData2);
	if (hr != S_OK)
	{ 
		MessageBoxA(0, "Shader Files Missing", 0, 0);
		goto Final;
	}
	D3DReadFileToBlob(L"PixelShader.cso", &pixelData);
	D3DReadFileToBlob(L"VertexShader2.cso", &vertexData2);
	D3DReadFileToBlob(L"VertexShader.cso", &vertexData);
	device1->CreatePixelShader(pixelData2->GetBufferPointer(), pixelData2->GetBufferSize(), nullptr, &mypixel2);
	device1->CreateVertexShader(vertexData2->GetBufferPointer(), vertexData2->GetBufferSize(), nullptr, &myvertex2);
	device1->CreateVertexShader(vertexData->GetBufferPointer(), vertexData->GetBufferSize(), nullptr, &myvertex);
	device1->CreatePixelShader(pixelData->GetBufferPointer(), pixelData->GetBufferSize(), nullptr, &mypixel);
	//////////////////////////////////////////////////////////////////////////////////
	//Originally only POSITION. Contains Input Parameters for Pixel, Vertex Shaders.
	//////////////////////////////////////////////////////////////////////////////////
	D3D11_INPUT_ELEMENT_DESC input_desc[2];
	input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;	//size of data float3
	input_desc[0].InputSlot = 0;
	input_desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	input_desc[0].InstanceDataStepRate = 0;
	input_desc[0].SemanticName = "POSITION";
	input_desc[0].SemanticIndex = 0;
	input_desc[0].AlignedByteOffset = 0; // CAUSES CRASHING

	input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT; //size of data float2
	input_desc[1].InputSlot = 0;	//still zero
	input_desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	input_desc[1].InstanceDataStepRate = 0;
	input_desc[1].SemanticName = "TEXCOORD";
	input_desc[1].SemanticIndex = 0;	//more than one of the same
	input_desc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;// CAUSES CRASHING D3D11_APPEND_ALIGNED_ELEMENT;
	UINT numElements = sizeof(input_desc) / sizeof(input_desc[0]); // 2
	device1->CreateInputLayout(&input_desc[0], 1, vertexData->GetBufferPointer(), vertexData->GetBufferSize(), &inputlayout);//modified
	hr = device1->CreateInputLayout(&input_desc[0], numElements, vertexData2->GetBufferPointer(), vertexData2->GetBufferSize(), &inputlayout2);//second inputlayout
	if (hr != S_OK)
	{
		MessageBoxA(0, "InputLayout fail", 0, 0);
		goto Final;
	}
	state = true;
Final:
	if (!state)
		MessageBoxA(0, "State failed", "fail", 0);
	/*
	if (!state)
	{
		size_t l = 0;
		info->GetMessageW(0, 0, &l);
		info->GetMessageW(0, msg, &l);
		std::string output = msg->pDescription;
		MessageBoxA(0, output.c_str(), 0, 0);
	}
	if (msg)
		_aligned_free(msg);
			if (info)
		info->Release();
		*/
	if (parent)
		parent->Release();
	if (rasterState)
		rasterState->Release();
	if (blendState)
		blendState->Release();
	if (pBackBuffer)
	pBackBuffer->Release();
	if(pixelData2)
	pixelData2->Release();
	if(pixelData)
	pixelData->Release();
	if(vertexData)
	vertexData->Release();
	vertexData = 0;
	if(vertexData2)
	vertexData2->Release();
	vertexData2 = 0;
	if(context)
	context->Release();
	if(device)
	device->Release();
	return(state);
}



void SetPixelShaderInputVariables(unsigned int offset, float tone, float start, float end, float R = 1.0f,float G = 1.0f,float B = 1.0f)
{
	if(cBuffer)
	cBuffer->Release();
	//constant buffer for parameters to shaders
	struct VS_CONSTANT_BUFFER
	{
		unsigned int count;
		float shade;
		unsigned int a;
		unsigned int b;
		float R, G, B;
		unsigned int padding3;
	};
	VS_CONSTANT_BUFFER data;
	data.count = offset;
	data.shade = tone;
	data.a = start;
	data.b = end;
	data.R = R;
	data.G = G;
	data.B = B;
	D3D11_BUFFER_DESC cDesc;
	cDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
	cDesc.Usage = D3D11_USAGE_DYNAMIC;
	cDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cDesc.MiscFlags = 0;
	cDesc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = &data;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;
	device1->CreateBuffer(&cDesc, &InitData, &cBuffer);
	context1->PSSetConstantBuffers(0, 1, &cBuffer);		//set for PIXEL SHADERS
}

void ClearScreen(float r = 0, float g = 0, float b = 0)
{
	const float BlankColour[4] = { r,b,g, 1.0f };
	context1->ClearRenderTargetView(renderTarget, BlankColour);
}

void PresentFrame()
{
	swapChain1->Present(1, 0);
}

void Draw_Squares(DWORD squares)
{
	context1->OMSetRenderTargets(1, &renderTarget, nullptr);
	context1->IASetInputLayout(inputlayout);
	// Set the vertex and index buffers, and specify the way they define geometry.
	UINT stride = 3 * sizeof(float);	//important.
	UINT offset = 0;
	context1->IASetVertexBuffers(
		0,
		1,
		&vertexBuffer,
		&stride,
		&offset
	);
	context1->IASetIndexBuffer(
		indexBuffer,
		DXGI_FORMAT_R16_UINT,	//short int or int
		0
	);
	context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);//triangletrip means points defined implicitly
	context1->VSSetShader(
		myvertex,
		nullptr,
		0
	);
	context1->PSSetShader(
		mypixel,
		nullptr,
		0
	);
	// Draw the triangle.
	context1->DrawIndexed(
		2 + 2 * squares, // NUMBER OF POINTS
		0,
		0
	);
}

void Create_Squares(float x, float y, float z, float h,float w, DWORD squares)	//create sequence of coloured squares. triangles = squares * 2
{
	DWORD NUM_TRIANGLES = squares * 2;
	if (vertexBuffer)
		vertexBuffer->Release();
	if (indexBuffer)
		indexBuffer->Release();
	float sizeX = w / (float)squares;
	float sizeY = h;
	float x1 = x;
	float y1 = y;
	unsigned int NUM_POINTS = 2 * squares + 2;
	float polyVertices[3 * NUM_POINTS];
	polyVertices[0] = x1;
	polyVertices[1] = y1 - sizeY;
	polyVertices[2] = z;
	polyVertices[3] = x1;
	polyVertices[4] = y1;
	polyVertices[5] = z;
		for (int i = 6; i < squares * 6 + 6; i += 6)
	{	//bottom->top->down  right repeat
		polyVertices[i + 0] = x1 + sizeX;
		polyVertices[i + 1] = y1 - sizeY;
		polyVertices[i + 2] = z;
		polyVertices[i + 3] = x1 + sizeX;
		polyVertices[i + 4] = y1;
		polyVertices[i + 5] = z;
		x1 += sizeX;
	}

	unsigned short polyIndices[2 * squares + 2];
	for (int i = 0; i < (2 * squares + 2); i++)
	{
		polyIndices[i] = i;	//define order of points
	}
	D3D11_BUFFER_DESC vertexBufferDesc = { 0 };
	vertexBufferDesc.ByteWidth = sizeof(float) * 3 * NUM_POINTS;	//total bytes = float size * co-ordinates * points
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vertexBufferData;
	vertexBufferData.pSysMem = &polyVertices[0];
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;
	device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &vertexBuffer);
	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.ByteWidth = sizeof(polyIndices);
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA indexBufferData;
	indexBufferData.pSysMem = polyIndices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	device1->CreateBuffer(&indexBufferDesc, &indexBufferData, &indexBuffer);
}

void Draw_Texture(void)
{
	context1->OMSetRenderTargets(1, &renderTarget, nullptr);
	context1->IASetInputLayout(inputlayout2);
	UINT stride = 5 * sizeof(float);	//important. position + texture
	UINT offset = 0;
	context1->IASetVertexBuffers(
		0,
		1,
		&vertexBuffer,
		&stride,
		&offset
	);
	context1->IASetIndexBuffer(
		indexBuffer,
		DXGI_FORMAT_R16_UINT,	//short int or int
		0
	);
	context1->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);	//DirectX can only draw points, lines and triangles.
	context1->PSSetShaderResources(0, 1, &resourceview);
	context1->PSSetSamplers(0, 1, &sampler);
	context1->VSSetShader(
		myvertex2,
		nullptr,
		0
	);
	context1->PSSetShader(
		mypixel2,
		nullptr,
		0
	);
	context1->DrawIndexed(
		6,	//NUMBER OF POINTS
		0,
		0
	);

}
///////////////////////////////////////
//POSITION AND COLOR INPUT PARAMETERS
///////////////////////////////////////
void Create_Texture(float x, float y, float z, float h, float w)	//draw texture inside square box
{
	DWORD NUM_TRIANGLES = 2;
	if (vertexBuffer)
		vertexBuffer->Release();
	if (indexBuffer)
		indexBuffer->Release();
	float polyVertices[30];
	polyVertices[0] = x;
	polyVertices[1] = y;
	polyVertices[2] = z;
	polyVertices[3] = 0.0; //texture coord
	polyVertices[4] = 0.0;
	polyVertices[5] = x;
	polyVertices[6] = y - h;
	polyVertices[7] = z;
	polyVertices[8] = 0.0; //texture coord
	polyVertices[9] = 1.0;
	polyVertices[10] = x + w;
	polyVertices[11] = y - h;
	polyVertices[12] = z;
	polyVertices[13] = 1.0; //texture coord
	polyVertices[14] = 1.0;
	polyVertices[15] = x;
	polyVertices[16] = y;
	polyVertices[17] = z;
	polyVertices[18] = 0.0; //texture coord
	polyVertices[19] = 0.0;
	polyVertices[20] = x + w;
	polyVertices[21] = y;
	polyVertices[22] = z;
	polyVertices[23] = 1.0; //texture coord
	polyVertices[24] = 0.0;
	polyVertices[25] = x + w;
	polyVertices[26] = y - h;
	polyVertices[27] = z;
	polyVertices[28] = 1.0f; //texture coord
	polyVertices[29] = 1.0f;


	unsigned short polyIndices[NUM_TRIANGLES * 3];
	for (int i = 0; i < NUM_TRIANGLES * 3; i++)
	{
		polyIndices[i] = i;
	}
	D3D11_BUFFER_DESC vertexBufferDesc = { 0 };
	vertexBufferDesc.ByteWidth = NUM_TRIANGLES * 60;	//(position + texture input)
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA vertexBufferData;
	vertexBufferData.pSysMem = &polyVertices[0];
	vertexBufferData.SysMemPitch = 0;
	vertexBufferData.SysMemSlicePitch = 0;
	device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &vertexBuffer);
	D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.ByteWidth = sizeof(unsigned short) * 3 * 2;
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;
	D3D11_SUBRESOURCE_DATA indexBufferData;
	indexBufferData.pSysMem = polyIndices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;
	device1->CreateBuffer(&indexBufferDesc, &indexBufferData, &indexBuffer);
}

void ReleaseDirectX()
{
	context1->ClearState();
	if (output)
		output->Release();
	if (card)
		card->Release();
	if (pBackBuffer)
		pBackBuffer->Release();
	if (renderTarget)
		renderTarget->Release();
	if (vertexBuffer)
		vertexBuffer->Release();
	if (cBuffer)
		cBuffer->Release();
	if (indexBuffer)
		indexBuffer->Release();
	if (output)
		output->Release();
	if (inputlayout)
		inputlayout->Release();
	if (inputlayout2)
		inputlayout2->Release();
	if (texture1)
		texture1->Release();
	if (context1)
		context1->Release();
	if (swapChain1)
		swapChain1->Release();
	if (resource)
		resource->Release();
	if (resourceview)
		resourceview->Release();
	if (device1)
		device1->Release();
	if (factory)
		factory->Release();
}


//window process
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	//change to fullscreen and back using enter key. Avoid alt-enter version.
	case WM_CHAR:
	{
		if (wParam == 0x0D) //enter
		{
			if (FullscreenState != 0)
				break;
			BOOL f = false;
			HRESULT hr;
			DXGI_MODE_DESC newmode;
			newmode.Width = width;
			newmode.Height = height;
			newmode.RefreshRate.Denominator = 1;
			newmode.RefreshRate.Numerator = 60;
			newmode.Scaling = SCALING_MODE;
			newmode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; // DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
			swapChain1->GetFullscreenState(&f, &output);
			if (f == false) //windowed to fullscreen
			{
				FullscreenState = 1;
				newmode.Format = DXGI_FORMAT_UNKNOWN;
				hr = swapChain1->ResizeTarget(&newmode);
				hr = swapChain1->SetFullscreenState(true, output1);
			}
			else //fullscreen to windowed
			{
				FullscreenState = 2;
				newmode.Format = FORMAT;
				hr = swapChain1->SetFullscreenState(false, output1);
				hr = swapChain1->ResizeTarget(&newmode);
			}
		}
		break;
	}

	case WM_SIZE: //clearstate waste of time
	{
		if (FullscreenState != 0)
		{
			//context1->ClearState();
			//context1->OMSetRenderTargets(0, NULL, NULL);
			//renderTarget->Release(); //release render target
			swapChain1->ResizeBuffers(2, width, height, FORMAT, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);//DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
			//recreate render target view
			//swapChain1->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
			//device1->CreateRenderTargetView(pBackBuffer, NULL, &renderTarget);
			//context1->OMSetRenderTargets(1, &renderTarget, NULL);
			//pBackBuffer->Release();
			FullscreenState = 0;
		}
		break;
	}

	case WM_DISPLAYCHANGE:
	{
		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
		break;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	default:
	{
		return(DefWindowProcA(hwnd, uMsg, wParam, lParam));
	}
	}
	return(0);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR pCmdLine, int nCmdShow)
{
	if (UpdateRegistry() == true) //automatically check and change fullscreen optimizations
	{
		MessageBoxA(0, "", "Restart", 0);
		return(0);
	}
		DEVMODEA monitorspecs;
		EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &monitorspecs);
		width = monitorspecs.dmPelsWidth;
		height = monitorspecs.dmPelsHeight;
		//window class structure
		WNDCLASSEXA windowClass;
		ZeroMemory(&windowClass, sizeof(WNDCLASSEXA));
		windowClass.cbSize = sizeof(WNDCLASSEXA);
		windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		windowClass.hInstance = hInstance;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.lpszClassName = "myWindow";
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.cbWndExtra = 0;
		windowClass.cbClsExtra = 0;
		windowClass.hCursor = 0;
		windowClass.hIcon = 0;
		windowClass.hIconSm = 0;
		windowClass.lpszMenuName = "menuname";
		RegisterClassExA(&windowClass);


		RECT rect = { 0,0,(long)width,(long)height };	//Create rectangle object
		AdjustWindowRectEx(&rect, 0, false, WS_OVERLAPPEDWINDOW);
		HWND hWnd = CreateWindowExA(0, "myWindow", "10-bit Colour Test", WS_OVERLAPPEDWINDOW, 0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, 0);
		if (!hWnd)
			return(-1);
		if (InitD3D(hWnd) == false)
		{
			MessageBoxA(0, "DirectX Error", 0, 0);
		}
		if (InitTexture() == false)
		{
			MessageBoxA(0, "Texture error.", 0, 0);
		}


		ShowWindow(hWnd, SW_MAXIMIZE);//return value irrelevant
		UpdateWindow(hWnd);
		DWORD s1 = 256;
		DWORD s2 = 1024;
		unsigned long long a = 0;
		MSG Message = { 0 };
		while (Message.message != WM_QUIT)
		{
			if (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			}
			else
			{
				if (GetTickCount64() - a >= 100)
				{
					ClearScreen(0.0f, 0.0f, 0.0f);
					Create_Squares(-1.0, 0.8, 0.1, 0.1, 2.0, s1);	//x,y,z,size,count
					SetPixelShaderInputVariables(0, 0.5f / (float)(1 << 8), 0.0, 0.0); //0.5f because 2 triangles per rectangle
					Draw_Squares(s1);
					Create_Squares(-1.0, 0.65, 0.1, 0.1, 2.0, s1);
					SetPixelShaderInputVariables(0, 0.5f / (float)(1 << 8), 0.0, 0.0, 1.0, 1.0, 0.0);
					Draw_Squares(s1);
					Create_Squares(-1.0, 0.50, 0.1, 0.1, 2.0, s1);
					SetPixelShaderInputVariables(0, 0.5f / (float)(1 << 8), 0.0, 0.0, 1.0, 0.0, 1.0);
					Draw_Squares(s1);
					Create_Squares(-1.0, 0.35, 0.1, 0.1, 2.0, s1);
					SetPixelShaderInputVariables(0, 0.5f / (float)(1 << 8), 0.0, 0.0, 0.0, 1.0, 1.0);
					Draw_Squares(s1);
					Create_Squares(-1.0, 0.0, 0.1, 0.1, 2.0, s2);
					SetPixelShaderInputVariables(2, 0.5f / (float)(1 << 10), 0.0, 0.0);	//unable to determine why SV_PrimitiveID offset by 1 square
					Draw_Squares(s2);
					Create_Squares(-1.0, -0.15, 0.1, 0.1, 2.0, s2);
					SetPixelShaderInputVariables(2, 0.5f / (float)(1 << 10), 0.0, 0.0, 1.0, 1.0, 0.0);
					Draw_Squares(s2);
					Create_Squares(-1.0, -0.3, 0.1, 0.1, 2.0, s2);
					SetPixelShaderInputVariables(2, 0.5f / (float)(1 << 10), 0.0, 0.0, 1.0, 0.0, 1.0);
					Draw_Squares(s2);
					Create_Squares(-1.0, -0.45, 0.1, 0.1, 2.0, s2);
					SetPixelShaderInputVariables(2, 0.5f / (float)(1 << 10), 0.0, 0.0, 0.0, 1.0, 1.0);
					Draw_Squares(s2);

					Create_Texture(-0.25, 1.0, 0.0, 0.12, 0.5 * ((float)height / (float)width)); //800.160
					Draw_Texture();
					PresentFrame();
					a = GetTickCount64();

				}
			}
		}
		//clean up
		ReleaseDirectX();
	return(0);
}