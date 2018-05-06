#pragma once

#include <fbxsdk.h>
#include "SkinnedData.h"

struct BoneIndexAndWeight
{
	BYTE mBoneIndices;
	float mBoneWeight;

	bool operator < (const BoneIndexAndWeight& rhs)
	{
		return (mBoneWeight > rhs.mBoneWeight);
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

	HRESULT LoadFBX(
		std::vector<SkinnedVertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		SkinnedData& outSkinnedData,
		const std::string& ClipName,
		std::vector<Material>& outMaterial,
		std::string fileName);

	HRESULT LoadFBX(
		std::vector<Vertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		std::vector<Material>& outMaterial,
		std::string fileName);

	HRESULT LoadFBX(
		AnimationClip & animation,
		const std::string& ClipName,
		std::string fileName);

	bool LoadTXT(
		std::vector<SkinnedVertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		SkinnedData& outSkinnedData,
		const std::string& clipName,
		std::vector<Material>& outMaterial,
		std::string fileName);

	bool LoadTXT(
		std::vector<Vertex>& outVertexVector, 
		std::vector<uint16_t>& outIndexVector,
		std::vector<Material>& outMaterial,
		std::string fileName);

	bool LoadAnimation(
		AnimationClip& animation, 
		const std::string& clipName,
		std::string fileName);


	void GetSkeletonHierarchy(FbxNode * pNode, int curIndex, int parentIndex);

	void GetControlPoints(fbxsdk::FbxNode * pFbxRootNode);


	void GetAnimation(
		FbxScene * pFbxScene,
		FbxNode * pFbxChildNode,
		std::string & outAnimationName,
		const std::string& ClipName);

	void GetOnlyAnimation(
		FbxScene* pFbxScene, 
		FbxNode * pFbxChildNode,
		AnimationClip& inAnimation);


	void GetVerticesAndIndice(
		fbxsdk::FbxMesh * pMesh,
		std::vector<SkinnedVertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		SkinnedData & outSkinnedData);

	void GetVerticesAndIndice(
		FbxMesh * pMesh, 
		std::vector<Vertex>& outVertexVector, 
		std::vector<uint16_t>& outIndexVector);


	void GetMaterials(FbxNode * pNode, std::vector<Material>& outMaterial);

	void GetMaterialAttribute(FbxSurfaceMaterial * pMaterial, Material& outMaterial);

	void GetMaterialTexture(FbxSurfaceMaterial * pMaterial, Material & Mat);

	FbxAMatrix GetGeometryTransformation(FbxNode * pNode);


	void ExportAnimation(
		const AnimationClip& animation,
		std::string fileName,
		const std::string& clipName);

	void ExportFBX(
		std::vector<SkinnedVertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		SkinnedData& outSkinnedData,
		const std::string& clipName,
		std::vector<Material>& outMaterial,
		std::string fileName);

	void ExportFBX(
		std::vector<Vertex>& outVertexVector,
		std::vector<uint16_t>& outIndexVector,
		std::vector<Material>& outMaterial,
		std::string fileName);
	
private:
	std::unordered_map<unsigned int, CtrlPoint*> mControlPoints;

	// skinnedData Output
	std::vector<int> mBoneHierarchy;
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
	std::vector<std::string> mBoneName;
	std::unordered_map<std::string, AnimationClip> mAnimations;
};