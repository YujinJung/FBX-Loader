#pragma once

#include <fbxsdk.h>
#include "SkinnedData.h"

struct BoneIndexAndWeight
{
	BYTE vBoneIndices;
	float vBoneWeight;

	bool operator < (const BoneIndexAndWeight& rhs)
	{
		return (vBoneWeight > rhs.vBoneWeight);
	}
};

struct CtrlPoint
{
	DirectX::XMFLOAT3 mPosition;
	std::vector<BoneIndexAndWeight> mBoneInfo;
	std::string mBoneName;

	CtrlPoint()
	{
		mBoneInfo.reserve(4);
	}

	void SortBlendingInfoByWeight()
	{
		std::sort(mBoneInfo.begin(), mBoneInfo.end());
	}
};

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector, SkinnedData & outSkinnedData, std::vector<Material>& outMaterial, std::string fileName);

	void GetSkeletonHierarchy(FbxNode * pNode, int curIndex, int parentIndex);

	void GetControlPoints(fbxsdk::FbxNode * pFbxRootNode);

	void GetAnimation(FbxScene * pFbxScene, FbxNode * pFbxChildNode, std::string & outAnimationName);

	void GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector, SkinnedData & outSkinnedData);

	FbxAMatrix GetGeometryTransformation(FbxNode * pNode);

	void GetMaterials(FbxNode * pNode, std::vector<Material>& outMaterial);

	void GetMaterialAttribute(FbxSurfaceMaterial * pMaterial, std::vector<Material>& outMaterial);

	void GetMaterialTexture(FbxSurfaceMaterial * pMaterial, Material & Mat);

private:
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;

	// skinnedData Output
	std::vector<int> mBoneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
	std::vector<std::string> mBoneName;
	std::unordered_map<std::string, AnimationClip> mAnimations;
};