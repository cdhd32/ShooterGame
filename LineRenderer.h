#pragma once

#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

#include <vector>

class Dx12Wrapper;

class LineRenderer
{
private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    Dx12Wrapper& _dx12;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipeline;
    Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW _vertexBufferView = {};
    UINT _vertexCount = 0;

private:
    HRESULT CreateRootSignature();
    HRESULT CreateGraphicsPipeline();
    HRESULT CreateGeometry();

    static void AddLine(
        std::vector<Vertex>& vertices,
        const DirectX::XMFLOAT3& start,
        const DirectX::XMFLOAT3& end,
        const DirectX::XMFLOAT4& color);

public:
    LineRenderer(Dx12Wrapper& dx12);

    void Draw();
};