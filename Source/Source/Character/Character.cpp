
#include "Character.h"

using namespace DirectX;

Character::Character()
{
}

Character::~Character()
{
}

void Character::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<CharacterVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	const SkinnedData& inSkinInfo,
	std::string geoName)
{
	if (inVertices.size() == 0)
	{
		MessageBox(0, L"Fbx not found", 0, 0);
		return;
	}

	UINT vCount = 0, iCount = 0;
	vCount = (UINT)inVertices.size();
	iCount = (UINT)inIndices.size();

	const UINT vbByteSize = vCount * sizeof(CharacterVertex);
	const UINT ibByteSize = iCount * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(CharacterVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	auto vSubmeshOffset = inSkinInfo.GetSubmeshOffset();
	auto vBoneName = inSkinInfo.GetBoneName();

	const int numOfSubmesh = 65;
	UINT SubmeshOffsetIndex = 0;
	for (int i = 0; i < numOfSubmesh; ++i)
	{
		if (i >= vSubmeshOffset.size())
		{
			SubmeshGeometry FbxSubmesh;
			FbxSubmesh.IndexCount = 0;
			FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
			FbxSubmesh.BaseVertexLocation = 0;

			std::string SubmeshName = vBoneName[0] + std::to_string(i);
			geo->DrawArgs[SubmeshName] = FbxSubmesh;
			const_cast<SkinnedData&>(inSkinInfo).SetBoneName(SubmeshName);
			continue;
		}

		UINT CurrSubmeshOffsetIndex = vSubmeshOffset[i];

		SubmeshGeometry FbxSubmesh;
		FbxSubmesh.IndexCount = CurrSubmeshOffsetIndex;
		FbxSubmesh.StartIndexLocation = SubmeshOffsetIndex;
		FbxSubmesh.BaseVertexLocation = 0;

		std::string SubmeshName = vBoneName[i];
		geo->DrawArgs[SubmeshName] = FbxSubmesh;

		SubmeshOffsetIndex += CurrSubmeshOffsetIndex;
	}

	BoundingBox box;
	BoundingBox::CreateFromPoints(
		box,
		inVertices.size(),
		&inVertices[0].Pos,
		sizeof(CharacterVertex));
	box.Extents = { 3.0f, 1.0f, 3.0f };
	box.Center.y = 3.0f;

	mInitBoundsBox = box;

	mGeometry = std::move(geo);
}
