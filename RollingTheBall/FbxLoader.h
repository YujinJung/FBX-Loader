#pragma once

#include <fbxsdk.h>
#include "SkinnedData.h"

//struct Keyframe
//{
//	FbxLongLong mFrameNum;
//	FbxAMatrix mGlobalTransform;
//	Keyframe* mNext;
//
//	Keyframe() :
//		mNext(nullptr)
//	{}
//};
//
//struct Joint
//{
//	std::string mName;
//	int mParentIndex;
//	FbxAMatrix mGlobalBindposeInverse;
//	Keyframe* mAnimation;
//	FbxNode* mNode;
//
//	Joint() :
//		mNode(nullptr),
//		mAnimation(nullptr)
//	{
//		mGlobalBindposeInverse.SetIdentity();
//		mParentIndex = -1;
//	}
//
//	~Joint()
//	{
//		while (mAnimation)
//		{
//			Keyframe* temp = mAnimation->mNext;
//			delete mAnimation;
//			mAnimation = temp;
//		}
//	}
//};
//
//struct Skeleton
//{
//	std::vector<Joint> mJoints;
//};
class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(std::vector<Vertex>& outVertexVector, std::vector<int32_t>& outIndexVector, SkinnedData& outSkinnedData);

	void GetVerticesAndIndices(fbxsdk::FbxMesh * pMesh, std::vector<Vertex> & outVertexVector, std::vector<int32_t> & outIndexVector);

	void ReadVertex(fbxsdk::FbxMesh * pMesh, const uint32_t &j, const uint32_t &k, Vertex &outVertex, fbxsdk::FbxVector4 * pVertices);

	void GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex, std::vector<int>& mBoneHierarchy);


public:
	FbxAMatrix GetGeometryTransformation(FbxNode * inNode);


private:

/*
private:
	Skeleton mSkeleton;*/

};