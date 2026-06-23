#include "Entity.h"

void Entity::Draw(ID3D12GraphicsCommandList* commandList) const
{
    if (!enabled || !mesh || !material)
        return;

    if (!material->GetPipelineState() || !material->GetRootSignature())
        return;

    commandList->SetPipelineState(material->GetPipelineState());
    commandList->SetGraphicsRootSignature(material->GetRootSignature());

    struct PerObjectData
    {
        vec4 positionOffset;
        vec4 color;
    } data;

    data.positionOffset = vec4(transform.position.x, transform.position.y, transform.position.z, 1.0f);
    data.color = material->color;

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(data) / 4, &data, 0);

    mesh->Draw(commandList);
}
