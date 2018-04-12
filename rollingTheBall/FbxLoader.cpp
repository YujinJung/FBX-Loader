/*
  * http://www.walkerb.net/blog/dx-4/
  */

#include <fbxsdk.h>
#include <vector>
#include <winerror.h>
#include <assert.h>
#include "FrameResource.h"
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

	bool bSuccess = pImporter->Initialize("../Resource/tank_fbx_Body.FBX", -1, gFbxManager->GetIOSettings());
	if (!bSuccess) return E_FAIL;

	FbxScene* pFbxScene = FbxScene::Create(gFbxManager, "");

	bSuccess = pImporter->Import(pFbxScene);
	if (!bSuccess) return E_FAIL;

	pImporter->Destroy();

	FbxAxisSystem sceneAxisSystem = pFbxScene->GetGlobalSettings().GetAxisSystem();
	//FbxAxisSystem::MayaYUp.ConvertScene(pFbxScene); // Delete?

	// Convert quad to triangle
	FbxGeometryConverter geometryConverter(gFbxManager);
	geometryConverter.Triangulate(pFbxScene, true);
	
	// Start to RootNode
	FbxNode* pFbxRootNode = pFbxScene->GetRootNode();

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
			
			// Vertex
			FbxVector4* pVertices = pMesh->GetControlPoints();
			uint32_t vCount = pMesh->GetControlPointsCount();
			Vertex vertex;

			for (uint32_t j = 0; j < vCount; ++j)
			{
				vertex.Pos.x = static_cast<float>(pVertices[j].mData[0]);
				vertex.Pos.y = static_cast<float>(pVertices[j].mData[1]);
				vertex.Pos.z = static_cast<float>(pVertices[j].mData[2]);

				outVertexVector.push_back(vertex);
			}

			// Triangle
			uint32_t tCount = pMesh->GetPolygonCount();
			
			for (uint32_t j = 0; j < tCount; ++j)
			{
				for (uint32_t k = 0; k < 3; ++k)
				{
					int controlPointIndex = pMesh->GetPolygonVertex(j, k);

					outIndexVector.push_back(controlPointIndex);
				}
			}

			//for (int j = 0; j < pMesh->GetPolygonCount(); j++)
			//{
			//	int iNumVertices = pMesh->GetPolygonSize(j);
			//	//assert(iNumVertices == 3);

			//	for (int k = 0; k < iNumVertices; k++) 
			//	{
			//		FbxVector4 pNormal;
			//		int iControlPointIndex = pMesh->GetPolygonVertex(j, k);
			//		pMesh->GetPolygonVertexNormal(j, iControlPointIndex, pNormal);

			//		Vertex vertex;
			//		vertex.Pos.x = (float)pVertices[iControlPointIndex].mData[0];
			//		vertex.Pos.y = (float)pVertices[iControlPointIndex].mData[1];
			//		vertex.Pos.z = (float)pVertices[iControlPointIndex].mData[2];
			//		vertex.Normal.x = pNormal.mData[0];
			//		vertex.Normal.y = pNormal.mData[1];
			//		vertex.Normal.z = pNormal.mData[2];
			//		
			//		int32_t index;
			//		index = pMesh->GetPolygonVertexIndex(j);

			//		outVertexVector.push_back(vertex);
			//		outIndexVector.push_back(iControlPointIndex);
			//	}
			//}

			
		}
	}

	return S_OK;
}