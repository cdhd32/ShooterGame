#include "LineRenderer.h"
#include <d3dx12.h>
#include "Dx12Wrapper.h"

#include "Utils.h"

HRESULT LineRenderer::CreateRootSignature()
{
    D3D12_ROOT_PARAMETER rootParameter = {};

    rootParameter.ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.ShaderVisibility =
        D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT result = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.ReleaseAndGetAddressOf(),
        errorBlob.ReleaseAndGetAddressOf());

    if (FAILED(result))
    {
        if (errorBlob)
        {
            OutputDebugStringA(
                static_cast<const char*>(
                    errorBlob->GetBufferPointer()));
        }

        return result;
    }

    return _dx12.Device()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(
            _rootSignature.ReleaseAndGetAddressOf()));
}

HRESULT LineRenderer::CreateGraphicsPipeline()
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    auto result = D3DReadFileToBlob(L"LineVertexShader.cso",
        vsBlob.ReleaseAndGetAddressOf());

    if (!Utils::CheckShaderCompileResult(result, errorBlob.Get()))
    {
        result = D3DCompileFromFile(L"LineVertexShader.hlsl",
            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "LineVS", "vs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
            vsBlob.ReleaseAndGetAddressOf(),
            errorBlob.ReleaseAndGetAddressOf());

        if (!Utils::CheckShaderCompileResult(result, errorBlob.Get())) {
            assert(0);
            return result;
        }
    }

    result = D3DReadFileToBlob(L"LinePixelShader.cso",
        psBlob.ReleaseAndGetAddressOf());

    if (!Utils::CheckShaderCompileResult(result, errorBlob.Get()))
    {
        result = D3DCompileFromFile(L"LinePixelShader.hlsl", nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "LinePS", "ps_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
            psBlob.ReleaseAndGetAddressOf(),
            errorBlob.ReleaseAndGetAddressOf());

        if (!Utils::CheckShaderCompileResult(result, errorBlob.Get()))
        {
            assert(0);
            return result;
        }
    }

    if (FAILED(result))
    {
        return result;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};

    pipelineDesc.pRootSignature = _rootSignature.Get();

    pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize();

    pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize();

    pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // 선이 모델의 깊이 버퍼를 덮어쓰지 않도록 한다.
    pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
    pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    pipelineDesc.SampleDesc.Count = 1;

    return _dx12.Device()->CreateGraphicsPipelineState(&pipelineDesc,
        IID_PPV_ARGS(_pipeline.ReleaseAndGetAddressOf()));
}

HRESULT LineRenderer::CreateGeometry()
{
	std::vector<Vertex> vertices;

    constexpr int halfGridCount = 20;
    constexpr float spacing = 1.0f;

    const float extent = halfGridCount * spacing;

    const DirectX::XMFLOAT4 gridColor =
    {
        0.62f, 0.62f, 0.62f, 1.0f
    };

    for (int i = -halfGridCount; i <= halfGridCount; i++)
    {
        const float offset = i * spacing;

        AddLine(vertices, {offset, 0.0f, -extent}, {offset, 0.0f, extent}, gridColor);
        AddLine(vertices, {-extent, 0.0f, offset}, {extent, 0.0f, offset}, gridColor);

    }

    constexpr float axisLength = 4.0f;

    // +X: 빨강
    AddLine(
        vertices,
        { 0.0f, 0.0f, 0.0f },
        { axisLength, 0.0f, 0.0f },
        { 1.0f, 0.1f, 0.1f, 1.0f });

    // +Y: 초록
    AddLine(
        vertices,
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, axisLength, 0.0f },
        { 0.1f, 0.8f, 0.1f, 1.0f });

    // +Z: 파랑
    AddLine(
        vertices,
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, axisLength },
        { 0.1f, 0.35f, 1.0f, 1.0f });

    _vertexCount = static_cast<UINT>(vertices.size());

    const UINT bufferSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));

    const auto heapProperties = CD3DX12_HEAP_PROPERTIES(
                                    D3D12_HEAP_TYPE_UPLOAD);

    const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    HRESULT result = _dx12.Device()->CreateCommittedResource(&heapProperties,
        D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(_vertexBuffer.ReleaseAndGetAddressOf()));

    if (FAILED(result))
    {
        return result;
    }

    void* mappedData = nullptr;

    result = _vertexBuffer->Map(0, nullptr, &mappedData);

	if (FAILED(result))
	{
		return result;
	}

	memcpy(mappedData, vertices.data(), bufferSize);
	_vertexBuffer->Unmap(0, nullptr);

	_vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
	_vertexBufferView.StrideInBytes = sizeof(Vertex);
	_vertexBufferView.SizeInBytes = bufferSize;
	
    return S_OK;
        
}

void LineRenderer::AddLine(std::vector<Vertex>& vertices,
    const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
    const DirectX::XMFLOAT4& color)
{
    vertices.push_back({ start, color });
    vertices.push_back({ end, color });
}

LineRenderer::LineRenderer(Dx12Wrapper & dx12)
	: _dx12(dx12)
{
	auto result = CreateRootSignature();
	assert(SUCCEEDED(result));
	result = CreateGraphicsPipeline();
	assert(SUCCEEDED(result));
    result = CreateGeometry();
	assert(SUCCEEDED(result));
}

void LineRenderer::Draw()
{
    auto commandList = _dx12.CommandList();

    commandList->SetPipelineState(
        _pipeline.Get());

    commandList->SetGraphicsRootSignature(
        _rootSignature.Get());

    _dx12.BindScene();

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    commandList->IASetVertexBuffers(
        0,
        1,
        &_vertexBufferView);

    commandList->DrawInstanced(
        _vertexCount,
        1,
        0,
        0);
}
