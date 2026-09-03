#pragma once
#include<d3d12.h>
#include<DirectXMath.h>
#include<vector>
#include<string>
#include<map>
#include<unordered_map>
#include<algorithm>
#include<wrl.h>

class Dx12Wrapper;
class PMDRenderer;

class PMDActor
{
	friend PMDRenderer;

private:
	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;

	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	struct Material
	{
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform
	{
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	std::vector <Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	std::vector<DirectX::XMMATRIX> _boneMatrices;

	struct BoneNode
	{
		int boneIdx;
		DirectX::XMFLOAT3 startPos;
		std::vector<BoneNode*> children;
	};
	std::map<std::string, BoneNode> _boneNodeTable;

	ComPtr< ID3D12DescriptorHeap> _materialHeap = nullptr;//머티리얼힙(5개분)

	struct KeyFrame
	{
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		//DirectX::XMFLOAT2 p1, p2;

		KeyFrame(
			unsigned int fno,
			const DirectX::XMVECTOR& q) :
			frameNo(fno),
			quaternion(q){}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	float _angle;

private:
	HRESULT CreateMaterialData();

	HRESULT CreateMaterialAndTextureView();

	HRESULT CreateTransformView();

	HRESULT LoadPMDFile(const wchar_t* path);
	HRESULT LoadPMDFile(const char* path);

	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);
public:
	void LoadVMDFile(const wchar_t* filepath, const char* name);
	PMDActor(const wchar_t* filePath, PMDRenderer& renderer);
	PMDActor(const char* filePath, PMDRenderer& renderer);
	~PMDActor();

	PMDActor* Clone();
	void Update();
	void Draw();
};