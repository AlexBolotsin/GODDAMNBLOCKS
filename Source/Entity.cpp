#include "Entity.h"

void Entity::Draw(ID3D12GraphicsCommandList* commandList, const FrameCameraData& frameData) const
{
    if (!enabled || !mesh || !material)
        return;

    if (!material->GetPipelineState() || !material->GetRootSignature())
        return;

    commandList->SetPipelineState(material->GetPipelineState());
    commandList->SetGraphicsRootSignature(material->GetRootSignature());

    struct PerFrameData
    {
        mat4 viewMatrix;
        mat4 projMatrix;
        vec4 cameraPosition;
    } perFrameData;

    perFrameData.viewMatrix = frameData.viewMatrix;
    perFrameData.projMatrix = frameData.projMatrix;
    perFrameData.cameraPosition = frameData.cameraPosition;

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(perFrameData) / 4, &perFrameData, 0);

    struct PerObjectData
    {
        mat4 worldMatrix;
        vec4 color;
    } perObjectData;

    perObjectData.worldMatrix = transform.GetWorldMatrix();
        perObjectData.color = tint;

    commandList->SetGraphicsRoot32BitConstants(1, sizeof(perObjectData) / 4, &perObjectData, 0);

    mesh->Draw(commandList);
}
