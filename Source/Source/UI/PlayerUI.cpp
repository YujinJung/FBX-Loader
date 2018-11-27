#include "Materials.h"
#include "PlayerUI.h"

using namespace DirectX;

PlayerUI::PlayerUI()
{
	mWorldTransform.Scale = { 1.0f, 1.0f, 1.0f };
	mWorldTransform.Position = { 0.0f, 0.0f, 0.0f };
}


PlayerUI::~PlayerUI()
{
}

UINT PlayerUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}
const std::vector<RenderItem*> PlayerUI::GetRenderItem(eUIList Type)
{
	return mRitems[(int)Type];
}

void PlayerUI::SetPosition(FXMVECTOR inPosition)
{
	XMStoreFloat3(&mWorldTransform.Position, inPosition);
}
void PlayerUI::SetDamageScale(float inScale)
{
	mWorldTransform.Scale.x = inScale;
}
void PlayerUI::SetGameover()
{
	// Show Gameover Text
	auto& gameoverWorld = mRitems[(int)eUIList::Rect].back()->World;
	XMStoreFloat4x4(&gameoverWorld, XMLoadFloat4x4(&gameoverWorld) * XMMatrixRotationY(XM_PI));

	// Hide Skill Icon
	for (int i = (int)eUIList::I_Punch; i < (int)eUIList::Count; ++i)
	{
		auto& e = mRitems[i][0]->World;
		XMStoreFloat4x4(&e, XMLoadFloat4x4(&e) * XMMatrixRotationY(XM_PI));
	}
}

void PlayerUI::BuildGeometry(
	ID3D12Device * device,
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<UIVertex>& inVertices,
	const std::vector<std::uint32_t>& inIndices,
	std::string geoName)
{
	UINT vCount = 0, iCount = 0;
	vCount = (UINT)inVertices.size();
	iCount = (UINT)inIndices.size();

	const UINT vbByteSize = vCount * sizeof(UIVertex);
	const UINT ibByteSize = iCount * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = geoName;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), inVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), inIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inVertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, inIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(UIVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry UISubmesh;
	UISubmesh.IndexCount = (UINT)inIndices.size();
	UISubmesh.StartIndexLocation = 0;
	UISubmesh.BaseVertexLocation = 0;

	geo->DrawArgs["SkillUI"] = UISubmesh;
	
	mGeometry = std::move(geo);
}

void PlayerUI::BuildRenderItem(
	std::unordered_map<std::string,
	std::unique_ptr<MeshGeometry>>& mGeometries,
	Materials & mMaterials)
{
	int UIIndex = 0;

	// place over the head
	auto frontHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&frontHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.0021f) * XMMatrixRotationX(-atan(3.0f / 2.0f)) * XMMatrixTranslation(0.0f, 0.901f, 0.0f));
	frontHealthBar->TexTransform = MathHelper::Identity4x4();
	frontHealthBar->Mat = mMaterials.Get("ice0");
	frontHealthBar->Geo = mGeometries["shapeGeo"].get();
	frontHealthBar->ObjCBIndex = UIIndex++;
	frontHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontHealthBar->StartIndexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
	frontHealthBar->BaseVertexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	frontHealthBar->IndexCount = frontHealthBar->Geo->DrawArgs["hpBar"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(frontHealthBar.get());
	mAllRitems.push_back(std::move(frontHealthBar));

	auto bgHealthBar = std::make_unique<RenderItem>();
	// atan(theta) : Theta is associated with PlayerCamera
	XMStoreFloat4x4(&bgHealthBar->World, XMMatrixScaling(0.01f, 1.0f, 0.002f) * XMMatrixRotationX(-atan(3.0f / 2.0f))  * XMMatrixTranslation(0.0f, 0.9f, 0.001f));
	bgHealthBar->TexTransform = MathHelper::Identity4x4();
	bgHealthBar->Mat = mMaterials.Get("stone0");
	bgHealthBar->Geo = mGeometries["shapeGeo"].get();
	bgHealthBar->ObjCBIndex = UIIndex++;
	bgHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	bgHealthBar->StartIndexLocation = bgHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
	bgHealthBar->BaseVertexLocation = bgHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	bgHealthBar->IndexCount = bgHealthBar->Geo->DrawArgs["hpBar"].IndexCount;
	mRitems[(int)eUIList::Rect].push_back(bgHealthBar.get());
	mAllRitems.push_back(std::move(bgHealthBar));


	//// Skill Icon
	//const float skillIconScale = 0.02f;
	//XMMATRIX skillIconWorldSRx = XMMatrixScaling(skillIconScale, 1.0f, skillIconScale) * XMMatrixRotationX(-atan(3.0f / 2.0f));
	//auto iconDelayKick = std::make_unique<RenderItem>();
	//XMStoreFloat4x4(&iconDelayKick->World, skillIconWorldSRx * XMMatrixTranslation(-skillIconScale * 10.0f, -1.0f, 0.0f));
	//iconDelayKick->TexTransform = MathHelper::Identity4x4();
	//iconDelayKick->Mat = mMaterials.Get("iconPunch");
	//iconDelayKick->Geo = mGeometry.get();
	//iconDelayKick->ObjCBIndex = UIIndex++;
	//iconDelayKick->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//iconDelayKick->StartIndexLocation = iconDelayKick->Geo->DrawArgs["SkillUI"].StartIndexLocation;
	//iconDelayKick->BaseVertexLocation = iconDelayKick->Geo->DrawArgs["SkillUI"].BaseVertexLocation;
	//iconDelayKick->IndexCount = iconDelayKick->Geo->DrawArgs["SkillUI"].IndexCount;
	//mRitems[(int)eUIList::I_Punch].push_back(iconDelayKick.get());
	//mAllRitems.push_back(std::move(iconDelayKick));
	//skillFullTime.push_back(3.0f);

	//auto iconKick = std::make_unique<RenderItem>();
	//XMStoreFloat4x4(&iconKick->World, skillIconWorldSRx  * XMMatrixTranslation(0.0f, -1.0f, 0.0f));
	//iconKick->TexTransform = MathHelper::Identity4x4();
	//iconKick->Mat = mMaterials.Get("iconKick");
	//iconKick->Geo = mGeometry.get();
	//iconKick->ObjCBIndex = UIIndex++;
	//iconKick->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//iconKick->StartIndexLocation = iconKick->Geo->DrawArgs["SkillUI"].StartIndexLocation;
	//iconKick->BaseVertexLocation= iconKick->Geo->DrawArgs["SkillUI"].BaseVertexLocation;
	//iconKick->IndexCount = iconKick->Geo->DrawArgs["SkillUI"].IndexCount;
	//mRitems[(int)eUIList::I_Kick].push_back(iconKick.get());
	//mAllRitems.push_back(std::move(iconKick));
	//skillFullTime.push_back(5.0f);

	//auto iconKick2 = std::make_unique<RenderItem>();
	//XMStoreFloat4x4(&iconKick2->World, skillIconWorldSRx  * XMMatrixTranslation(skillIconScale * 10.0f, -1.0f, 0.0f));
	//iconKick2->TexTransform = MathHelper::Identity4x4();
	//iconKick2->Mat = mMaterials.Get("iconKick2");
	//iconKick2->Geo = mGeometry.get();
	//iconKick2->ObjCBIndex = UIIndex++;
	//iconKick2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//iconKick2->StartIndexLocation = iconKick2->Geo->DrawArgs["SkillUI"].StartIndexLocation;
	//iconKick2->BaseVertexLocation = iconKick2->Geo->DrawArgs["SkillUI"].BaseVertexLocation;
	//iconKick2->IndexCount = iconKick2->Geo->DrawArgs["SkillUI"].IndexCount;
	//mRitems[(int)eUIList::I_Kick2].push_back(iconKick2.get());
	//mAllRitems.push_back(std::move(iconKick2));
	//skillFullTime.push_back(10.0f);

	//auto GameoverUI = std::make_unique<RenderItem>();
	//// atan(theta) : Theta is associated with PlayerCamera
	//XMStoreFloat4x4(&GameoverUI->World, XMMatrixScaling(0.3f, 1.0f, 0.061f) * XMMatrixRotationX(-atan(3.0f / 2.0f)) * XMMatrixRotationY(XM_PI) * XMMatrixTranslation(0.0f, 2.0f, -5.0f));
	//GameoverUI->TexTransform = MathHelper::Identity4x4();
	//GameoverUI->Mat = mMaterials.Get("Gameover");
	//GameoverUI->Geo = mGeometries["shapeGeo"].get();
	//GameoverUI->ObjCBIndex = UIIndex++;
	//GameoverUI->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//GameoverUI->StartIndexLocation = GameoverUI->Geo->DrawArgs["hpBar"].StartIndexLocation;
	//GameoverUI->BaseVertexLocation = GameoverUI->Geo->DrawArgs["hpBar"].BaseVertexLocation;
	//GameoverUI->IndexCount = GameoverUI->Geo->DrawArgs["hpBar"].IndexCount;
	//mRitems[(int)eUIList::Rect].push_back(GameoverUI.get());
	//mAllRitems.push_back(std::move(GameoverUI));
}

void PlayerUI::UpdateUICBs(
	UploadBuffer<UIConstants>* curUICB,
	XMMATRIX playerWorld,
	XMVECTOR inEyeLeft,
	float* Delay,
	bool mTransformDirty)
{
	int iconIndex = 0;
	for (auto& e : mAllRitems)
	{
		// if Transform then Reset the Dirty flag
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources; }

		if (e->NumFramesDirty > 0)
		{
			XMMATRIX T = XMMatrixTranslation(
				mWorldTransform.Position.x,
				mWorldTransform.Position.y,
				mWorldTransform.Position.z);
			XMMATRIX S = XMMatrixIdentity();

			// Health Bar move to left
			XMVECTOR UIoffset = XMVectorZero();
			if (e->ObjCBIndex == 0)
			{
				UIoffset = (1.0f - mWorldTransform.Scale.x) * inEyeLeft * 0.1f;

				T = T * XMMatrixTranslationFromVector(UIoffset);
				S = XMMatrixScaling(
					mWorldTransform.Scale.x,
					mWorldTransform.Scale.y,
					mWorldTransform.Scale.z);
			}
			
			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorld * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			float remainingTime;
			if (iconIndex >= 2 && iconIndex < 2 + skillFullTime.size()) // 2, 3, 4
			{
				// iconIndex - 2 -> health Bar and bg bar
				remainingTime = skillFullTime[iconIndex - 2] - Delay[iconIndex];
				if (remainingTime < 0.0f) remainingTime = 0.0f;
				uiConstants.Scale = remainingTime / skillFullTime[iconIndex - 2];
			}

			curUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
		iconIndex++;
	}
}

