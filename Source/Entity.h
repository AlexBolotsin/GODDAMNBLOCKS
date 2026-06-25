#pragma once

#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include <memory>
#include <vector>

struct FrameCameraData
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 lightViewProjMatrix;
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
    bool isBillboardActor = false;
    bool castsProjectedShadow = true;
    bool usesSpriteTexture = false;
    vec4 spriteUVRect = vec4(0.0f, 0.0f, 1.0f, 1.0f);

    std::vector<vec4> animFrames;
    float animSpeed = 8.0f;
    float animTimer = 0.0f;

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        const FrameCameraData& frameData,
        const mat4* worldOverride = nullptr,
        const vec4* tintOverride = nullptr,
        bool shadowPass = false) const;
};
