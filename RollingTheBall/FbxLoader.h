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

	HRESULT LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector, SkinnedData& outSkinnedData);

	void GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex);

	void GetControlPoints(fbxsdk::FbxNode * pFbxRootNode);

	void GetAnimation(FbxScene * pFbxScene, FbxNode * pFbxChildNode, std::string & outAnimationName);

	void GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector);

public:
	FbxAMatrix GetGeometryTransformation(FbxNode * inNode);


private:
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;

	// skinnedData Output
	std::vector<int> mBoneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
	std::vector<std::string> boneName;
	std::unordered_map<std::string, AnimationClip> mAnimations;
};