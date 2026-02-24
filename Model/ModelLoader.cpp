#include "ModelLoader.h"
#include "../Core/FileUtility.h"
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

using namespace DirectX;

bool FbxBinLoader::LoadFbxBin(const std::string& filename,
	std::vector<Vertex>& vertices,
	std::vector<UINT>& indices,
	std::vector<SubMeshInfo>& subsets) {
	FILE* file = NULL;
	::fopen_s(&file, filename.data(), "rb");
	::rewind(file);

	if (!file) return false;

	char pstrToken[1024] = { '\0' };

	std::vector<SkinnedVertex> tmpSkinnedVertices;
	std::unordered_map<std::string, FrameData> frames;
	std::vector<std::string> tmpBoneNames;
	std::vector<DirectX::XMFLOAT4X4> tmpBoneOffsets;

	if (FileUtility::ReadStringFromFile(file, pstrToken))
	{
		if (!strcmp(pstrToken, "<Hierarchy>"))
		{
			for (; ; )
			{
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<Frame>:"))
				{
					std::string frameRootName = "PARRY_DEMO_FRAME_ROOT";
					LoadFrameHierarchyFromFile(file, vertices, tmpSkinnedVertices, indices, subsets, frames, tmpBoneNames, tmpBoneOffsets, frameRootName);
				}
				else if (!strcmp(pstrToken, "</Hierarchy>"))
				{
					break;
				}
			}
		}
	}


	::fclose(file);

	return true;
}

bool FbxBinLoader::LoadFbxBin(
	const std::string& filename,
	std::vector<SkinnedVertex>& skinnedVertices,
	std::vector<UINT>& indices,
	std::vector<SubMeshInfo>& subsets,
	AnimationSets* animationsSets
) {
	FILE* file = NULL;
	::fopen_s(&file, filename.data(), "rb");
	::rewind(file);

	if (!file) return false;

	char pstrToken[1024] = { '\0' };

	std::vector<Vertex> tmpVertices;
	std::unordered_map<std::string, FrameData> frames;
	std::vector<std::string> boneNames;
	std::vector<DirectX::XMFLOAT4X4> boneOffsets;
	for (; ; )
	{
		if (FileUtility::ReadStringFromFile(file, pstrToken))
		{
			if (!strcmp(pstrToken, "<Hierarchy>"))
			{
				for (; ; )
				{
					FileUtility::ReadStringFromFile(file, pstrToken);
					if (!strcmp(pstrToken, "<Frame>:"))
					{
						std::string frameRootName = "PARRY_DEMO_FRAME_ROOT";
						LoadFrameHierarchyFromFile(file, tmpVertices, skinnedVertices, indices, subsets, frames, boneNames, boneOffsets, frameRootName);
					}
					else if (!strcmp(pstrToken, "</Hierarchy>"))
					{
						break;
					}
				}
			}
			else if (!strcmp(pstrToken, "<Animation>"))
			{
				LoadAnimationFromFile(file, frames, move(boneNames), animationsSets);;
			}
			else if (!strcmp(pstrToken, "</Animation>"))
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	animationsSets->boneOffsets = move(boneOffsets);

	::fclose(file);

	return true;
}

void FbxBinLoader::LoadFrameHierarchyFromFile(
	FILE* file,
	std::vector<Vertex>& vertices,
	std::vector<SkinnedVertex>& skinnedVertices,
	std::vector<UINT>& indices,
	std::vector<SubMeshInfo>& subsets,
	std::unordered_map<std::string, FrameData>& frames,
	std::vector<std::string>& boneNames,
	std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
	std::string& parentFrameName
) {
	char pstrToken[256] = { '\0' };
	UINT nReads = 0;

	int frameIndex = FileUtility::ReadIntegerFromFile(file);
	FileUtility::ReadStringFromFile(file, pstrToken);
	std::string newFrameName = std::string(pstrToken);
	FrameData& frameData = frames[newFrameName];
	frameData.parentFrameName = parentFrameName;

	for (; ; )
	{
		FileUtility::ReadStringFromFile(file, pstrToken);
		if (!strcmp(pstrToken, "<Transform>:"))
		{
			nReads = (UINT)::fread(&frameData.toParent, sizeof(XMFLOAT4X4), 1, file);

			nReads = (UINT)::fread(&frameData.scale, sizeof(XMFLOAT3), 1, file);
			nReads = (UINT)::fread(&frameData.rotation, sizeof(XMFLOAT3), 1, file);
			nReads = (UINT)::fread(&frameData.translation, sizeof(XMFLOAT3), 1, file);
		}
		else if (!strcmp(pstrToken, "<Mesh>:"))
		{
			LoadMeshFromFile(file, vertices, indices, subsets);
		}
		else if (!strcmp(pstrToken, "<SkinDeformations>:"))
		{
			std::vector<DirectX::XMFLOAT4> boneWeights;
			std::vector<DirectX::XMINT4> boneIndices;
			LoadSkinDeformationsFromFile(file, boneNames, boneOffsets, boneWeights, boneIndices);

			FileUtility::ReadStringFromFile(file, pstrToken);
			if (!strcmp(pstrToken, "<Mesh>:")) {
				LoadMeshFromFile(file, skinnedVertices, indices, subsets);
				int startIndex = skinnedVertices.size() - boneWeights.size();
				for (int i = 0; i < boneWeights.size(); ++i) {
					skinnedVertices[startIndex + i].BoneWeights = { boneWeights[i].x, boneWeights[i].y, boneWeights[i].z };
					skinnedVertices[startIndex + i].BoneIndices[0] = boneIndices[i].x;
					skinnedVertices[startIndex + i].BoneIndices[1] = boneIndices[i].y;
					skinnedVertices[startIndex + i].BoneIndices[2] = boneIndices[i].z;
					skinnedVertices[startIndex + i].BoneIndices[3] = boneIndices[i].w;
				}
			}
		}
		else if (!strcmp(pstrToken, "<Children>:")) {
			int childCount = FileUtility::ReadIntegerFromFile(file);
			if (childCount > 0)
			{
				for (int i = 0; i < childCount; i++)
				{
					FileUtility::ReadStringFromFile(file, pstrToken);
					if (!strcmp(pstrToken, "<Frame>:"))
					{
						LoadFrameHierarchyFromFile(file, vertices, skinnedVertices, indices, subsets, frames, boneNames, boneOffsets, newFrameName);
					}
				}
			}
		}
		else if (!strcmp(pstrToken, "</Frame>"))
		{
			break;
		}
	}
}

template<typename T> requires std::derived_from<T, Vertex>
void FbxBinLoader::LoadMeshFromFile(
	FILE* file,
	std::vector<T>& vertices,
	std::vector<UINT>& indices,
	std::vector<SubMeshInfo>& subsets
) {
	char pstrToken[64] = { '\0' };
	BYTE strLength = 0;
	UINT nReads = 0;

	FileUtility::ReadStringFromFile(file, pstrToken);
	std::string meshName(pstrToken);

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT2> uvs;
	std::vector<XMFLOAT3> normals;
	std::vector<XMFLOAT3> tangents;
	std::vector<XMFLOAT3> biTangents;

	for (; ; )
	{
		FileUtility::ReadStringFromFile(file, pstrToken);

		if (!strcmp(pstrToken, "<ControlPoints>:"))	// Vertex Position
		{
			int vertexCount = FileUtility::ReadUnsignedIntegerFromFile(file);
			if (vertexCount > 0)
			{
				positions.resize(vertexCount);
				nReads = (UINT)::fread(positions.data(), sizeof(XMFLOAT3), vertexCount, file);
			}
		}
		else if (!strcmp(pstrToken, "<UVs>:"))
		{
			int vertexCount = FileUtility::ReadUnsignedIntegerFromFile(file);
			int uvCount = FileUtility::ReadUnsignedIntegerFromFile(file);

			if (uvCount > 0) {
				uvs.resize(vertexCount);
			}
			// use only last UV
			for (int i = 0; i < uvCount; ++i) {
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<UV>:")) {
					FileUtility::ReadUnsignedIntegerFromFile(file); // UV index
					nReads = (UINT)::fread(uvs.data(), sizeof(XMFLOAT2), vertexCount, file);
				}
			}
		}
		else if (!strcmp(pstrToken, "<Normals>:"))
		{
			int vertexCount = FileUtility::ReadUnsignedIntegerFromFile(file);
			int normalCount = FileUtility::ReadUnsignedIntegerFromFile(file);

			if (normalCount > 0) {
				normals.resize(vertexCount);
			}
			// use only last Normal
			for (int i = 0; i < normalCount; ++i) {
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<Normal>:")) {
					FileUtility::ReadUnsignedIntegerFromFile(file); // Normal index
					nReads = (UINT)::fread(normals.data(), sizeof(XMFLOAT3), vertexCount, file);
				}
			}
		}
		else if (!strcmp(pstrToken, "<Tangents>:"))
		{
			int vertexCount = FileUtility::ReadUnsignedIntegerFromFile(file);
			int tangentCount = FileUtility::ReadUnsignedIntegerFromFile(file);

			if (tangentCount > 0) {
				tangents.resize(vertexCount);
			}
			// use only last Tangent
			for (int i = 0; i < tangentCount; ++i) {
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<Tangent>:")) {
					FileUtility::ReadUnsignedIntegerFromFile(file); // Tangent index
					nReads = (UINT)::fread(tangents.data(), sizeof(XMFLOAT3), vertexCount, file);
				}
			}
		}
		else if (!strcmp(pstrToken, "<BiTangents>:"))
		{
			int vertexCount = FileUtility::ReadUnsignedIntegerFromFile(file);
			int bigTangentCount = FileUtility::ReadUnsignedIntegerFromFile(file);

			if (bigTangentCount > 0) {
				biTangents.resize(vertexCount);
			}
			// use only last Normal
			for (int i = 0; i < bigTangentCount; ++i) {
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<BiTangent>:")) {
					FileUtility::ReadUnsignedIntegerFromFile(file); // BiTangent index
					nReads = (UINT)::fread(biTangents.data(), sizeof(XMFLOAT3), vertexCount, file);
				}
			}
		}
		else if (!strcmp(pstrToken, "<Polygons>:"))
		{
			int nPolygons = FileUtility::ReadIntegerFromFile(file);
			for (; ; )
			{
				FileUtility::ReadStringFromFile(file, pstrToken);
				if (!strcmp(pstrToken, "<SubIndices>:"))
				{
					int startIndexLocation = indices.size();
					const UINT baseVertexLocation = vertices.size();

					int totalIndexCount = FileUtility::ReadIntegerFromFile(file);
					indices.resize(indices.size() + totalIndexCount);

					int subMeshCount = FileUtility::ReadIntegerFromFile(file);
					for (int i = 0; i < subMeshCount; i++)
					{
						FileUtility::ReadStringFromFile(file, pstrToken);
						if (!strcmp(pstrToken, "<SubIndex>:"))
						{
							int idx = FileUtility::ReadIntegerFromFile(file); // SubIndex index
							UINT indexCount = FileUtility::ReadIntegerFromFile(file);
							if (indexCount > 0)
							{
								nReads = (UINT)::fread(&indices[startIndexLocation], sizeof(UINT), indexCount, file);

								subsets.emplace_back(meshName + std::to_string(idx), baseVertexLocation, startIndexLocation, indexCount);
								startIndexLocation += indexCount;
							}
						}
					}
				}
				else if (!strcmp(pstrToken, "</Polygons>"))
				{
					break;
				}
			}
		}
		else if (!strcmp(pstrToken, "</Mesh>"))
		{
			break;
		}
	}

	int startVertexIndex = vertices.size();
	vertices.resize(vertices.size() + positions.size());
	for (int i = 0; i < positions.size(); ++i) {
		vertices[startVertexIndex + i].Pos = positions[i];
	}
	for (int i = 0; i < uvs.size(); ++i) {
		vertices[startVertexIndex + i].TexC = uvs[i];
	}
	for (int i = 0; i < normals.size(); ++i) {
		vertices[startVertexIndex + i].Normal = normals[i];
	}
	for (int i = 0; i < tangents.size(); ++i) {
		vertices[startVertexIndex + i].TangentU = tangents[i];
	}
};

void FbxBinLoader::LoadSkinDeformationsFromFile(
	FILE* file,
	std::vector<std::string>& boneNames,
	std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
	std::vector<DirectX::XMFLOAT4>& boneWeights,
	std::vector<DirectX::XMINT4>& boneIndices
) {
	char pstrToken[64] = { '\0' };
	BYTE nStrLength = 0;
	UINT nReads = 0;

	for (; ; )
	{
		FileUtility::ReadStringFromFile(file, pstrToken);
		if (!strcmp(pstrToken, "<BoneNames>:"))
		{
			int boneCount = FileUtility::ReadIntegerFromFile(file);
			if (boneCount > 0)
			{
				boneNames.reserve(boneCount);
				for (int i = 0; i < boneCount; i++)
				{
					FileUtility::ReadStringFromFile(file, pstrToken);
					boneNames.emplace_back(pstrToken);
				}
			}
		}
		else if (!strcmp(pstrToken, "<BoneOffsets>:"))
		{
			int boneCount = FileUtility::ReadIntegerFromFile(file);
			if (boneCount > 0)
			{
				boneOffsets.resize(boneCount);
				nReads = (UINT)::fread(boneOffsets.data(), sizeof(XMFLOAT4X4), boneCount, file);
			}
		}
		else if (!strcmp(pstrToken, "<BoneIndices>:"))
		{
			int vertexCount = FileUtility::ReadIntegerFromFile(file);
			if (vertexCount > 0)
			{
				boneIndices.resize(vertexCount);

				nReads = (UINT)::fread(boneIndices.data(), sizeof(XMINT4), vertexCount, file);
			}
		}
		else if (!strcmp(pstrToken, "<BoneWeights>:"))
		{
			int vertexCount = FileUtility::ReadIntegerFromFile(file);
			if (vertexCount > 0)
			{
				boneWeights.resize(vertexCount);

				nReads = (UINT)::fread(boneWeights.data(), sizeof(XMFLOAT4), vertexCount, file);
			}
		}
		else if (!strcmp(pstrToken, "</SkinDeformations>"))
		{
			break;
		}
	}
}

void FbxBinLoader::LoadAnimationFromFile(
	FILE* file,
	std::unordered_map<std::string, FrameData>& frames,
	std::vector<std::string>&& boneNames,
	AnimationSets* animationsSets
) {
	char pstrToken[64] = { '\0' };
	UINT nReads = 0;

	int animationSetCount = 0;

	// create bone hierarchy
	animationsSets->boneHierarchy.clear();
	animationsSets->boneHierarchy.resize(boneNames.size());

	std::unordered_map<std::string, int> boneNameToIdx;
	boneNameToIdx.emplace(move(boneNames[0]), 0);
	animationsSets->boneHierarchy[0] = 0;
	for (int i = 1; i < boneNames.size(); ++i) {
		const FrameData& frame = frames[boneNames[i]];
		boneNameToIdx.emplace(move(boneNames[i]), i);
		animationsSets->boneHierarchy[i] = boneNameToIdx[frame.parentFrameName];
	}
	boneNames.clear();

	for (; ; )
	{
		FileUtility::ReadStringFromFile(file, pstrToken);
		if (!strcmp(pstrToken, "<AnimationSets>:"))
		{
			animationSetCount = FileUtility::ReadIntegerFromFile(file);
		}
		else if (!strcmp(pstrToken, "<AnimationSet>:"))
		{
			int animationSetIndex = FileUtility::ReadIntegerFromFile(file);

			FileUtility::ReadStringFromFile(file, pstrToken); //Animation Set Name

			float startTime = FileUtility::ReadFloatFromFile(file);
			float endTime = FileUtility::ReadFloatFromFile(file);
			std::unordered_map<int, std::string> m;

			m.emplace(std::piecewise_construct,
				std::forward_as_tuple(6),
				std::forward_as_tuple(9, 'g'));
			auto token = std::string(pstrToken);
			animationsSets->animationSets.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(pstrToken),
				std::forward_as_tuple(startTime, endTime, token)
			);

			AnimationSet& newAnimationSet = animationsSets->animationSets[std::string(pstrToken)];

			FileUtility::ReadStringFromFile(file, pstrToken);
			if (!strcmp(pstrToken, "<AnimationLayers>:"))
			{
				int animationLayerCount = FileUtility::ReadIntegerFromFile(file);
				newAnimationSet.animationLayers.resize(animationLayerCount);

				for (int i = 0; i < animationLayerCount; i++)
				{
					FileUtility::ReadStringFromFile(file, pstrToken);
					if (!strcmp(pstrToken, "<AnimationLayer>:"))
					{
						int animationLayerIndex = FileUtility::ReadIntegerFromFile(file);
						AnimationLayer& newAnimationLayer = newAnimationSet.animationLayers[animationLayerIndex];

						int animationCurveCount = FileUtility::ReadIntegerFromFile(file);

						newAnimationLayer.weight = FileUtility::ReadFloatFromFile(file);

						newAnimationLayer.animationCurves.clear();
						for (int j = 0; j < animationCurveCount; j++)
						{
							FileUtility::ReadStringFromFile(file, pstrToken);
							if (!strcmp(pstrToken, "<AnimationCurve>:"))
							{
								int curveNodeIndex = FileUtility::ReadIntegerFromFile(file); //j
								FileUtility::ReadStringFromFile(file, pstrToken); //Frame Name
								std::string frameName = std::string(pstrToken);

								AnimationCurvesPerCurveType newAnimationCurve;
								for (; ; )
								{
									FileUtility::ReadStringFromFile(file, pstrToken);
									if (!strcmp(pstrToken, "<TX>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::TX);
									else if (!strcmp(pstrToken, "<TY>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::TY);
									else if (!strcmp(pstrToken, "<TZ>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::TZ);
									else if (!strcmp(pstrToken, "<RX>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::RX);
									else if (!strcmp(pstrToken, "<RY>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::RY);
									else if (!strcmp(pstrToken, "<RZ>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::RZ);
									else if (!strcmp(pstrToken, "<SX>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::SX);
									else if (!strcmp(pstrToken, "<SY>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::SY);
									else if (!strcmp(pstrToken, "<SZ>:")) LoadAnimationKeyValues(file, newAnimationCurve, AnimationCurveType::SZ);
									else if (!strcmp(pstrToken, "</AnimationCurve>"))
									{
										break;
									}
								}
								if (boneNameToIdx.find(frameName) != boneNameToIdx.end()) {
									newAnimationLayer.animationCurves.emplace_back(move(newAnimationCurve));
								}
							}
						}
						FileUtility::ReadStringFromFile(file, pstrToken); //</AnimationLayer>
					}
				}
				FileUtility::ReadStringFromFile(file, pstrToken); //</AnimationLayers>
			}
			FileUtility::ReadStringFromFile(file, pstrToken); //</AnimationSet>
		}
		else if (!strcmp(pstrToken, "</AnimationSets>"))
		{

			break;
		}
	}

#if defined(DEBUG) || defined(_DEBUG) 
	TCHAR pstrDebug[256] = { 0 };
	_stprintf_s(pstrDebug, 256, _T("Frame Hierarchy\n"));
	OutputDebugString(pstrDebug);

	PrintBoneHierarcyInfo(animationsSets);
#endif
}

void FbxBinLoader::LoadAnimationKeyValues(
	FILE* file,
	AnimationCurvesPerCurveType& animationCurve,
	AnimationCurveType curveType
) {
	int nAnimationKeys = FileUtility::ReadIntegerFromFile(file);

	int curveTypeIndex = (int)curveType;
	animationCurve[curveTypeIndex].SetKeyCount(nAnimationKeys);

	::fread(animationCurve[curveTypeIndex].keyTimes.data(), sizeof(float), nAnimationKeys, file);
	::fread(animationCurve[curveTypeIndex].keyValues.data(), sizeof(float), nAnimationKeys, file);
}

void FbxBinLoader::PrintBoneHierarcyInfo(const AnimationSets const* animationsSets)
{
	TCHAR pstrDebug[256] = { 0 };

	int i = 0;
	for (const int it : animationsSets->boneHierarchy) {
		_stprintf_s(pstrDebug, 256, _T("(Frame: %d) (Parent: %d)\n"), i++, it);
		OutputDebugString(pstrDebug);
	}
}
