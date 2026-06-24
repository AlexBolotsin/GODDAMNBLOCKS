#pragma once

#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include <memory>

struct FrameCameraData
{
    mat4 viewMatrix;
    mat4 projMatrix;
    vec4 cameraPosition;
};

class Entity
{
public:
    Entity() = default;
    ~Entity() = default;

    Transform transform;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    vec4 tint = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool enabled = true;

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const FrameCameraData& frameData,
        const mat4* worldOverride = nullptr,
        const vec4* tintOverride = nullptr,
        bool shadowPass = false) const;
};
