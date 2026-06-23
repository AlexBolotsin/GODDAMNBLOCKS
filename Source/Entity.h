#pragma once

#include "Transform.h"
#include "Mesh.h"
#include "Material.h"
#include <memory>

class Entity
{
public:
    Entity() = default;
    ~Entity() = default;

    Transform transform;
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool enabled = true;

    void Draw(ID3D12GraphicsCommandList* commandList) const;
};
