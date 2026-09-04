#include "Plane.h"

#include "Dx12Wrapper.h"
#include "PMDRenderer.h"

#include <d3dx12.h>
#include <cassert>
#include <cstring>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

HRESULT Plane::CreateGeometry(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& halfRight, const DirectX::XMFLOAT3& halfUp)
{
	const XMVECTOR c = XMLoadFloat3(&center);
	const XMVECTOR r = XMLoadFloat3(&halfRight);
	const XMVECTOR u = XMLoadFloat3(&halfUp);
	const XMVECTOR normalVector = XMVector3Normalize(XMVector3Cross(r, u));

	Vertex vertices[4] = {};
	XMStoreFloat3(&vertices[0].position, XMVectorSubtract(XMVectorSubtract(c, r), u));
	XMStoreFloat3(&vertices[1].position, XMVectorAdd(XMVectorSubtract(c, u), r));
	XMStoreFloat3(&vertices[2].position, XMVectorAdd(XMVectorAdd(c, r), u));
	XMStoreFloat3(&vertices[3].position, XMVectorAdd(XMVectorSubtract(c, r), u));

	for (auto& vertex : vertices)
	{
		XMStoreFloat3(&vertex.normal, normalVector);
		vertex.boneNo[0] = 0;
		vertex.boneNo[1] = 0;
		vertex.weight = 100;
		vertex.edgeFlag = 0;
	}
	vertices[0].uv = { 0.0f, 1.0f };
	vertices[1].uv = { 1.0f, 1.0f };
	vertices[2].uv = { 1.0f, 0.0f };
	vertices[3].uv = { 0.0f, 0.0f };

	constexpr unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };
	const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
	auto result = _dx12.Device()->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(_vertexBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) return result;

	void* mapped = nullptr;
	result = _vertexBuffer->Map(0, nullptr, &mapped);
	if (FAILED(result)) return result;
	std::memcpy(mapped, vertices, sizeof(vertices));
	_vertexBuffer->Unmap(0, nullptr);

	desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices));
	result = _dx12.Device()->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(_indexBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) return result;

	result = _indexBuffer->Map(0, nullptr, &mapped);
	if (FAILED(result)) return result;
	std::memcpy(mapped, indices, sizeof(indices));
	_indexBuffer->Unmap(0, nullptr);

	_vertexView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	_vertexView.StrideInBytes = sizeof(Vertex);
	_vertexView.SizeInBytes = sizeof(vertices);
	_indexView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
	_indexView.Format = DXGI_FORMAT_R16_UINT;
	_indexView.SizeInBytes = sizeof(indices);
	return S_OK;
}

HRESULT Plane::CreateTransformBuffer()
{
	constexpr UINT matrixCount = 257;
	constexpr UINT bufferSize = matrixCount * sizeof(XMMATRIX);
	const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
	auto result = _dx12.Device()->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(_transformBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) return result;

	XMMATRIX* matrices = nullptr;
	result = _transformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&matrices));
	if (FAILED(result)) return result;
	for (UINT i = 0; i < matrixCount; ++i)
	{
		matrices[i] = XMMatrixIdentity();
	}
	return S_OK;
}

HRESULT Plane::CreateMaterial(const DirectX::XMFLOAT4& color)
{
	constexpr UINT materialBufferSize = 256;
	const MaterialData material = {
		color,
		{ 0.0f, 0.0f, 0.0f, 0.0f },
		{ color.x * 0.35f, color.y * 0.35f, color.z * 0.35f },
		0.0f
	};

	const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize);
	auto result = _dx12.Device()->CreateCommittedResource(
		&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(_materialBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result)) return result;

	void* mapped = nullptr;
	result = _materialBuffer->Map(0, nullptr, &mapped);
	if (FAILED(result)) return result;
	std::memcpy(mapped, &material, sizeof(material));
	_materialBuffer->Unmap(0, nullptr);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 5;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	result = _dx12.Device()->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result)) return result;

	auto handle = _materialHeap->GetCPUDescriptorHandleForHeapStart();
	const UINT increment = _dx12.Device()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
	cbv.BufferLocation = _materialBuffer->GetGPUVirtualAddress();
	cbv.SizeInBytes = materialBufferSize;
	_dx12.Device()->CreateConstantBufferView(&cbv, handle);

	auto createSrv = [&](ID3D12Resource* resource)
	{
		handle.ptr += increment;
		D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Format = resource->GetDesc().Format;
		srv.Texture2D.MipLevels = 1;
		_dx12.Device()->CreateShaderResourceView(resource, &srv, handle);
	};

	createSrv(_renderer.GetWhiteTexture());
	createSrv(_renderer.GetWhiteTexture());
	createSrv(_renderer.GetBlackTexture());
	createSrv(_renderer.GetGradationTexture());
	return S_OK;
}

Plane::Plane(
	Dx12Wrapper& dx12,
	PMDRenderer& renderer,
	const DirectX::XMFLOAT3& center,
	const DirectX::XMFLOAT3& halfRight,
	const DirectX::XMFLOAT3& halfUp,
	const DirectX::XMFLOAT4& color)
	: _dx12(dx12)
	, _renderer(renderer)
{
	HRESULT hr = CreateGeometry(center, halfRight, halfUp);

	if (FAILED(hr)) { }

	hr = CreateTransformBuffer();
	if (FAILED(hr)) { }

	hr = CreateMaterial(color);
	if (FAILED(hr)) { }
}


void Plane::Draw()
{
	auto commandList = _dx12.CommandList();
	commandList->SetPipelineState(_renderer.GetPipelineState());
	commandList->SetGraphicsRootSignature(_renderer.GetRootSignature());
	_dx12.BindScene();
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &_vertexView);
	commandList->IASetIndexBuffer(&_indexView);
	commandList->SetGraphicsRootConstantBufferView(
		1, _transformBuffer->GetGPUVirtualAddress());
	ID3D12DescriptorHeap* heaps[] = { _materialHeap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	commandList->SetGraphicsRootDescriptorTable(
		2, _materialHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
