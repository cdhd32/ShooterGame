#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class Dx12Wrapper;
class PMDActor;

class PMDRenderer
{
	friend PMDActor;
private:
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _pipeline = nullptr;//PMD용파이프라인
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr;//PMD용루트 시그니처

	//PMD용공용텍스처(흰색、검은색、그레이스케일그라데이션)
	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;
private:
	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();//흰색텍스처의생성
	ID3D12Resource* CreateBlackTexture();//검은색텍스처의생성
	ID3D12Resource* CreateGrayGradationTexture();//회색텍스처의생성

	//파이프라인초기화
	HRESULT CreateGraphicsPipelineForPMD();
	//루트 시그니처초기화
	HRESULT CreateRootSignature();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);
public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	ID3D12PipelineState* GetPipelineState();
	ID3D12RootSignature* GetRootSignature();

	ID3D12Resource* GetWhiteTexture();
	ID3D12Resource* GetBlackTexture();
	ID3D12Resource* GetGradationTexture();
};

