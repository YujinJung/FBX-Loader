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

HRESULT FbxLoader::LoadFBX(std::vector<SkinnedVertex>& outVertexVector, std::vector<uint16_t>& outIndexVector, SkinnedData& outSkinnedData, std::vector<Material>& outMaterial)
{
	if (gFbxManager == nullptr)
	{
	    gFbxManager = FbxManager::Create();

		FbxIOSettings* pIOsettings = FbxIOSettings::Create(gFbxManager, IOSROOT);
		gFbxManager->SetIOSettings(pIOsettings);
	}

	FbxImporter* pImporter = FbxImporter::Create(gFbxManager, "");

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
				GetVerticesAndIndice(pMesh, outVertexVector, outIndexVector, outSkinnedData);

				GetMaterials(pFbxChildNode, outMaterial);

				break;
			}

		}
		outSkinnedData.Set(mBoneHierarchy, mBoneOffsets, mAnimations);
	}

	return S_OK;
}

void FbxLoader::GetSkeletonHierarchy(FbxNode * pNode, int curIndex, int parentIndex)
{
	mBoneHierarchy.push_back(parentIndex);
	mBoneName.push_back(pNode->GetName());

	for (int i = 0; i < pNode->GetChildCount(); ++i)
	{
		GetSkeletonHierarchy(pNode->GetChild(i), mBoneHierarchy.size(), curIndex);
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
	animation.BoneAnimations.resize(mBoneName.size());
	for (int i = 0; i < mBoneName.size(); ++i)
	{
		BoneAnimation initBoneAnim;
		for (int i = 0; i < 59; ++i) // 60 frames
		{
			Keyframe key;

			key.TimePos = static_cast<float>(i / 24.0f);
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
			for (currJointIndex = 0; currJointIndex< mBoneName.size(); ++currJointIndex)
			{
				if (mBoneName[currJointIndex] == currJointName)
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
				mControlPoints[controlPointIndices[i]]->mBoneName = currJointName;
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

			// TRqS transformation and Time per frame
			for (FbxLongLong i = start.GetFrameCount(FbxTime::eFrames60); i <= end.GetFrameCount(FbxTime::eFrames60); ++i)
			{
				FbxTime currTime;
				currTime.SetFrame(i, FbxTime::eFrames60);

				Keyframe key;
				key.TimePos = static_cast<float>(i / 24.0f);

				FbxAMatrix currentTransformOffset = pSceneEvaluator->GetNodeGlobalTransform(pFbxChildNode, currTime) * geometryTransform;
				FbxAMatrix temp = currentTransformOffset.Inverse() * pSceneEvaluator->GetNodeGlobalTransform(pCurrCluster->GetLink(), currTime);

				// Transition, Scaling and Rotation Quaternion
				FbxVector4 TS = temp.GetT();
				key.Translation = { 
					static_cast<float>(TS.mData[0]), 
					static_cast<float>(TS.mData[1]),  
					static_cast<float>(TS.mData[2]) };
				TS = temp.GetS();
				key.Scale = { 
					static_cast<float>(TS.mData[0]),  
					static_cast<float>(TS.mData[1]),  
					static_cast<float>(TS.mData[2]) };
				FbxQuaternion Q = temp.GetQ();
				key.RotationQuat = { 
					static_cast<float>(Q.mData[0]),  
					static_cast<float>(Q.mData[1]),  
					static_cast<float>(Q.mData[2]) , 
					static_cast<float>(Q.mData[3]) };

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

void FbxLoader::GetVerticesAndIndice(fbxsdk::FbxMesh * pMesh, std::vector<SkinnedVertex> & outVertexVector, std::vector<uint16_t> & outIndexVector, SkinnedData& outSkinnedData)
{
	// Vertex and Index
	std::unordered_map<std::string, std::vector<uint16_t>> IndexVector;
	std::unordered_map<Vertex, uint16_t> IndexMapping;
	uint32_t VertexIndex = 0;

	// Material
	FbxLayerElementArrayTemplate<int>* MaterialIndices;
	FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eNone;
	MaterialIndices = &(pMesh->GetElementMaterial()->GetIndexArray());
	MaterialMappingMode = pMesh->GetElementMaterial()->GetMappingMode();

	uint32_t tCount = pMesh->GetPolygonCount(); // Triangle
	bool isSameCount = MaterialIndices->GetCount() == tCount;

	for (int i = 0; i < tCount; ++i)
	{
		// For indexing by bone
		std::string CurrBoneName = mControlPoints[pMesh->GetPolygonVertex(i, 0)]->mBoneName;

		// Material
		uint16_t MaterialIndex = MaterialIndices->GetAt(0);

		if (MaterialIndices)
		{
			switch (MaterialMappingMode)
			{
			case FbxGeometryElement::eByPolygon:
			{
				if (isSameCount)
				{
					MaterialIndex = MaterialIndices->GetAt(i);
				}
			}
			break;
			}
		}

		// Vertex and Index info
		for (int j = 0; j < 3; ++j)
		{
			int controlPointIndex = pMesh->GetPolygonVertex(i, j);
			CtrlPoint* CurrCtrlPoint = mControlPoints[controlPointIndex];

			// Normal
			FbxVector4 pNormal;
			pMesh->GetPolygonVertexNormal(i, j, pNormal);

			// UV
			float * lUVs = NULL;
			FbxStringList lUVNames;
			pMesh->GetUVSetNames(lUVNames);
			const char * lUVName = NULL;
			if (lUVNames.GetCount())
			{
				lUVName = lUVNames[0];
			}

			FbxVector2 pUVs;
			bool bUnMappedUV;	
			if (!pMesh->GetPolygonVertexUV(i, j, lUVName, pUVs, bUnMappedUV))
			{
				MessageBox(0, L"UV not found", 0, 0);
			}
			
			Vertex Temp;
			// Position
			Temp.Pos.x = CurrCtrlPoint->mPosition.x;
			Temp.Pos.y = CurrCtrlPoint->mPosition.y;
			Temp.Pos.z = CurrCtrlPoint->mPosition.z;

			// Normal
			Temp.Normal.x = pNormal.mData[0];
			Temp.Normal.y = pNormal.mData[1];
			Temp.Normal.z = pNormal.mData[2];

			// UV
			Temp.TexC.x = pUVs.mData[0];
			Temp.TexC.y = 1.0f - pUVs.mData[1];

			// push vertex and index
			auto lookup = IndexMapping.find(Temp);

			if (lookup != IndexMapping.end())
			{
				IndexVector[CurrBoneName].push_back(lookup->second);
			}
			else
			{
				// Index
				uint16_t Index = VertexIndex++;
				IndexMapping[Temp] = Index;
				IndexVector[CurrBoneName].push_back(Index);

				// Vertex
				SkinnedVertex SkinnedVertexInfo;
				SkinnedVertexInfo.Pos = Temp.Pos;
				SkinnedVertexInfo.Normal = Temp.Normal;
				SkinnedVertexInfo.TexC = Temp.TexC;
				SkinnedVertexInfo.MaterialIndex = MaterialIndex;

				CurrCtrlPoint->SortBlendingInfoByWeight();

				// Set the Bone information
				for (int l = 0; l < CurrCtrlPoint->mBoneInfo.size(); ++l)
				{
					if (l >= 4)
						break;

					SkinnedVertexInfo.BoneIndices[l] = CurrCtrlPoint->mBoneInfo[l].vBoneIndices;

					switch (l)
					{
					case 0:
						SkinnedVertexInfo.BoneWeights.x = CurrCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					case 1:
						SkinnedVertexInfo.BoneWeights.y = CurrCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					case 2:
						SkinnedVertexInfo.BoneWeights.z = CurrCtrlPoint->mBoneInfo[l].vBoneWeight;
						break;
					}
				}

				outVertexVector.push_back(SkinnedVertexInfo);
			}
		}

	}

	for (int i = 0; i < mBoneName.size(); ++i)
	{
		auto CurrIndexVector = IndexVector[mBoneName[i]];
		int IndexCount = CurrIndexVector.size();

		outSkinnedData.SetSubmeshOffset(IndexCount);

		outIndexVector.insert(outIndexVector.end(), CurrIndexVector.begin(), CurrIndexVector.end());
	}
}

FbxAMatrix FbxLoader::GetGeometryTransformation(FbxNode* pNode)
{
	if (!pNode)
	{
		throw std::exception("Null for mesh geometry");
	}

	const FbxVector4 lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	return FbxAMatrix(lT, lR, lS);
}

void FbxLoader::GetMaterials(FbxNode* pNode, std::vector<Material>& outMaterial)
{
	uint32_t MaterialCount = pNode->GetMaterialCount();

	for (int i = 0; i < MaterialCount; ++i)
	{
		FbxSurfaceMaterial* SurfaceMaterial = pNode->GetMaterial(i);
		GetMaterialAttribute(SurfaceMaterial, outMaterial);
		GetMaterialTexture(SurfaceMaterial, outMaterial[i]);
	}
}

void FbxLoader::GetMaterialAttribute(FbxSurfaceMaterial* pMaterial, std::vector<Material>& outMaterial)
{
	FbxDouble3 double3;
	FbxDouble double1;
	if (pMaterial->GetClassId().Is(FbxSurfacePhong::ClassId))
	{
		Material currMaterial;

		// Amibent Color
		double3 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Ambient;
		currMaterial.Ambient.x = static_cast<float>(double3.mData[0]);
		currMaterial.Ambient.y = static_cast<float>(double3.mData[1]);
		currMaterial.Ambient.z = static_cast<float>(double3.mData[2]);

		// Diffuse Color
		double3 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Diffuse;
		currMaterial.DiffuseAlbedo.x = static_cast<float>(double3.mData[0]);
		currMaterial.DiffuseAlbedo.y = static_cast<float>(double3.mData[1]);
		currMaterial.DiffuseAlbedo.z = static_cast<float>(double3.mData[2]);

		// Roughness 
		double1 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Shininess;
		currMaterial.Roughness = 1 - double1;

		// Reflection
		double3 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Reflection;
		currMaterial.FresnelR0.x = static_cast<float>(double3.mData[0]);
		currMaterial.FresnelR0.y = static_cast<float>(double3.mData[1]);
		currMaterial.FresnelR0.z = static_cast<float>(double3.mData[2]);

		// Specular Color
		double3 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Specular;
		currMaterial.Specular.x = static_cast<float>(double3.mData[0]);
		currMaterial.Specular.y = static_cast<float>(double3.mData[1]);
		currMaterial.Specular.z = static_cast<float>(double3.mData[2]);

		// Emissive Color
		double3 = reinterpret_cast<FbxSurfacePhong *>(pMaterial)->Emissive;
		currMaterial.Emissive.x = static_cast<float>(double3.mData[0]);
		currMaterial.Emissive.y = static_cast<float>(double3.mData[1]);
		currMaterial.Emissive.z = static_cast<float>(double3.mData[2]);
		
		/*
		// Transparency Factor
		double1 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->TransparencyFactor;
		currMaterial->mTransparencyFactor = double1;

		// Specular Factor
		double1 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->SpecularFactor;
		currMaterial->mSpecularPower = double1;


		// Reflection Factor
	double1 = reinterpret_cast<FbxSurfacePhong *>(inMaterial)->ReflectionFactor;
		currMaterial->mReflectionFactor = double1;	*/

		outMaterial.push_back(currMaterial);
	}
	else if (pMaterial->GetClassId().Is(FbxSurfaceLambert::ClassId))
	{
		Material currMaterial;

		// Amibent Color
		double3 = reinterpret_cast<FbxSurfaceLambert *>(pMaterial)->Ambient;
		currMaterial.Ambient.x = static_cast<float>(double3.mData[0]);
		currMaterial.Ambient.y = static_cast<float>(double3.mData[1]);
		currMaterial.Ambient.z = static_cast<float>(double3.mData[2]);

		// Diffuse Color
		double3 = reinterpret_cast<FbxSurfaceLambert *>(pMaterial)->Diffuse;
		currMaterial.DiffuseAlbedo.x = static_cast<float>(double3.mData[0]);
		currMaterial.DiffuseAlbedo.y = static_cast<float>(double3.mData[1]);
		currMaterial.DiffuseAlbedo.z = static_cast<float>(double3.mData[2]);

		// Emissive Color
		double3 = reinterpret_cast<FbxSurfaceLambert *>(pMaterial)->Emissive;
		currMaterial.Emissive.x = static_cast<float>(double3.mData[0]);
		currMaterial.Emissive.y = static_cast<float>(double3.mData[1]);
		currMaterial.Emissive.z = static_cast<float>(double3.mData[2]);

		outMaterial.push_back(currMaterial);
	}
}

void FbxLoader::GetMaterialTexture(FbxSurfaceMaterial * pMaterial, Material & Mat)
{
	unsigned int textureIndex = 0;
	FbxProperty property;

	FBXSDK_FOR_EACH_TEXTURE(textureIndex)
	{
		property = pMaterial->FindProperty(FbxLayerElement::sTextureChannelNames[textureIndex]);
		if (property.IsValid())
		{
			unsigned int textureCount = property.GetSrcObjectCount<FbxTexture>();
			for (unsigned int i = 0; i < textureCount; ++i)
			{
				FbxLayeredTexture* layeredTexture = property.GetSrcObject<FbxLayeredTexture>(i);
				if (layeredTexture)
				{
					throw std::exception("Layered Texture is currently unsupported\n");
				}
				else
				{
					FbxTexture* texture = property.GetSrcObject<FbxTexture>(i);
					if (texture)
					{
						std::string textureType = property.GetNameAsCStr();
						FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);

						if (fileTexture)
						{
							if (textureType == "DiffuseColor")
							{
								Mat.Name = fileTexture->GetFileName();
							}
							/*else if (textureType == "SpecularColor")
							{
							Mat->mSpecularMapName = fileTexture->GetFileName();
							}
							else if (textureType == "Bump")
							{
							Mat->mNormalMapName = fileTexture->GetFileName();
							}*/
						}
					}
				}
			}
		}
	}
}

