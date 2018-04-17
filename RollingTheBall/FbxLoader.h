#pragma once

#include <fbxsdk.h>
#include "SkinnedData.h"

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<int32_t>& outIndexVector, SkinnedData& outSkinnedData);

	void GetAnimation(FbxScene * pFbxScene, FbxNode * pFbxChildNode, std::vector<DirectX::XMFLOAT4X4>& mBoneOffsets, const std::vector<std::string>& boneName, AnimationClip & animation, std::string & animationName, std::vector<SkinnedVertex>& outVertexVector, const std::vector<Vertex>& tempVertexVector);

	void GetVerticesAndIndices(fbxsdk::FbxMesh * pMesh, std::vector<Vertex>& tempVertexVector, std::vector<int32_t>& outIndexVector);

	void ReadVertex(fbxsdk::FbxMesh * pMesh, const uint32_t & j, const uint32_t & k, Vertex & outVertex, fbxsdk::FbxVector4 * pVertices);

	void GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex, std::vector<int>& mBoneHierarchy, std::vector<std::string>& boneName);


public:
	FbxAMatrix GetGeometryTransformation(FbxNode * inNode);


private:

/*
private:
	Skeleton mSkeleton;*/

};