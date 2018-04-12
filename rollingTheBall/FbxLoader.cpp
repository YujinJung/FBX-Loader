#include <fbxsdk.h>
#include <vector>
#include <winerror.h>
#include <assert.h>
#include "FbxLoader.h"

FbxLoader::FbxLoader()
{
}

FbxLoader::~FbxLoader()
{
}

FbxManager * g_pFbxSdkManager = nullptr;

HRESULT FbxLoader::LoadFBX(std::vector<FbxVertex>& pOutVertexVector)
{
	if (g_pFbxSdkManager == nullptr)
	{
		g_pFbxSdkManager = FbxManager::Create();

		FbxIOSettings* pIOsettings = FbxIOSettings::Create(g_pFbxSdkManager, IOSROOT);
		g_pFbxSdkManager->SetIOSettings(pIOsettings);
	}

	FbxImporter* pImporter = FbxImporter::Create(g_pFbxSdkManager, "");
	FbxScene* pFbxScene = FbxScene::Create(g_pFbxSdkManager, "");

	bool bSuccess = pImporter->Initialize("../../Resource/Tank/cone.FBX", -1, g_pFbxSdkManager->GetIOSettings());

	if (!bSuccess) return E_FAIL;

	bSuccess = pImporter->Import(pFbxScene);
	if (!bSuccess) return E_FAIL;

	pImporter->Destroy();

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

			FbxVector4* pVertices = pMesh->GetControlPoints();

			for (int j = 0; j < pMesh->GetPolygonCount(); j++)
			{
				int iNumVertices = pMesh->GetPolygonSize(j);
				//assert(iNumVertices == 3);

				for (int k = 0; k < iNumVertices; k++) {
					int iControlPointIndex = pMesh->GetPolygonVertex(j, k);

					FbxVertex vertex;
					vertex.pos[0] = (float)pVertices[iControlPointIndex].mData[0];
					vertex.pos[1] = (float)pVertices[iControlPointIndex].mData[1];
					vertex.pos[2] = (float)pVertices[iControlPointIndex].mData[2];

					pOutVertexVector.push_back(vertex);
				}
			}

		}

	}

	return S_OK;
}