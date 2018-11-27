#include "Materials.h"

Materials::Materials()
{
}

Materials::~Materials()
{
}

UINT Materials::GetSize() const
{
	return (UINT)mMaterials.size();
}

Material * Materials::Get(std::string Name)
{
	return mMaterials[Name].get();
}

void Materials::SetMaterial(
	const std::string& Name,
	const DirectX::XMFLOAT4& DiffuseAlbedo,
	const DirectX::XMFLOAT3& FresnelR0,
	const float& Roughness, const int& MatIndex,
	const int& DiffuseSrvHeapIndex,
	const int& NormalSrvHeapIndex)
{
	auto temp = std::make_unique<Material>();
	temp->Name = Name;
	temp->MatCBIndex = MatIndex;
	temp->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex;
	temp->NormalSrvHeapIndex = NormalSrvHeapIndex;
	temp->DiffuseAlbedo = DiffuseAlbedo;
	temp->FresnelR0 = FresnelR0;
	temp->Roughness = Roughness;

	mMaterials[Name] = std::move(temp);
}
void Materials::SetMaterial(
	const std::vector<std::string>& Name,
	const std::vector<DirectX::XMFLOAT4>& DiffuseAlbedo,
	const std::vector<DirectX::XMFLOAT3>& FresnelR0,
	const std::vector<float>& Roughness, int MatIndex,
	const std::vector<int>& DiffuseSrvHeapIndex,
	const std::vector<int>& NormalSrvHeapIndex)
{
	for (int i = 0; i < Name.size(); ++i)
	{
		auto temp = std::make_unique<Material>();
		temp->Name = Name[i];
		temp->MatCBIndex = MatIndex++;
		temp->DiffuseSrvHeapIndex = DiffuseSrvHeapIndex[i];
		temp->NormalSrvHeapIndex = NormalSrvHeapIndex[i];
		temp->DiffuseAlbedo = DiffuseAlbedo[i];
		temp->FresnelR0 = FresnelR0[i];
		temp->Roughness = Roughness[i];

		if (temp->Name == "bricks0")
		{
			temp->NormalSrvHeapIndex = 0;
		}

		mMaterials[Name[i]] = std::move(temp);
	}
}

void Materials::UpdateMaterialCB(UploadBuffer<MaterialConstants>* currMaterialCB)
{
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			DirectX::XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}

