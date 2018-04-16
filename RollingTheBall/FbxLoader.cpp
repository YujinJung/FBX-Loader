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

HRESULT FbxLoader::LoadFBX(std::vector<Vertex>& outVertexVector, std::vector<int32_t>& outIndexVector, SkinnedData& outSkinnedData)
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
		// skinnedData ///////////////
		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
		std::unordered_map<std::string, AnimationClip> mAnimations;

		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);
			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
			FbxNodeAttribute::EType AttributeType = pMesh->GetAttributeType();
			FbxAMatrix geometryTransform = GetGeometryTransformation(pFbxChildNode);
			if (!pMesh || !AttributeType) { continue; }


			// Animation
			AnimationClip animation;
			std::string animationName;

			switch (AttributeType)
			{
			case FbxNodeAttribute::eMesh:
				// Get Vertices and indices info
				GetVerticesAndIndices(pMesh, outVertexVector, outIndexVector);

				/// Deformer - Cluster - Link
				// Deformer
				for (uint32_t deformerIndex = 0; deformerIndex < pMesh->GetDeformerCount(); ++deformerIndex)
				{
					FbxSkin* pCurrSkin = reinterpret_cast<FbxSkin*>(pMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
					if (!pCurrSkin) { continue; }

					// Cluster
					for (uint32_t clusterIndex = 0; clusterIndex < pCurrSkin->GetClusterCount(); ++clusterIndex)
					{
						FbxCluster* pCurrCluster = pCurrSkin->GetCluster(clusterIndex);
						FbxAMatrix transformMatrix;
						FbxAMatrix transformLinkMatrix;
						FbxAMatrix globalBindposeInverseMatrix;

						pCurrCluster->GetTransformMatrix(transformMatrix);	// The transformation of the mesh at binding time
						pCurrCluster->GetTransformLinkMatrix(transformLinkMatrix);	// The transformation of the cluster(joint) at binding time from joint space to world space
						globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

						DirectX::XMFLOAT4X4 boneOffset;
						for (int i = 0; i < 4; ++i)
						{
							for (int j = 0; j < 4; ++j)
							{
								boneOffset.m[i][j] = globalBindposeInverseMatrix.Get(i, j);
							}
						}
						mBoneOffsets.push_back(boneOffset);

						
						BoneAnimation boneAnim;

						FbxAnimStack* pCurrAnimStack = pFbxScene->GetSrcObject<FbxAnimStack>(0);
						FbxString currAnimStackName = pCurrAnimStack->GetName();
						animationName = currAnimStackName.Buffer();

						FbxTakeInfo* takeInfo = pFbxScene->GetTakeInfo(currAnimStackName);
						FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
						FbxTime end = takeInfo->mLocalTimeSpan.GetStop();

						for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames24); i <= end.GetFrameCount(FbxTime::eFrames24); ++i)
						{
							FbxTime currTime;
							currTime.SetFrame(i, FbxTime::eFrames24);

							Keyframe key;
							key.TimePos = i;
							FbxAMatrix currentTransformOffset = pFbxChildNode->EvaluateGlobalTransform(currTime) * geometryTransform;
							FbxAMatrix temp = currentTransformOffset.Inverse() * pCurrCluster->GetLink()->EvaluateGlobalTransform(currTime);
							key.Translation = { (float)temp.GetT().mData[0],  (float)temp.GetT().mData[1],  (float)temp.GetT().mData[2] };
							key.RotationQuat = { (float)temp.GetR().mData[0],  (float)temp.GetR().mData[1],  (float)temp.GetR().mData[2] , 0.0f};
							key.Scale = { (float)temp.GetS().mData[0],  (float)temp.GetS().mData[1],  (float)temp.GetS().mData[2] };

							boneAnim.Keyframes.push_back(key);
						}

						animation.BoneAnimations.push_back(boneAnim);
					}
				}
				mAnimations[animationName] = animation;
				
				outSkinnedData.SetAnimationName(animationName);
				break;

			case FbxNodeAttribute::eSkeleton:
				GetSkeletonHierarchy(pFbxChildNode, 0, -1, mBoneHierarchy);
				break;
			}

		}
		outSkinnedData.Set(mBoneHierarchy, mBoneOffsets, mAnimations);
	}

	return S_OK;
}

void FbxLoader::GetVerticesAndIndices(fbxsdk::FbxMesh * pMesh, std::vector<Vertex> & outVertexVector, std::vector<int32_t> & outIndexVector)
{
	// Temp
	std::unordered_map<Vertex, int32_t> indexMapping;

	FbxVector4* pVertices = pMesh->GetControlPoints();

	uint32_t vCount = pMesh->GetControlPointsCount(); // Vertex
	uint32_t tCount = pMesh->GetPolygonCount(); // Triangle

	for (uint32_t j = 0; j < tCount; ++j)
	{
		for (uint32_t k = 0; k < 3; ++k)
		{
			// Vertces
			Vertex outVertex;
			ReadVertex(pMesh, j, k, outVertex, pVertices);

			// push vertex and index
			{
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

void FbxLoader::GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex, std::vector<int>& mBoneHierarchy)
{
	mBoneHierarchy.push_back(parentIndex);

	for (int i = 0; i < inNode->GetChildCount(); ++i)
	{
		GetSkeletonHierarchy(inNode->GetChild(i), mBoneHierarchy.size(), curIndex, mBoneHierarchy);
	}
}

FbxAMatrix FbxLoader::GetGeometryTransformation(FbxNode* inNode)
{
	if (!inNode)
	{
		throw std::exception("Null for mesh geometry");
	}

	const FbxVector4 lT = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = inNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}