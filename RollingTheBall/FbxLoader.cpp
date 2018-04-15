/*
  * http://www.walkerb.net/blog/dx-4/
  */

#include <vector>
#include <winerror.h>
#include <assert.h>
#include "FrameResource.h"
#include "vertexHash.h"
#include "FbxLoader.h"

FbxLoader::FbxLoader()
{
}

FbxLoader::~FbxLoader()
{
}

FbxManager * gFbxManager = nullptr;

HRESULT FbxLoader::LoadFBX(std::vector<Vertex>& outVertexVector, std::vector<int32_t>& outIndexVector)
{
	if (gFbxManager == nullptr)
	{
	    gFbxManager = FbxManager::Create();

		FbxIOSettings* pIOsettings = FbxIOSettings::Create(gFbxManager, IOSROOT);
		gFbxManager->SetIOSettings(pIOsettings);
	}

	FbxImporter* pImporter = FbxImporter::Create(gFbxManager, "");

	//bool bSuccess = pImporter->Initialize("../Resource/cone.FBX", -1, gFbxManager->GetIOSettings());
	bool bSuccess = pImporter->Initialize("../Resource/Boxing.FBX", -1, gFbxManager->GetIOSettings());
	if (!bSuccess) return E_FAIL;

	FbxScene* pFbxScene = FbxScene::Create(gFbxManager, "");

	bSuccess = pImporter->Import(pFbxScene);
	if (!bSuccess) return E_FAIL;

	pImporter->Destroy();

	FbxAxisSystem sceneAxisSystem = pFbxScene->GetGlobalSettings().GetAxisSystem();
	FbxAxisSystem::MayaZUp.ConvertScene(pFbxScene); // Delete?

	// Convert quad to triangle
	FbxGeometryConverter geometryConverter(gFbxManager);
	geometryConverter.Triangulate(pFbxScene, true);

	// Start to RootNode
	FbxNode* pFbxRootNode = pFbxScene->GetRootNode();
	//int numStacks = pFbxScene->GetSrcObjectCount(FBX_TYPE(FbxAnimStack)); i++)
	if (pFbxRootNode)
	{
		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);

			if (pFbxChildNode->GetNodeAttribute() == NULL)
				continue;

			FbxNodeAttribute::EType AttributeType = pFbxChildNode->GetNodeAttribute()->GetAttributeType();

			if (AttributeType != FbxNodeAttribute::eMesh)
				continue;

			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();

			// Temp
			std::unordered_map<Vertex, int32_t> indexMapping;

			// Vertex
			FbxVector4* pVertices = pMesh->GetControlPoints();
			uint32_t vCount = pMesh->GetControlPointsCount();

			/*	for (uint32_t j = 0; j < vCount; ++j)
				{
					vertex.Pos.x = static_cast<float>(pVertices[j].mData[0]);
					vertex.Pos.y = static_cast<float>(pVertices[j].mData[1]);
					vertex.Pos.z = static_cast<float>(pVertices[j].mData[2]);

					outVertexVector.push_back(vertex);
				}*/

			// Triangle
			uint32_t tCount = pMesh->GetPolygonCount();

			for (uint32_t j = 0; j < tCount; ++j)
			{
				for (uint32_t k = 0; k < 3; ++k)
				{
					// Vertces
					Vertex outVertex;

					ReadVertex(pMesh, j, k, outVertex, pVertices);

					// push vertex and index
					auto lookup = indexMapping.find(outVertex);
					if (lookup != indexMapping.end())
					{
						outIndexVector.push_back(lookup->second);
					}
					else
					{
						int32_t index = outVertexVector.size();
						indexMapping[outVertex] = index;
						outIndexVector.push_back(index);
						outVertexVector.push_back(outVertex);
					}
				}
			}
		}
	}

	return S_OK;
}

void FbxLoader::ReadVertex(fbxsdk::FbxMesh * pMesh, const uint32_t &j, const uint32_t &k, Vertex &outVertex, fbxsdk::FbxVector4 * pVertices)
{
	int controlPointIndex = pMesh->GetPolygonVertex(j, k);
	outVertex.Pos.x = static_cast<float>(pVertices[controlPointIndex].mData[0]);
	outVertex.Pos.y = static_cast<float>(pVertices[controlPointIndex].mData[1]);
	outVertex.Pos.z = static_cast<float>(pVertices[controlPointIndex].mData[2]);

	FbxVector4 pNormal;
	pMesh->GetPolygonVertexNormal(j, k, pNormal);
	outVertex.Normal.x = pNormal.mData[0];
	outVertex.Normal.y = pNormal.mData[1];
	outVertex.Normal.z = pNormal.mData[2];

	FbxVector2 pUVs;
	bool bUnMappedUV;
	pMesh->GetPolygonVertexUV(j, k, "", pUVs, bUnMappedUV);
	outVertex.TexC.x = pUVs.mData[0];
	outVertex.TexC.y = pUVs.mData[1];
}

void FbxLoader::ProcessSkeletonHierarchy(FbxNode* inRootNode)
{
	for (int childIndex = 0; childIndex < inRootNode->GetChildCount(); ++childIndex)
	{
		FbxNode* currNode = inRootNode->GetChild(childIndex);
		ProcessSkeletonHierarchyRecursively(currNode, 0, 0, -1);
	}
}

void FbxLoader::ProcessSkeletonHierarchyRecursively(FbxNode* inNode, int inDepth, int myIndex, int inParentIndex)
{
	if (inNode->GetNodeAttribute() && inNode->GetNodeAttribute()->GetAttributeType() && inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Joint currJoint;
		currJoint.mParentIndex = inParentIndex;
		currJoint.mName = inNode->GetName();
		mSkeleton.mJoints.push_back(currJoint);
	}
	for (int i = 0; i < inNode->GetChildCount(); i++)
	{
		ProcessSkeletonHierarchyRecursively(inNode->GetChild(i), inDepth + 1, mSkeleton.mJoints.size(), myIndex);
	}
}