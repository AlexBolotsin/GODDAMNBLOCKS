#include "Mesh.h"

Mesh::~Mesh()
{
    Shutdown();
}

bool Mesh::Init(ID3D12Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
        return false;

    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    m_vertexBuffer = CreateGPUBuffer(device, vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(Vertex)));
    if (!m_vertexBuffer)
        return false;

    m_indexBuffer = CreateGPUBuffer(device, indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
    if (!m_indexBuffer)
        return false;

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

    return true;
}

void Mesh::Shutdown()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexCount = 0;
    m_indexCount = 0;
}

void Mesh::Draw(ID3D12GraphicsCommandList* commandList) const
{
    if (!commandList || m_indexCount == 0)
        return;

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Mesh::CreateGPUBuffer(ID3D12Device* device, const void* data, uint32_t size)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer))))
        return nullptr;

    void* mappedPtr = nullptr;
    if (FAILED(buffer->Map(0, nullptr, &mappedPtr)))
        return nullptr;

    memcpy(mappedPtr, data, size);
    buffer->Unmap(0, nullptr);

    return buffer;
}
