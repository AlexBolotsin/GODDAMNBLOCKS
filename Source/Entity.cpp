#include "Entity.h"

namespace
{
    mat4 BuildPerspectiveRH(float fovYRadians, float aspect, float nearZ, float farZ)
    {
        mat4 result;

        const float f = 1.0f / tanf(fovYRadians * 0.5f);
        result.m[0] = f / aspect;
        result.m[5] = f;
        result.m[10] = farZ / (nearZ - farZ);
        result.m[11] = -1.0f;
        result.m[14] = nearZ * farZ / (nearZ - farZ);
        result.m[15] = 0.0f;

        return result;
    }
}

void Entity::Draw(ID3D12GraphicsCommandList* commandList, float aspectRatio) const
{
    if (!enabled || !mesh || !material)
        return;

    if (!material->GetPipelineState() || !material->GetRootSignature())
        return;

    commandList->SetPipelineState(material->GetPipelineState());
    commandList->SetGraphicsRootSignature(material->GetRootSignature());

    const float safeAspect = (aspectRatio > 0.0f) ? aspectRatio : (16.0f / 9.0f);

    struct PerObjectData
    {
        mat4 viewProj;
        vec4 positionOffset;
        vec4 color;
    } data;

    data.viewProj = BuildPerspectiveRH(1.0471976f, safeAspect, 0.1f, 100.0f);
    data.positionOffset = vec4(transform.position.x, transform.position.y, transform.position.z, 1.0f);
    data.color = material->color;

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(data) / 4, &data, 0);

    mesh->Draw(commandList);
}
