#pragma once

#include<Windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include<vector>
#include<map>
#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>
#include<wrl.h>

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

//struct MaterialForHlsl
//{
//	XMFLOAT3 diffuse;
//	float alpha;
//	XMFLOAT3 specular;
//	float specularity;
//	XMFLOAT3 ambient;
//};
//
//struct AdditionalMaterial
//{
//	std::string texPath;
//	int toonIdx;
//	bool edgeFlg;
//};
//
//struct Material
//{
//	unsigned int indicesNum;
//	MaterialForHlsl material;
//	AdditionalMaterial additional;
//};

class Dx12Wrapper
{
private:
	SIZE _winSize;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<IDXGIFactory6> _dxgiFactory = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;

	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	ComPtr <ID3D12Resource> _depthBuffer = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;

	std::unique_ptr<D3D12_VIEWPORT> _viewport = {};
	std::unique_ptr<D3D12_RECT> _scissorrect = {};

	ComPtr <ID3D12Resource> _sceneConstBuff = nullptr;

	struct SceneData
	{
		XMMATRIX view;
		XMMATRIX proj;
		XMFLOAT3 eye;
	};

	SceneData* _mapSceneData;
	ComPtr<ID3D12DescriptorHeap> _SceneDescHeap = nullptr;

	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	using LoadLambda_t = function<HRESULT(const std::wstring& path, TexMetadata*, ScratchImage&)>;
	std::map <std::wstring, LoadLambda_t> loadLambdaTable;

	std::unordered_map <std::wstring, ComPtr<ID3D12Resource>> _textureTable;

private:
	HRESULT InitializeDXGIDevice();

	HRESULT InitializeCommand();

	HRESULT CreateRenderTarget();

	HRESULT CreateSwapChain(const HWND& hwnd);

	HRESULT CreateDepthStencilView();

	HRESULT CreateSceneView();

	void CreateTextureLoaderTable();

	ID3D12Resource* CreateTextureFromFile(const wchar_t* texpath);


public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();

	void BeginDraw();
	void EndDraw();

	ComPtr<ID3D12Resource> GetTextureByPath(const wchar_t* texpath);

	ComPtr<ID3D12Device> Device();
	ComPtr<ID3D12GraphicsCommandList> CommandList();
	ComPtr<IDXGISwapChain4> Swapchain();

	void SetScene();
};

