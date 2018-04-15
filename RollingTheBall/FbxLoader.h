#pragma once

#include <fbxsdk.h>

class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(std::vector<Vertex>& outVertexVector, std::vector<int32_t>& outIndexVector);

	void ReadVertex(fbxsdk::FbxMesh * pMesh, const uint32_t &j, const uint32_t &k, Vertex &outVertex, fbxsdk::FbxVector4 * pVertices);

	void ProcessSkeletonHierarchy(FbxNode * inRootNode);

	void ProcessSkeletonHierarchyRecursively(FbxNode * inNode, int inDepth, int myIndex, int inParentIndex);

private:

};