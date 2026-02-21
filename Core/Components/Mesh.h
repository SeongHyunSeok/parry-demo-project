#pragma once

#include "../d3dUtil.h"
#include "../Material.h"
#include "../Texture.h"
#include "../MeshGeometry.h"
#include "../component.h"

using namespace DirectX;

struct DrawItem
{
    Material* Mat = nullptr;
    MeshGeometry* Geo;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

struct MeshComponent : public Component
{
    MeshComponent(Handle<GameObject> owner) : Component{ owner } {};
    MeshComponent(
        Handle<GameObject> owner,
        const UINT objContextBufferIndex,
        const FXMMATRIX& worldTransform,
        const FXMMATRIX& texTransform,
        const std::vector<MeshGeometry*>& geometries,
        const std::vector<std::string>& subMeshNames,
        const std::vector<Material*>& materials,
        const std::vector<D3D12_PRIMITIVE_TOPOLOGY>* primitiveTypes = nullptr
    ) : Component{ owner }, ObjCBIndex{ objContextBufferIndex } {
        XMStoreFloat4x4(&World, worldTransform);
        XMStoreFloat4x4(&TexTransform, texTransform);
        drawItems.resize(geometries.size());
        for (int i = 0; i < drawItems.size(); ++i) {
            drawItems[i].Mat = materials[i];
            drawItems[i].Geo = geometries[i];
            if (primitiveTypes && primitiveTypes->size() > i) {
                drawItems[i].PrimitiveType = primitiveTypes->at(i);
            }
            else {
                drawItems[i].PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

            }
            SubmeshGeometry& subMesh = geometries[i]->DrawArgs[subMeshNames[i]];
            drawItems[i].IndexCount = subMesh.IndexCount;
            drawItems[i].StartIndexLocation = subMesh.StartIndexLocation;
            drawItems[i].BaseVertexLocation = subMesh.BaseVertexLocation;
        }
    };
    MeshComponent(const MeshComponent& rhs) = delete;
    MeshComponent(MeshComponent&&) = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    std::vector<DrawItem> drawItems;
};
