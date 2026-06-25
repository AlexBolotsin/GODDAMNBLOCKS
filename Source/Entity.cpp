#include "Entity.h"

void Entity::Draw(
    ID3D12GraphicsCommandList* commandList,
    const FrameCameraData& frameData,
    const mat4* worldOverride,
    const vec4* tintOverride,
    bool shadowPass) const
{
    if (!enabled || !mesh || !material)
        return;

    if (!material->GetPipelineState() || !material->GetRootSignature())
        return;

    commandList->SetPipelineState(material->GetPipelineState());
    commandList->SetGraphicsRootSignature(material->GetRootSignature());

    if (material->GetSrvHeap())
    {
        ID3D12DescriptorHeap* descriptorHeaps[] = { material->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        commandList->SetGraphicsRootDescriptorTable(2, material->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());
    }

    struct PerFrameData
    {
        mat4 viewMatrix;
        mat4 projMatrix;
        mat4 lightViewProjMatrix;
    } perFrameData;

    perFrameData.viewMatrix          = frameData.viewMatrix;
    perFrameData.projMatrix          = frameData.projMatrix;
    perFrameData.lightViewProjMatrix = frameData.lightViewProjMatrix;

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(perFrameData) / 4, &perFrameData, 0);

    struct PerObjectData
    {
        mat4 worldMatrix;
        vec4 color;
        vec4 renderParams;
        vec4 spriteUVRect;
    } perObjectData;

    perObjectData.worldMatrix = worldOverride ? *worldOverride : transform.GetWorldMatrix();
    perObjectData.color = tintOverride ? *tintOverride : tint;
    perObjectData.renderParams = shadowPass
        ? vec4(1.0f, usesSpriteTexture ? 1.0f : 0.0f, 0.0f, 0.0f)
        : vec4(0.0f, usesSpriteTexture ? 1.0f : 0.0f, 0.0f, 0.0f);
    perObjectData.spriteUVRect = spriteUVRect;

    commandList->SetGraphicsRoot32BitConstants(1, sizeof(perObjectData) / 4, &perObjectData, 0);

    mesh->Draw(commandList);
}
