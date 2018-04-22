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
	bool bSuccess = pImporter->Initialize("../Resource/FBX/Boxing.FBX", -1, gFbxManager->GetIOSettings());
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
		// Skeleton Bone Hierarchy Index 
		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);
			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
			FbxNodeAttribute::EType AttributeType = pMesh->GetAttributeType();
			if (!pMesh || !AttributeType) { continue; }

			switch (AttributeType)
			{
			case FbxNodeAttribute::eSkeleton:
				GetSkeletonHierarchy(pFbxChildNode, 0, -1);
				break;
			}
		}

		// Bone offset, Control point, Vertex, Index Data
		// And Animation Data
		for (int i = 0; i < pFbxRootNode->GetChildCount(); i++)
		{
			FbxNode* pFbxChildNode = pFbxRootNode->GetChild(i);
			FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
			FbxNodeAttribute::EType AttributeType = pMesh->GetAttributeType();
			if (!pMesh || !AttributeType) { continue; }

			

			switch (AttributeType)
			{
			case FbxNodeAttribute::eMesh:

				GetControlPoints(pFbxChildNode);

				// To access the bone index directly
				mBoneOffsets.resize(mBoneHierarchy.size());

				// Get Animation Clip
				std::string outAnimationName;
				GetAnimation(pFbxScene, pFbxChildNode, outAnimationName);
				outSkinnedData.SetAnimationName(outAnimationName);

				// Get Vertices and indices info
				GetVerticesAndIndice(pMesh, outVertexVector, outIndexVector);

				break;
			
				// TODO Material

			}

		}
		outSkinnedData.Set(mBoneHierarchy, mBoneOffsets, mAnimations);
	}

	return S_OK;
}

void FbxLoader::GetSkeletonHierarchy(FbxNode * inNode, int curIndex, int parentIndex)
{
	mBoneHierarchy.push_back(parentIndex);
	boneName.push_back(inNode->GetName());

	for (int i = 0; i < inNode->GetChildCount(); ++i)
	{
		GetSkeletonHierarchy(inNode->GetChild(i), mBoneHierarchy.size(), curIndex);
	}
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

void FbxLoader::GetAnimation(FbxScene* pFbxScene, FbxNode * pFbxChildNode, std::string &outAnimationName)
{
	FbxMesh* pMesh = (FbxMesh*)pFbxChildNode->GetNodeAttribute();
	FbxAMatrix geometryTransform = GetGeometryTransformation(pFbxChildNode);

	// Animation Data
	AnimationClip animation;

	// Initialize BoneAnimations
	animation.BoneAnimations.resize(boneName.size());
	for (int i = 0; i < boneName.size(); ++i)
	{
		BoneAnimation initBoneAnim;
		for (int i = 0; i < 59; ++i) // 60 frames
		{
			Keyframe key;

			key.TimePos = (float)i / 24.0f;
			key.Translation = { 0.0f, 0.0f, 0.0f };
			key.Scale = { 1.0f, 1.0f, 1.0f };
			key.RotationQuat = { 0.0f, 0.0f, 0.0f, 0.0f };
			initBoneAnim.Keyframes.push_back(key);
		}
		animation.BoneAnimations[i] = initBoneAnim;
	}

	// Deformer - Cluster - Link
	// Deformer
	for (uint32_t deformerIndex = 0; deformerIndex < pMesh->GetDeformerCount(); ++deformerIndex)
	{
		FbxSkin* pCurrSkin = reinterpret_cast<FbxSkin*>(pMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
		if (!pCurrSkin) { continue; }
		
		// Cluster
		for (uint32_t clusterIndex = 0; clusterIndex < pCurrSkin->GetClusterCount(); ++clusterIndex)
		{
			FbxCluster* pCurrCluster = pCurrSkin->GetCluster(clusterIndex);

			FbxAMatrix transformMatrix, transformLinkMatrix;
			FbxAMatrix globalBindposeInverseMatrix;

			transformMatrix = pCurrCluster->GetTransformMatrix(transformMatrix);	// The transformation of the mesh at binding time
			transformLinkMatrix = pCurrCluster->GetTransformLinkMatrix(transformLinkMatrix);	// The transformation of the cluster(joint) at binding time from joint space to world space
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * geometryTransform;

			// To find the index that matches the name of the current joint
			std::string currJointName = pCurrCluster->GetLink()->GetName();
			BYTE currJointIndex; // current joint index
			for (currJointIndex = 0; currJointIndex< boneName.size(); ++currJointIndex)
			{
				if (boneName[currJointIndex] == currJointName)
					break;
			}

			// Set the BoneOffset Matrix
			DirectX::XMFLOAT4X4 boneOffset;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					boneOffset.m[i][j] = globalBindposeInverseMatrix.Get(i, j);
				}
			}
			mBoneOffsets[currJointIndex] = boneOffset;

			// Set the Bone index and weight ./ Max 4
			auto controlPointIndices =  pCurrCluster->GetControlPointIndices();
			for (uint32_t i = 0; i < pCurrCluster->GetControlPointIndicesCount(); ++i)
			{
				BoneIndexAndWeight currBoneIndexAndWeight;
				currBoneIndexAndWeight.vBoneIndices = currJointIndex;
				currBoneIndexAndWeight.vBoneWeight = pCurrCluster->GetControlPointWeights()[i];

				mControlPoints[controlPointIndices[i]]->mBoneInfo.push_back(currBoneIndexAndWeight);
			}
			
			// Set the Bone Animation Matrix
			BoneAnimation boneAnim;

			FbxAnimStack* pCurrAnimStack = pFbxScene->GetSrcObject<FbxAnimStack>(0);
			FbxString currAnimStackName = pCurrAnimStack->GetName();
			outAnimationName = currAnimStackName.Buffer();

			FbxAnimEvaluator* pSceneEvaluator = pFbxScene->GetAnimationEvaluator();

			FbxTakeInfo* takeInfo = pFbxScene->GetTakeInfo(currAnimStackName);
			FbxTime start = takeInfo->mReferenceTimeSpan.GetStart();
			FbxTime end = takeInfo->mReferenceTimeSpan.GetStop();

			for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames60); i <= end.GetFrameCount(FbxTime::eFrames60); ++i)
			{
				FbxTime currTime;
				currTime.SetFrame(i, FbxTime::eFrames60);

				Keyframe key;
				key.TimePos = (float)i / 24.0f;

				FbxAMatrix currentTransformOffset = pSceneEvaluator->GetNodeGlobalTransform(pFbxChildNode, currTime) * geometryTransform;
				FbxAMatrix temp = currentTransformOffset.Inverse() * pSceneEvaluator->GetNodeGlobalTransform(pCurrCluster->GetLink(), currTime);

				FbxVector4 TS = temp.GetT();
				key.Translation = { (float)TS.mData[0],  (float)TS.mData[1],  (float)TS.mData[2] };
				TS = temp.GetS();
				key.Scale = { (float)TS.mData[0],  (float)TS.mData[1],  (float)TS.mData[2] };
				FbxQuaternion Q = temp.GetQ();
				key.RotationQuat = { (float)Q.mData[0],  (float)Q.mData[1],  (float)Q.mData[2] , (float)Q.mData[3] };

				boneAnim.Keyframes.push_back(key);
			}

			animation.BoneAnimations[currJointIndex] = boneAnim;
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

	mAnimations[outAnimationName] = animation;
}

void FbxLoader::GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex> & outVertexVector, std::vector<uint16_t> & outIndexVector)
{
	std::unordered_map<Vertex, uint16_t> indexMapping;
	uint32_t vertexIndex = 0;

	uint32_t tCount = pMesh->GetPolygonCount(); // Triangle
	for (int j = 0; j < tCount; ++j)
	{
		for (int k = 0; k < 3; ++k)
		{
			int controlPointIndex = pMesh->GetPolygonVertex(j, k);
			CtrlPoint* currCtrlPoint = mControlPoints[controlPointIndex];

			FbxVector4 pNormal;
			pMesh->GetPolygonVertexNormal(j, k, pNormal);
			
			FbxVector2 pUVs;
			bool bUnMappedUV;
			pMesh->GetPolygonVertexUV(j, k, "", pUVs, bUnMappedUV);
			
			Vertex temp;
			// Position
			temp.Pos.x = currCtrlPoint->mPosition.x;
			temp.Pos.y = currCtrlPoint->mPosition.y;
			temp.Pos.z = currCtrlPoint->mPosition.z;

			// Normal
			temp.Normal.x = pNormal.mData[0];
			temp.Normal.y = pNormal.mData[1];
			temp.Normal.z = pNormal.mData[2];

			// UV
			temp.TexC.x = pUVs.mData[0];
			temp.TexC.y = pUVs.mData[1];

			// push vertex and index
			auto lookup = indexMapping.find(temp);

			if (lookup != indexMapping.end())
			{
				outIndexVector.push_back(lookup->second);
			}
			else
			{
				// Index
				uint16_t index = vertexIndex++;
				indexMapping[temp] = index;
				outIndexVector.push_back(index);

				// Vertex
				SkinnedVertex skinnedVertexInfo;
				skinnedVertexInfo.Pos = temp.Pos;
				skinnedVertexInfo.Normal = temp.Normal;
				skinnedVertexInfo.TexC = temp.TexC;

				currCtrlPoint->SortBlendingInfoByWeight();

				// Set the Bone information
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