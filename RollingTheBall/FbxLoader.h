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

	HRESULT LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<int32_t>& outIndexVector, SkinnedData& outSkinnedData);

	void GetControlPoints(fbxsdk::FbxNode * pFbxRootNode);

	void GetAnimation(FbxScene * pFbxScene, FbxNode * pFbxChildNode, std::vector<DirectX::XMFLOAT4X4>& mBoneOffsets, const std::vector<std::string>& boneName, AnimationClip & animation, std::string & animationName);

	void GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex>& outVertexVector, std::vector<int32_t>& outIndexVector);

	void ReadUV(FbxMesh * pMesh, int inCtrlPointIndex, int inTextureUVIndex, int inUVLayer, DirectX::XMFLOAT2 & outUV);

	void ReadNormal(FbxMesh * pMesh, int inCtrlPointIndex, int inVertexCounter, DirectX::XMFLOAT3 & outNormal);

	void GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex, std::vector<int>& mBoneHierarchy, std::vector<std::string>& boneName);


public:
	FbxAMatrix GetGeometryTransformation(FbxNode * inNode);


private:
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;
/*
private:
	Skeleton mSkeleton;*/

};