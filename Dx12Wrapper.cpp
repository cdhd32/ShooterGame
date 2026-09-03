#include "Dx12Wrapper.h"
#include <d3dx12.h>
#include "Application.h"

#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace
{
	std::wstring GetTexturePathFromModelAndTexPath(
		const std::wstring& modelPath,
		const wchar_t* texPath)
	{
		int pathIndex1 = modelPath.rfind(L'/');
		int pathIndex2 = modelPath.rfind(L'\\');

		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);

		return folderPath + texPath;
	}

	std::wstring GetWideStringFromString(const std::string& str)
	{
		auto num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			nullptr,
			0);

		std::wstring wstr;
		wstr.resize(num1);

		auto num2 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			&wstr[0],
			num1);

		assert(num1 == num2);
		return wstr;
	}

	std::wstring GetExtensionW(const std::wstring& path)
	{
		auto idx = path.rfind(L'.');
		if (idx == std::wstring::npos)
		{
			return L"";
		}
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	std::string GetExtension(const std::string& path)
	{
		auto idx = path.rfind('.');
		if (idx == std::string::npos)
		{
			return "";
		}
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	void EnableDebugLayer()
	{
		ID3D12Debug* debugLayer = nullptr;
		auto result = D3D12GetDebugInterface(
			IID_PPV_ARGS(&debugLayer));

		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

HRESULT Dx12Wrapper::InitializeDXGIDevice()
{
	UINT flagsDXGI = 0;
	auto result = S_OK;

#ifdef _DEBUG
	flagsDXGI = DXGI_CREATE_FACTORY_DEBUG;
	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG,
		IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
#else
	result = CreateDXGIFactory1(
		IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));
#endif

	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	if (FAILED(result))
	{
		return result;
	}

	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if ((strDesc.find(L"NVIDIA") != std::string::npos) ||
			(strDesc.find(L"AMD") != std::string::npos) ||
			(strDesc.find(L"Intel") != std::string::npos))
		{
			tmpAdapter = adpt;
			break;
		}
	}

	result = S_FALSE;
	D3D_FEATURE_LEVEL featureLevel;
	for (auto lv : levels)
	{
		if (SUCCEEDED(D3D12CreateDevice(nullptr, lv,
			IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf()))))
		{
			featureLevel = lv;
			result = S_OK;
			break;
		}
	}

	return result;
}

HRESULT Dx12Wrapper::InitializeCommand()
{
	auto result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAllocator.Get(), nullptr,
		IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _dev->CreateCommandQueue(&cmdQueueDesc,
		IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	return S_OK;
}

HRESULT Dx12Wrapper::CreateRenderTarget()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapchain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //렌더 타겟 뷰
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; //앞, 뒤 버퍼 각각 하나씩
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //따로 지정하지 않음

	result = _dev->CreateDescriptorHeap(&heapDesc,
		IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		_rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	_backBuffers.resize(swcDesc.BufferCount);
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = _swapchain->GetBuffer(static_cast<UINT>(idx),
			IID_PPV_ARGS(&_backBuffers[idx]));
		assert(SUCCEEDED(result));
		_dev->CreateRenderTargetView(_backBuffers[idx], &rtvDesc, handle);
		handle.ptr +=
			_dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	_viewport.reset(new CD3DX12_VIEWPORT(_backBuffers[0]));
	_scissorrect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));

	return result;
}

HRESULT Dx12Wrapper::CreateSwapChain(const HWND& hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = _winSize.cx;
	swapchainDesc.Height = _winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;

	//백버퍼는 확대/축소 가능
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	//플립 후 바로 파기
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//알파 모드 지정하지 않음
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	//윈도우 모드와 전체 화면 모드 간 전환 허용
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto result = _dxgiFactory->CreateSwapChainForHwnd(_cmdQueue.Get(),
		hwnd, &swapchainDesc, nullptr, nullptr,
		(IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf());

	if (FAILED(result)) {
		assert(0);
		return result;
	}

	return result;
}

HRESULT Dx12Wrapper::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = _winSize.cx;
	depthResDesc.Height = _winSize.cy;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	auto result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		//오류 처리
		return result;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.ReleaseAndGetAddressOf()));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	_dev->CreateDepthStencilView(
		_depthBuffer.Get(),
		&dsvDesc,
		_dsvHeap->GetCPUDescriptorHandleForHeapStart());

	return result;
}

void Dx12Wrapper::CreateTextureLoaderTable()
{
	loadLambdaTable[L"sph"]
		= loadLambdaTable[L"spa"]
		= loadLambdaTable[L"bmp"]
		= loadLambdaTable[L"png"]
		= loadLambdaTable[L"jpg"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
		{
			return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
		};

	loadLambdaTable[L"tga"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
		{
			return LoadFromTGAFile(path.c_str(), meta, img);
		};

	loadLambdaTable[L"dds"]
		= [](const std::wstring& path, TexMetadata* meta, ScratchImage& img)
		-> HRESULT
		{
			return LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, meta, img);
		};
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const wchar_t* texPath)
{
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto it = _textureTable.find(texPath);

	if (it != _textureTable.end())
	{
		return _textureTable[texPath].Get();
	}

	auto ext = GetExtensionW(texPath);//둮뮗럔귩롦벦
	auto result = loadLambdaTable[ext](texPath,
		&metadata,
		scratchImg);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty =
		D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff));

	if (FAILED(result))
	{
		return nullptr;
	}

	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch);

	if (FAILED(result))
	{
		return nullptr;
	}

	_textureTable[texPath] = texbuff;
	return texbuff;
}

HRESULT Dx12Wrapper::CreateSceneView()
{
	auto heapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff);
	auto result = _dev->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(_sceneConstBuff.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(0);
		return result;
	}

	_mapSceneData = nullptr;
	result = _sceneConstBuff->Map(0, nullptr, (void**)&_mapSceneData);

	XMFLOAT3 eye(0, 10, -10);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);

	_mapSceneData->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	_mapSceneData->eye = eye;
	_mapSceneData->proj = XMMatrixPerspectiveFovLH(XM_PIDIV4,
		static_cast<float>(_winSize.cx) / static_cast<float>(_winSize.cy),
		1.0f,
		100.0f
	);

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(&descHeapDesc,
		IID_PPV_ARGS(_SceneDescHeap.ReleaseAndGetAddressOf()));

	auto heapHandle = _SceneDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = _sceneConstBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(&cbvDesc, heapHandle);

	return result;
}

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif

	auto& app = Application::Instance();
	_winSize = app.GetWindowSize();

	//DirectX12관련초기화
	if (FAILED(InitializeDXGIDevice())) 
	{
		assert(0);
		return;
	}
	if (FAILED(InitializeCommand()))
	{
		assert(0);
		return;
	}
	if (FAILED(CreateSwapChain(hwnd)))
	{
		assert(0);
		return;
	}
	if (FAILED(CreateRenderTarget()))
	{
		assert(0);
		return;
	}

	if (FAILED(CreateSceneView()))
	{
		assert(0);
		return;
	}

	CreateTextureLoaderTable();

	if (FAILED(CreateDepthStencilView()))
	{
		assert(0);
		return;
	}

	if (FAILED(_dev->CreateFence(_fenceVal, 
		D3D12_FENCE_FLAG_NONE, 
		IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf())))) 
	{
		assert(0);
		return;
	}

}

Dx12Wrapper::~Dx12Wrapper()
{
}

void Dx12Wrapper::Update()
{
}

void Dx12Wrapper::BeginDraw()
{
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	
	_cmdList->RSSetViewports(1, _viewport.get());
	_cmdList->RSSetScissorRects(1, _scissorrect.get());
}

void Dx12Wrapper::EndDraw()
{
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &BarrierDesc);
	
	_cmdList->Close();

	ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(1, cmdLists);

	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	if ((_fence->GetCompletedValue() != _fenceVal))
	{
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator.Get(), nullptr);
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const wchar_t* texPath)
{
	auto it = _textureTable.find(texPath);
	if (it != _textureTable.end())
	{
		return _textureTable[texPath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texPath));
	}
}

void Dx12Wrapper::SetScene(const XMMATRIX& view, const XMFLOAT3& eye)
{
	_mapSceneData->view = view;
	_mapSceneData->eye = eye;
}

void Dx12Wrapper::BindScene()
{
	_cmdList->SetGraphicsRootConstantBufferView(0, _sceneConstBuff->GetGPUVirtualAddress());
}

ComPtr<ID3D12Device> Dx12Wrapper::Device()
{
	return _dev;
}

ComPtr<ID3D12GraphicsCommandList> Dx12Wrapper::CommandList()
{
	return _cmdList;
}

ComPtr<IDXGISwapChain4> Dx12Wrapper::Swapchain()
{
	return _swapchain;
}
