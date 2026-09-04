#include "PMDActor.h"
#include"PMDRenderer.h"
#include"Dx12Wrapper.h"
#include<d3dx12.h>

namespace
{
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

	std::wstring GetTexturePathFromModelAndTexPath(
		const std::wstring& modelPath,
		const std::wstring& texPath)
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

	std::pair<std::string, std::string> SplitFileName(
		const std::string& path, const char splitter = '*')
	{
		int idx = path.find(splitter);
		pair<std::string, std::string> ret;

		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);

		return ret;
	}

}

void* PMDActor::Transform::operator new(size_t size)
{
	//XMMATRIX 내부의 SIMD 연산을 위해 16바이트 정렬이 되어있는 상태여야 하므로
	// 메모리 정렬 관련 문제가 발생 하므로 구조체로 한 번 감싼 다음
	// _aligned_malloc을 통해 16바이트 정렬을 할 수 있게 한다.
	return _aligned_malloc(size, 16);
}

PMDActor::PMDActor(const wchar_t* filePath, PMDRenderer& renderer)
	:_renderer(renderer), _dx12(renderer._dx12), _angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(filePath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMDActor::~PMDActor()
{}

PMDActor* PMDActor::Clone()
{
	return nullptr;
}

void PMDActor::Update()
{
	_mappedMatrices[0] =
		XMMatrixScaling(_scale, _scale, _scale) *
		XMMatrixRotationY(_yaw) *
		XMMatrixTranslation(_position.x, _position.y, _position.z);
}

void PMDActor::SetTransform(const DirectX::XMFLOAT3& position, float scale, float yaw)
{
	_position = position;
	_scale = scale;
	_yaw = yaw;
}

void PMDActor::SetVisible(bool visible)
{
	_visible = visible;
}

void PMDActor::Draw()
{
	if (!_visible)
	{
		return;
	}

	ID3D12GraphicsCommandList* cmdList = _dx12.CommandList().Get();

	cmdList->IASetVertexBuffers(0, 1, &_vbView);
	cmdList->IASetIndexBuffer(&_ibView);

	_dx12.CommandList()->SetGraphicsRootConstantBufferView(1,
		_transformBuff->GetGPUVirtualAddress());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };
	cmdList->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = _dx12.Device()->
		GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;

	for (auto& m : _materials)
	{
		cmdList->SetGraphicsRootDescriptorTable(2, materialH);
		cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node,
	const DirectX::XMMATRIX& mat)
{
	_boneMatrices[node->boneIdx] *= mat;

	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, _boneMatrices[node->boneIdx]);
	}
}

HRESULT PMDActor::CreateMaterialData()
{
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * _materials.size());

	auto result = _dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	for (auto& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}

	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.NumDescriptors = 1 + _materials.size() * 5;
	matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = _dx12.Device()->CreateDescriptorHeap(&matDescHeapDesc,
		IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(0);
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	auto matDescHeapHandle = _materialHeap->GetCPUDescriptorHandleForHeapStart();

	auto incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	int materialNum = _materials.size();
	for (int i = 0; i < materialNum; ++i)
	{
		_dx12.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapHandle);

		matDescHeapHandle.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (_textureResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_textureResources[i].Get(), &srvDesc, matDescHeapHandle);
		}

		matDescHeapHandle.ptr += incSize;

		if (_sphResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._whiteTex.Get(), &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_sphResources[i].Get(), &srvDesc, matDescHeapHandle);
		}

		matDescHeapHandle.ptr += incSize;

		if (_spaResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._blackTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._blackTex.Get(), &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(
				_spaResources[i].Get(), &srvDesc, matDescHeapHandle);
		}

		matDescHeapHandle.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._gradTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_renderer._gradTex.Get(), &srvDesc, matDescHeapHandle);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapHandle);
		}

		matDescHeapHandle.ptr += incSize;
	}

	return S_OK;
}

HRESULT PMDActor::CreateTransformView()
{
	auto buffSize = sizeof(XMMATRIX)*(1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xff;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(
		D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);
	
	auto result = _dx12.Device()->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}
	_mappedMatrices[0] = _transform.world;

	std::copy(
		_boneMatrices.begin(),
		_boneMatrices.end(),
		_mappedMatrices + 1
	);

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc,
		IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	_dx12.Device()->CreateConstantBufferView(&cbvDesc,
		_transformHeap->GetCPUDescriptorHandleForHeapStart()); 

	return result;
}

HRESULT PMDActor::LoadPMDFile(const wchar_t* path)
{
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3];
	PMDHeader pmdheader = {};
	std::wstring strModelPath = path;

	FILE* fp;
	auto err = _wfopen_s(&fp, strModelPath.c_str(), L"rb");
	if (fp == nullptr)
	{
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;
	fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIdx;
		unsigned char charedgeFlg;
		unsigned int indicesNum;
		char texFilePath[20];
	};
#pragma pack()

	//struct PMDVertex
	//{
	//	XMFLOAT3 pos;
	//	XMFLOAT3 normal;
	//	XMFLOAT2 uv;
	//	unsigned short NoneNo[2];
	//	unsigned char boneWeight;
	//	unsigned char edgeFlg;
	//}; // total 38 byte

	constexpr unsigned int pmdvertex_size = 38;

	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);

	fread(vertices.data(), vertices.size(), 1, fp);

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(),
		pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

#pragma pack(1)
	struct Bone {
		char boneName[20];
		unsigned short parentNo;
		unsigned short nextNo;
		unsigned char type;
		unsigned short ikBoneNo;
		XMFLOAT3 pos;
	};
#pragma pack()
	vector<Bone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(Bone), boneNum, fp);
	fclose(fp);

	std::vector<std::string> boneNames(pmdBones.size());

	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}

	for (auto& pb : pmdBones)
	{
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	constexpr size_t MaxBoneCount = 256;

	size_t boneCount = std::max<size_t>(1, pmdBones.size());

	if (boneCount > MaxBoneCount)
	{
		assert(false);
		return E_FAIL;
	}

	_boneMatrices.assign(
		boneCount,
		XMMatrixIdentity()
	);

	D3D12_HEAP_PROPERTIES heapprop = {};

	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = vertices.size() * sizeof(vertices[0]);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = _dx12.Device()->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;

	result = _vb->Map(0, nullptr, (void**)&vertMap);
	std::copy(vertices.begin(), vertices.end(), vertMap);
	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress();
	_vbView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(vertices[0]));
	_vbView.StrideInBytes = pmdvertex_size;

	resdesc.Width = indices.size() * sizeof(indices[0]);

	result = _dx12.Device()->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf()));

	unsigned short* mappedIdx = nullptr;
	_ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	_ib->Unmap(0, nullptr);

	_ibView.BufferLocation = _ib->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
		_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		wchar_t toonFilePath[32];
		swprintf_s(toonFilePath, L"toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0) {
			_textureResources[i] = nullptr;
			continue;
		}

		std::string texFileName = pmdMaterials[i].texFilePath;
		std::string sphFileName = "";
		std::string spaFileName = "";

		if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);

			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph") {
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa") {
					spaFileName = namepair.second;
				}
				else
				{
				}
			}
		}
		else
		{
			if (GetExtension(texFileName) == "sph")
			{
				sphFileName = texFileName;
				texFileName = "";
			}
			else if (GetExtension(texFileName) == "spa")
			{
				spaFileName = texFileName;
				texFileName = "";
			}
			else
			{
			}
		}

		if (texFileName != "")
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath,
				GetWideStringFromString(texFileName));
			_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath,
				GetWideStringFromString(sphFileName));
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath,
				GetWideStringFromString(spaFileName));
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
		}
	}

	return S_OK;
}



void PMDActor::LoadVMDFile(const wchar_t* filepath, const char* name)
{
	FILE* fp;
	auto err = _wfopen_s(&fp, filepath, L"rb");
	fseek(fp, 50, SEEK_SET);
	if (fp == nullptr)
	{
		assert(0);
		return;
	}
	unsigned int motionDataNum = 0;
	fread(&motionDataNum, sizeof(motionDataNum), 1, fp);

	struct VMDMotionData
	{
		char boneName[15];
		unsigned int frameNo;
		XMFLOAT3 location;
		XMFLOAT4 quaternion;
		unsigned char bezier[64];
	};

	std::vector<VMDMotionData> vmdMotionData(motionDataNum);

	for (auto& motion : vmdMotionData)
	{
		fread(motion.boneName, sizeof(motion.boneName), 1, fp);
		fread(&motion.frameNo,
			sizeof(motion.frameNo)
			+ sizeof(motion.location)
			+ sizeof(motion.quaternion)
			+ sizeof(motion.bezier),
			1, fp);
	}

	for (auto& vmdMotion : vmdMotionData)
	{
		_motiondata[vmdMotion.boneName].emplace_back(
			KeyFrame(vmdMotion.frameNo,
				XMLoadFloat4(&vmdMotion.quaternion)));
		
	}

	for (auto& bonemotion : _motiondata)
	{
		auto node = _boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)
			* XMMatrixTranslation(pos.x, pos.y, pos.z);

		_boneMatrices[node.boneIdx] = mat;
	}

	const char str_center[9] = { -125, 90, -125, -109, -125, 94, -127, 91, 0 };

	RecursiveMatrixMultiply(&_boneNodeTable[str_center], XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}
