#pragma once

#include "d3dUtil.h"
#include "Material.h"
#include "Texture.h"
#include "MeshGeometry.h"
#include "../Model/Animation.h"

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

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderObject
{
    RenderObject() = default;
    RenderObject(const RenderObject& rhs) = delete;

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

    // Only applicable to skinned render-items.
    UINT SkinnedCBIndex = -1;

    // nullptr if this render-item is not animated by skinned mesh.
    AnimationController* animationController = nullptr;
};
