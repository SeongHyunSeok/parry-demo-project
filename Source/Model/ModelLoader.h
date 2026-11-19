#ifndef LOADMODEL_H
#define LOADMODEL_H

#include "Animation.h"
#include "../Core/Vertex.h"

class FbxBinLoader
{
public:
    struct SubMeshInfo
    {
        std::string name;
        UINT baseVertexLocation = 0;
        UINT startIndexLocation = 0;
        UINT indexCount = 0;
    };

    bool LoadFbxBin(const std::string& filename,
        std::vector<Vertex>& vertices,
        std::vector<UINT>& indices,
        std::vector<SubMeshInfo>& subsets);
    bool LoadFbxBin(const std::string& filename,
        std::vector<SkinnedVertex>& skinnedVertices,
        std::vector<UINT>& indices,
        std::vector<SubMeshInfo>& subsets,
        AnimationSets* animationsSets);
private:
    struct FrameData
    {
        std::string parentFrameName;

        XMFLOAT4X4 toParent = MathHelper::Identity4x4();
        XMFLOAT3 scale = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
        XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
    };


    void LoadFrameHierarchyFromFile(
        FILE* file,
        std::vector<Vertex>& vertices,
        std::vector<SkinnedVertex>& skinnedVertices,
        std::vector<UINT>& indices,
        std::vector<SubMeshInfo>& subsets,
        std::unordered_map<std::string, FrameData>& frames,
        std::vector<std::string>& boneNames,
        std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
        std::string& parentFrameName
    );
    template<typename T> requires std::derived_from<T, Vertex>
    void LoadMeshFromFile(
        FILE* file,
        std::vector<T>& vertices,
        std::vector<UINT>& indices,
        std::vector<SubMeshInfo>& subsets
    );
    void LoadSkinDeformationsFromFile(
        FILE* file,
        std::vector<std::string>& boneNames,
        std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
        std::vector<DirectX::XMFLOAT4>& boneWeights,
        std::vector<DirectX::XMINT4>& boneIndices
    );
    void LoadAnimationFromFile(
        FILE* file,
        std::unordered_map<std::string, FrameData>& frames,
        std::vector<std::string>&& boneNames,
        AnimationSets* animationsSets
    );
    void LoadAnimationKeyValues(
        FILE* file,
        AnimationCurvesPerCurveType& newAnimationCurve,
        AnimationCurveType curveType
    );
    void PrintBoneHierarcyInfo(const AnimationSets const* animationsSets);
};

struct MaterialInfo
{
    std::string Name;

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.8f;
    bool AlphaClip = false;

    std::string MaterialTypeName;
    std::string DiffuseMapName;
    std::string NormalMapName;
};

#endif // LOADMODEL_H
