#include "Plane.h"

#include "Dx12Wrapper.h"
#include "PMDRenderer.h"

#include <d3dx12.h>
#include <cassert>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

HRESULT Plane::CreateGeometry(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& halfRight, const DirectX::XMFLOAT3& halfUp)
{
	return E_NOTIMPL;
}

HRESULT Plane::CreateTransformBuffer()
{
	return E_NOTIMPL;
}

HRESULT Plane::CreateMaterial(const DirectX::XMFLOAT4& color)
{
	return E_NOTIMPL;
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
{}
