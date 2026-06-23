#include "Entity.h"

void Entity::Draw(ID3D12GraphicsCommandList* commandList) const
{
    if (!enabled || !mesh || !material)
        return;

    if (!material->GetPipelineState() || !material->GetRootSignature())
        return;

    commandList->SetPipelineState(material->GetPipelineState());
    commandList->SetGraphicsRootSignature(material->GetRootSignature());

    mat4 worldMatrix = transform.GetWorldMatrix();
    
    struct PerObjectData
    {
        mat4 worldMatrix;
        vec4 color;
    } data;
    
    data.worldMatrix = worldMatrix;
    data.color = material->color;

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(data) / 4, &data, 0);

    mesh->Draw(commandList);
}
