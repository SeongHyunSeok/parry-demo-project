#pragma once

#include "d3dUtil.h"

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
    DirectX::XMFLOAT3 TangentU;
};

struct SkinnedVertex : public Vertex
{
    DirectX::XMFLOAT3 BoneWeights;
    BYTE BoneIndices[4];
};
