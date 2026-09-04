#pragma once
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

class Dx12Wrapper;
class PMDRenderer;

class Plane
{
private:
#pragma pack(push, 1)
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
        unsigned short boneNo[2];
        unsigned char weight;
        unsigned char edgeFlag;
    };
#pragma pack(pop)

    static_assert(sizeof(Vertex) == 38);

    struct MaterialData
    {
        DirectX::XMFLOAT4 diffuse;
        DirectX::XMFLOAT4 specular;
        DirectX::XMFLOAT3 ambient;
        float padding;
    };

    Dx12Wrapper& _dx12;
    PMDRenderer& _renderer;

    Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> _transformBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> _materialBuffer;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _materialHeap;

    D3D12_VERTEX_BUFFER_VIEW _vertexView = {};
    D3D12_INDEX_BUFFER_VIEW _indexView = {};

    HRESULT CreateGeometry(
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT3& halfRight,
        const DirectX::XMFLOAT3& halfUp);

    HRESULT CreateTransformBuffer();
    HRESULT CreateMaterial(
        const DirectX::XMFLOAT4& color);

public:
    Plane(
        Dx12Wrapper& dx12,
        PMDRenderer& renderer,
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT3& halfRight,
        const DirectX::XMFLOAT3& halfUp,
        const DirectX::XMFLOAT4& color);

    void Draw();

};

