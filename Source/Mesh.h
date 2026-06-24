#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>
#include "EngineMath.h"

struct Vertex
{
    vec3 position;
    vec3 normal;
    vec4 color;
    vec2 uv;
};

class Mesh
{
public:
    Mesh() = default;
    ~Mesh();

    bool Init(ID3D12Device* device, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void Shutdown();
    
    void Draw(ID3D12GraphicsCommandList* commandList) const;
    
    uint32_t GetVertexCount() const { return m_vertexCount; }
    uint32_t GetIndexCount() const { return m_indexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateGPUBuffer(ID3D12Device* device, const void* data, uint32_t size);
};
