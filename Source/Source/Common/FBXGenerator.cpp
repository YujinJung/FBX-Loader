#include "Textures.h"
#include "Materials.h"
#include "Player.h"
#include "FbxLoader.h"
#include "FBXGenerator.h"

FBXGenerator::FBXGenerator()
	:mInBeginEndPair(false)
{
}


FBXGenerator::~FBXGenerator()
{
}

void FBXGenerator::Begin(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, ID3D12DescriptorHeap* cbvHeap)
{
	if (mInBeginEndPair)
		throw std::exception("Cannot nest Begin calls on a FBX Generator");

	mDevice = device;
	mCommandList = cmdList;
	mCbvHeap = cbvHeap;

	mInBeginEndPair = true;
}

void FBXGenerator::End()
{
	if (!mInBeginEndPair)
		throw std::exception("Begin must be called before End");

	mDevice = nullptr;
	mCommandList = nullptr;
	mCbvHeap = nullptr;

	mInBeginEndPair = false;
}

void FBXGenerator::BuildFBXTexture(
	std::vector<Material> &outMaterial,
	std::string inTextureName, std::string inMaterialName,
	Textures& mTextures, Textures& mTexturesNormal, Materials& mMaterials)
{
	// Begin
	mTextures.Begin(mDevice, mCommandList, mCbvHeap);
	mTexturesNormal.Begin(mDevice, mCommandList, mCbvHeap);

	// Load Texture and Material
	int MatIndex = mMaterials.GetSize();
	for (int i = 0; i < outMaterial.size(); ++i)
	{
		std::string TextureName;
		// Load Texture 
		if (!outMaterial[i].Name.empty())
		{
			// Texture
			TextureName = inTextureName;
			TextureName.push_back(i + 48);
			std::wstring TextureFileName;
			TextureFileName.assign(outMaterial[i].Name.begin(), outMaterial[i].Name.end());
			mTextures.SetTexture(
				TextureName,
				TextureFileName);

			// Normal Map
			std::wstring TextureNormalFileName;
			TextureNormalFileName = TextureFileName.substr(0, TextureFileName.size() - 11);
			TextureNormalFileName.append(L"normal.jpg");
			struct stat buffer;
			std::string fileCheck;
			fileCheck.assign(TextureNormalFileName.begin(), TextureNormalFileName.end());
			if (stat(fileCheck.c_str(), &buffer) == 0)
			{
				mTexturesNormal.SetTexture(
					TextureName,
					TextureNormalFileName);
			}
		}

		// Load Material
		std::string MaterialName = inMaterialName;
		MaterialName.push_back(i + 48);

		mMaterials.SetMaterial(
			MaterialName,
			outMaterial[i].DiffuseAlbedo,
			outMaterial[i].FresnelR0,
			outMaterial[i].Roughness,
			MatIndex++,
			mTextures.GetTextureIndex(TextureName),
			mTexturesNormal.GetTextureIndex(TextureName));
	}
	mTextures.End();
	mTexturesNormal.End();
}

void FBXGenerator::LoadFBXPlayer(Player& mPlayer, Textures& mTextures, Textures& mTexturesNormal, Materials& mMaterials)
{
	FbxLoader fbx;
	std::vector<CharacterVertex> outSkinnedVertices;
	std::vector<std::uint32_t> outIndices;
	std::vector<Material> outMaterial;
	SkinnedData outSkinnedInfo;

	// Player
	std::string FileName = "../Resource/FBX/Character/";
	fbx.LoadFBX(outSkinnedVertices, outIndices, outSkinnedInfo, "Idle", outMaterial, FileName);

	fbx.LoadFBX(outSkinnedInfo, "playerWalking", FileName);
	fbx.LoadFBX(outSkinnedInfo, "run", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Kick", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Kick2", FileName);
	fbx.LoadFBX(outSkinnedInfo, "FlyingKick", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Hook", FileName);
	fbx.LoadFBX(outSkinnedInfo, "HitReaction", FileName);
	fbx.LoadFBX(outSkinnedInfo, "Death", FileName);
	fbx.LoadFBX(outSkinnedInfo, "WalkingBackward", FileName);

	mPlayer.BuildGeometry(mDevice, mCommandList, outSkinnedVertices, outIndices, outSkinnedInfo, "playerGeo");

	BuildFBXTexture(outMaterial, "playerTex", "playerMat", mTextures, mTexturesNormal, mMaterials);
}
//
//void FBXGenerator::LoadFBXArchitecture(
//	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries, 
//	Textures& mTextures, Materials& mMaterials)
//{
//	// Architecture FBX
//	FbxLoader fbx;
//	std::vector<Vertex> outVertices;
//	std::vector<std::uint32_t> outIndices;
//	std::vector<Material> outMaterial;
//	std::string FileName;
//
//	std::vector<std::vector<Vertex>> archVertex;
//	std::vector<std::vector<uint32_t>> archIndex;
//	std::vector<std::string> archName;
//
//	FileName = "../Resource/FBX/Architecture/houseA/house";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("house");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Rocks/RockCluster/RockCluster";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("RockCluster");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Canyon/Canyon0";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Canyon0");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Canyon/Canyon1";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Canyon1");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Canyon/Canyon2";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Canyon2");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Rocks/Rock/Rock";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Rock0");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Tree/Tree";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Tree");
//
//	outVertices.clear();
//	outIndices.clear();
//	fbx.clear();
//
//	FileName = "../Resource/FBX/Architecture/Tree/Leaf";
//	fbx.LoadFBX(outVertices, outIndices, outMaterial, FileName);
//	archVertex.push_back(outVertices);
//	archIndex.push_back(outIndices);
//	archName.push_back("Leaf");
//
//	BuildArcheGeometry(archVertex, archIndex, archName, mGeometries);
//	//BuildFBXTexture(outMaterial, "archiTex", "archiMat");
//	BuildFBXTexture(outMaterial, "archiTex", "archiMat", mTextures, mMaterials);
//
//	outVertices.clear();
//	outIndices.clear();
//	outMaterial.clear();
//}
//
//void FBXGenerator::BuildArcheGeometry(
//	const std::vector<std::vector<Vertex>>& outVertices,
//	const std::vector<std::vector<std::uint32_t>>& outIndices,
//	const std::vector<std::string>& geoName,
//	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries)
//{
//	UINT vertexOffset = 0;
//	UINT indexOffset = 0;
//	std::vector<SubmeshGeometry> submesh(geoName.size());
//
//	// Submesh
//	for (int i = 0; i < geoName.size(); ++i)
//	{
//
//		DirectX::BoundingBox box;
//		DirectX::BoundingBox::CreateFromPoints(
//			box,
//			outVertices[i].size(),
//			&outVertices[i][0].Pos,
//			sizeof(Vertex));
//		submesh[i].Bounds = box;
//		submesh[i].IndexCount = (UINT)outIndices[i].size();
//		submesh[i].StartIndexLocation = indexOffset;
//		submesh[i].BaseVertexLocation = vertexOffset;
//
//		vertexOffset += outVertices[i].size();
//		indexOffset += outIndices[i].size();
//	}
//
//	// vertex
//	std::vector<Vertex> vertices(vertexOffset);
//	UINT k = 0;
//	for (int i = 0; i < geoName.size(); ++i)
//	{
//		for (int j = 0; j < outVertices[i].size(); ++j, ++k)
//		{
//			vertices[k].Pos = outVertices[i][j].Pos;
//			vertices[k].Normal = outVertices[i][j].Normal;
//			vertices[k].TexC = outVertices[i][j].TexC;
//		}
//	}
//
//	// index
//	std::vector<std::uint32_t> indices;
//	for (int i = 0; i < geoName.size(); ++i)
//	{
//		indices.insert(indices.end(), outIndices[i].begin(), outIndices[i].end());
//	}
//
//	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
//	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);
//
//	auto geo = std::make_unique<MeshGeometry>();
//	geo->Name = "Architecture";
//
//	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
//	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
//
//	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
//	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
//
//	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
//		mCommandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);
//
//	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
//		mCommandList, indices.data(), ibByteSize, geo->IndexBufferUploader);
//
//	geo->VertexByteStride = sizeof(Vertex);
//	geo->VertexBufferByteSize = vbByteSize;
//	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
//	geo->IndexBufferByteSize = ibByteSize;
//
//	for (int i = 0; i < geoName.size(); ++i)
//	{
//		geo->DrawArgs[geoName[i]] = submesh[i];
//	}
//
//	mGeometries[geo->Name] = std::move(geo);
//}
