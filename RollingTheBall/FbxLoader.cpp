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

HRESULT FbxLoader::LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector, SkinnedData& outSkinnedData)
{
	if (gFbxManager == nullptr)
	{
	    gFbxManager = FbxManager::Create();

		FbxIOSettings* pIOsettings = FbxIOSettings::Create(gFbxManager, IOSROOT);
		gFbxManager->SetIOSettings(pIOsettings);
	}

	FbxImporter* pImporter = FbxImporter::Create(gFbxManager, "");

	//bool bSuccess = pImporter->Initialize("../Resource/cone.FBX", -1, gFbxManager->GetIOSettings());
	bool bSuccess = pImporter->Initialize("../Resource/Stabbing.FBX", -1, gFbxManager->GetIOSettings());
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
	if (pFbxRootNode)
	{
		// skinnedData Output
		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
		std::unordered_map<std::string, AnimationClip> mAnimations;

		// tempData
		std::vector<std::string> boneName;
		std::vector<Vertex> tempVertexVector;

		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);
			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
			FbxNodeAttribute::EType AttributeType = pMesh->GetAttributeType();
			if (!pMesh || !AttributeType) { continue; }

			switch (AttributeType)
			{
			case FbxNodeAttribute::eSkeleton:
				GetSkeletonHierarchy(pFbxChildNode, 0, -1, mBoneHierarchy, boneName);
				break;
			}
		}

		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);
			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
			FbxNodeAttribute::EType AttributeType = pMesh->GetAttributeType();
			if (!pMesh || !AttributeType) { continue; }

			// Animation
			AnimationClip animation;
			std::string animationName;

			switch (AttributeType)
			{
			case FbxNodeAttribute::eMesh:

				GetControlPoints(pFbxChildNode);

				mBoneOffsets.resize(mBoneHierarchy.size());
				//outVertexVector.resize(tempVertexVector.size());
				// Get Animation Clip
				GetAnimation(pFbxScene, pFbxChildNode, mBoneOffsets, boneName, animation, animationName);
				mAnimations[animationName] = animation;
				outSkinnedData.SetAnimationName(animationName);

				// Get Vertices and indices info
				GetVerticesAndIndice(pMesh, outVertexVector, outIndexVector);

				break;
			}

		}
		outSkinnedData.Set(mBoneHierarchy, mBoneOffsets, mAnimations);
	}

	return S_OK;
}

void FbxLoader::GetControlPoints(fbxsdk::FbxNode * pFbxRootNode)
{
	FbxMesh * pCurrMesh = (FbxMesh*)pFbxRootNode->GetNodeAttribute();
	unsigned int ctrlPointCount = pCurrMesh->GetControlPointsCount();
	for (unsigned int i = 0; i < ctrlPointCount; ++i)
	{
		CtrlPoint* currCtrlPoint = new CtrlPoint();
		DirectX::XMFLOAT3 currPosition;
		currPosition.x = static_cast<float>(pCurrMesh->GetControlPointAt(i).mData[0]);
		currPosition.y = static_cast<float>(pCurrMesh->GetControlPointAt(i).mData[1]);
		currPosition.z = static_cast<float>(pCurrMesh->GetControlPointAt(i).mData[2]);
		currCtrlPoint->mPosition = currPosition;
		mControlPoints[i] = currCtrlPoint;
	}
}

void FbxLoader::GetAnimation(FbxScene* pFbxScene, FbxNode * pFbxChildNode, std::vector<DirectX::XMFLOAT4X4> &mBoneOffsets, const std::vector<std::string>& boneName, AnimationClip &animation, std::string &animationName)
{
	FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
	FbxAMatrix geometryTransform = GetGeometryTransformation(pFbxChildNode);
	
	/*std::vector<BoneIndexAndWeight> vBoneIndexAndWeight;
	vBoneIndexAndWeight.resize(outVertexVector.size());*/

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

			transformMatrix = pCurrCluster->GetTransformMatrix(transformMatrix);	// The transformation of the mesh at binding time
			transformLinkMatrix = pCurrCluster->GetTransformLinkMatrix(transformLinkMatrix);	// The transformation of the cluster(joint) at binding time from joint space to world space
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

			std::string currJointName = pCurrCluster->GetLink()->GetName();
			BYTE currJointIndex;
			for (currJointIndex = 0; currJointIndex< boneName.size(); ++currJointIndex)
			{
				if (boneName[currJointIndex] == currJointName)
					break;
			}

			DirectX::XMFLOAT4X4 boneOffset;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					boneOffset.m[i][j] = globalBindposeInverseMatrix.Get(i, j);
					//boneOffset.m[i][j] = globalBindposeInverseMatrix.Get(i, j);
				}
			}
			mBoneOffsets[currJointIndex] = boneOffset;

			for (uint32_t i = 0; i < pCurrCluster->GetControlPointIndicesCount(); ++i)
			{
				BoneIndexAndWeight currBoneIndexAndWeight;
				currBoneIndexAndWeight.vBoneIndices = currJointIndex;
				currBoneIndexAndWeight.vBoneWeight = pCurrCluster->GetControlPointWeights()[i];

				mControlPoints[pCurrCluster->GetControlPointIndices()[i]]->mBoneInfo.push_back(currBoneIndexAndWeight);
			}

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
				key.TimePos = (float)i / 24.0f;
				FbxAMatrix currentTransformOffset = pFbxChildNode->EvaluateLocalTransform(currTime) * geometryTransform;
				FbxAMatrix temp = currentTransformOffset.Inverse() * pCurrCluster->GetLink()->EvaluateLocalTransform(currTime);

				/*FbxAMatrix currentTransformOffset = pFbxChildNode->EvaluateGlobalTransform(currTime) * geometryTransform;
				FbxAMatrix temp = currentTransformOffset.Inverse() * pCurrCluster->GetLink()->EvaluateGlobalTransform(currTime);*/
				key.Translation = { (float)temp.GetT().mData[0],  (float)temp.GetT().mData[1],  (float)temp.GetT().mData[2] };
				key.RotationQuat = { (float)temp.GetQ().mData[0],  (float)temp.GetQ().mData[1],  (float)temp.GetQ().mData[2] , (float)temp.GetQ().mData[3] };
				key.Scale = { (float)temp.GetS().mData[0],  (float)temp.GetS().mData[1],  (float)temp.GetS().mData[2] };

				boneAnim.Keyframes.push_back(key);
			}

			animation.BoneAnimations.push_back(boneAnim);
		}
	}

	BoneIndexAndWeight currBoneIndexAndWeight;
	currBoneIndexAndWeight.vBoneIndices = 0;
	currBoneIndexAndWeight.vBoneWeight = 0;
	for (auto itr = mControlPoints.begin(); itr != mControlPoints.end(); ++itr)
	{
		for (unsigned int i = itr->second->mBoneInfo.size(); i <= 4; ++i)
		{
			itr->second->mBoneInfo.push_back(currBoneIndexAndWeight);
		}
	}

	/*for (uint32_t i = 0; i < outVertexVector.size(); ++i)
	{
		auto& currVertexVector = outVertexVector[i];
		currVertexVector.BoneWeights = { 0.0f, 0.0f, 0.0f };

		currVertexVector.Pos = tempVertexVector[i].Pos;
		currVertexVector.Normal = tempVertexVector[i].Normal;
		currVertexVector.TexC = tempVertexVector[i].TexC;

		// weight
		for (uint32_t j = 0; j < vBoneIndexAndWeight[i].vBoneWeight.size(); ++j)
		{
			if (j == 3)
				break;
			switch (j)
			{
			case 0:
				currVertexVector.BoneWeights.x = vBoneIndexAndWeight[i].vBoneWeight[j];
				break;
			case 1:
				currVertexVector.BoneWeights.y = vBoneIndexAndWeight[i].vBoneWeight[j];
				break;
			case 2:
				currVertexVector.BoneWeights.z = vBoneIndexAndWeight[i].vBoneWeight[j];
				break;
			}
		}
			
		// indice
		if (vBoneIndexAndWeight[i].vBoneIndices.size() < 4)
		{
			currVertexVector.BoneIndices[0] = currVertexVector.BoneIndices[1]
				= currVertexVector.BoneIndices[2] = currVertexVector.BoneIndices[3] = 0;
		}
		for (uint32_t j = 0; j < vBoneIndexAndWeight[i].vBoneIndices.size(); ++j)
		{
			if (j == 4)
				break;
			
			currVertexVector.BoneIndices[j] = vBoneIndexAndWeight[i].vBoneIndices[j];
		}
	}*/
}

void FbxLoader::GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex> & outVertexVector, std::vector<uint16_t> & outIndexVector)
{
	std::unordered_map<Vertex, uint16_t> indexMapping;
	std::vector<Vertex> tempVertexVector;
	int vertexCounter = 0;
	//FbxVector4* pVertices = pMesh->GetControlPoints();

	uint32_t vCount = pMesh->GetControlPointsCount(); // Vertex
	uint32_t tCount = pMesh->GetPolygonCount(); // Triangle

	for (int j = 0; j < tCount; ++j)
	{
		for (int k = 0; k < 3; ++k)
		{
			// Vertces
			Vertex outVertex;
			// TODO : DELETE
			float t = 1.f;
			//float t = 1.f;

			int controlPointIndex = pMesh->GetPolygonVertex(j, k);
			CtrlPoint* currCtrlPoint = mControlPoints[controlPointIndex];

			outVertex.Pos = currCtrlPoint->mPosition;

			FbxVector4 pNormal;
			pMesh->GetPolygonVertexNormal(j, k, pNormal);
			
			FbxVector2 pUVs;
			bool bUnMappedUV;
			pMesh->GetPolygonVertexUV(j, k, "", pUVs, bUnMappedUV);
			

			/*ReadNormal(pMesh, controlPointIndex, vertexCounter, normal[k]);
			for (int l = 0; l < 1; ++l)
			{
				ReadUV(pMesh, controlPointIndex, pMesh->GetTextureUVIndex(j, k), k, UV[k][l]);
			}*/
			
			////
			Vertex temp;
			temp.Pos.x = currCtrlPoint->mPosition.x * t;
			temp.Pos.y = currCtrlPoint->mPosition.y * t;
			temp.Pos.z = currCtrlPoint->mPosition.z * t;

			temp.Normal.x = pNormal.mData[0] * t;
			temp.Normal.y = pNormal.mData[1] * t;
			temp.Normal.z = pNormal.mData[2] * t;

			// Need to modify TexC
			temp.TexC.x = pUVs.mData[0] * t;
			temp.TexC.y = pUVs.mData[1] * t;
			/*temp.Normal = normal[k];
			temp.TexC = UV[k][0];*/

			// push vertex and index
			auto lookup = indexMapping.find(temp);

			if (lookup != indexMapping.end())
			{
				outIndexVector.push_back(lookup->second);
			}
			else
			{
				uint16_t index = tempVertexVector.size();
				indexMapping[temp] = index;
				tempVertexVector.push_back(temp);
				outIndexVector.push_back(index);

				SkinnedVertex skinnedVertexInfo;
				skinnedVertexInfo.Pos = temp.Pos;
				skinnedVertexInfo.Normal = temp.Normal;
				skinnedVertexInfo.TexC = temp.TexC;

				currCtrlPoint->SortBlendingInfoByWeight();

				for (int l = 0; l < currCtrlPoint->mBoneInfo.size(); ++l)
				{
					if (l >= 4)
						break;
					skinnedVertexInfo.BoneIndices[l] = currCtrlPoint->mBoneInfo[l].vBoneIndices;

					switch (l)
					{
					case 0:
						skinnedVertexInfo.BoneWeights.x = currCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					case 1:
						skinnedVertexInfo.BoneWeights.y = currCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					case 2:
						skinnedVertexInfo.BoneWeights.z = currCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					}
				}

				outVertexVector.push_back(skinnedVertexInfo);
			}

		}
			//++vertexCounter;
	}
}

void FbxLoader::ReadUV(FbxMesh* pMesh, int inCtrlPointIndex, int inTextureUVIndex, int inUVLayer, DirectX::XMFLOAT2& outUV)
{
	if (inUVLayer >= 2 || pMesh->GetElementUVCount() <= inUVLayer)
	{
		throw std::exception("Invalid UV Layer Number");
	}
	FbxGeometryElementUV* vertexUV = pMesh->GetElementUV(inUVLayer);

	switch (vertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(inCtrlPointIndex);
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		case FbxGeometryElement::eIndexToDirect:
		{
			outUV.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(inTextureUVIndex).mData[0]);
			outUV.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(inTextureUVIndex).mData[1]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;
	}
}

void FbxLoader::ReadNormal(FbxMesh* pMesh, int inCtrlPointIndex, int inVertexCounter, DirectX::XMFLOAT3& outNormal)
{
	if (pMesh->GetElementNormalCount() < 1)
	{
		throw std::exception("Invalid Normal Number");
	}

	FbxGeometryElementNormal* vertexNormal = pMesh->GetElementNormal(0);
	switch (vertexNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inCtrlPointIndex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inCtrlPointIndex);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(inVertexCounter).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(inVertexCounter);
			outNormal.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			outNormal.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			outNormal.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;

		default:
			throw std::exception("Invalid Reference");
		}
		break;
	}
}

void FbxLoader::GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex, std::vector<int>& mBoneHierarchy, std::vector<std::string>& boneName)
{
	mBoneHierarchy.push_back(parentIndex);
	boneName.push_back(inNode->GetName());

	for (int i = 0; i < inNode->GetChildCount(); ++i)
	{
		GetSkeletonHierarchy(inNode->GetChild(i), mBoneHierarchy.size(), curIndex, mBoneHierarchy, boneName);
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