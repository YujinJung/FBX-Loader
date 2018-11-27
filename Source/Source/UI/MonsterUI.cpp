#include "MonsterUI.h"
#include "Materials.h"

using namespace DirectX;

MonsterUI::MonsterUI()
{
}


MonsterUI::~MonsterUI()
{
}

UINT MonsterUI::GetSize() const
{
	return (UINT)mAllRitems.size();
}

const std::vector<RenderItem*> MonsterUI::GetRenderItem(eUIList Type)const
{
	return mRitems[(int)Type];
}

void MonsterUI::SetDamageScale(int cIndex, float inScale)
{
	mWorldTransform[cIndex].Scale.x = inScale;
}
void MonsterUI::DeleteMonsterUI(int cIndex) // cIndex starts zero
{
	mAllRitems.erase(mAllRitems.begin() + 4 * cIndex, mAllRitems.begin() + 4 * cIndex + 4);
	mRitems[(int)eUIList::Rect].erase(mRitems[(int)eUIList::Rect].begin() + 4 * cIndex, mRitems[(int)eUIList::Rect].begin() + 4 * cIndex + 4);
	mWorldTransform.erase(mWorldTransform.begin() + cIndex);
}

void MonsterUI::BuildRenderItem(
	std::unordered_map<std::string,
	std::unique_ptr<MeshGeometry>>& mGeometries,
	Materials & mMaterials,
	std::string monsterName,
	UINT numOfMonster)
{
	int UIIndex = 0;
	for (UINT i = 0; i < numOfMonster; ++i)
	{
		// place over the head
		// odd are background HP bar

		XMMATRIX uiWorldTransformSRx = XMMatrixScaling(0.02f, 1.0f, 0.003f)  * XMMatrixRotationX(XM_PIDIV2);
		if (i == 0)
		{
			uiWorldTransformSRx = XMMatrixScaling(2.0f, 1.0f, 1.0f) * uiWorldTransformSRx;
		}

		auto frontHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&frontHealthBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 1.9f, 0.5005f));
		frontHealthBar->TexTransform = MathHelper::Identity4x4();
		frontHealthBar->Mat = mMaterials.Get("red");
		frontHealthBar->Geo = mGeometries["shapeGeo"].get();
		frontHealthBar->ObjCBIndex = UIIndex++;
		frontHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		frontHealthBar->StartIndexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		frontHealthBar->BaseVertexLocation = frontHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		frontHealthBar->IndexCount = frontHealthBar->Geo->DrawArgs["hpBar"].IndexCount;

		auto frontBGHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&frontBGHealthBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 1.9f, 0.5f));
		frontBGHealthBar->TexTransform = MathHelper::Identity4x4();
		frontBGHealthBar->Mat = mMaterials.Get("bricks0");
		frontBGHealthBar->Geo = mGeometries["shapeGeo"].get();
		frontBGHealthBar->ObjCBIndex = UIIndex++;
		frontBGHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		frontBGHealthBar->StartIndexLocation = frontBGHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		frontBGHealthBar->BaseVertexLocation = frontBGHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		frontBGHealthBar->IndexCount = frontBGHealthBar->Geo->DrawArgs["hpBar"].IndexCount;


		uiWorldTransformSRx = XMMatrixScaling(0.02f, 1.0f, 0.003f)  * XMMatrixRotationX(-XM_PIDIV2);
		if (i == 0)
		{
			uiWorldTransformSRx = XMMatrixScaling(2.0f, 1.0f, 1.0f) * uiWorldTransformSRx;
		}
		auto backHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&backHealthBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 1.9f, 0.4995f));
		backHealthBar->TexTransform = MathHelper::Identity4x4();
		backHealthBar->Mat = mMaterials.Get("red");
		backHealthBar->Geo = mGeometries["shapeGeo"].get();
		backHealthBar->ObjCBIndex = UIIndex++;
		backHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backHealthBar->StartIndexLocation = backHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		backHealthBar->BaseVertexLocation = backHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		backHealthBar->IndexCount = backHealthBar->Geo->DrawArgs["hpBar"].IndexCount;

		auto backBGHealthBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&backBGHealthBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 1.9f, 0.5f));
		backBGHealthBar->TexTransform = MathHelper::Identity4x4();
		backBGHealthBar->Mat = mMaterials.Get("bricks0");
		backBGHealthBar->Geo = mGeometries["shapeGeo"].get();
		backBGHealthBar->ObjCBIndex = UIIndex++;
		backBGHealthBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backBGHealthBar->StartIndexLocation = backBGHealthBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		backBGHealthBar->BaseVertexLocation = backBGHealthBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		backBGHealthBar->IndexCount = backBGHealthBar->Geo->DrawArgs["hpBar"].IndexCount;

		// Name
		uiWorldTransformSRx = XMMatrixScaling(0.02f, 1.0f, 0.008f) * XMMatrixRotationX(-XM_PIDIV2);
		auto backNameBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&backNameBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 2.0f, 0.4995f));
		backNameBar->TexTransform = MathHelper::Identity4x4();
		backNameBar->Mat = mMaterials.Get(monsterName);
		backNameBar->Geo = mGeometries["shapeGeo"].get();
		backNameBar->ObjCBIndex = UIIndex++;
		backNameBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backNameBar->StartIndexLocation = backNameBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		backNameBar->BaseVertexLocation = backNameBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		backNameBar->IndexCount = backNameBar->Geo->DrawArgs["hpBar"].IndexCount;

		uiWorldTransformSRx *= XMMatrixRotationY(XM_PI);
		auto frontNameBar = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&frontNameBar->World, uiWorldTransformSRx * XMMatrixTranslation(0.0f, 2.0f, 0.5005f));
		frontNameBar->TexTransform = MathHelper::Identity4x4();
		frontNameBar->Mat = mMaterials.Get(monsterName);
		frontNameBar->Geo = mGeometries["shapeGeo"].get();
		frontNameBar->ObjCBIndex = UIIndex++;
		frontNameBar->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		frontNameBar->StartIndexLocation = frontNameBar->Geo->DrawArgs["hpBar"].StartIndexLocation;
		frontNameBar->BaseVertexLocation = frontNameBar->Geo->DrawArgs["hpBar"].BaseVertexLocation;
		frontNameBar->IndexCount = frontNameBar->Geo->DrawArgs["hpBar"].IndexCount;


		mRitems[(int)eUIList::Rect].push_back(frontHealthBar.get());
		mAllRitems.push_back(std::move(frontHealthBar));
		mRitems[(int)eUIList::Rect].push_back(backHealthBar.get());
		mAllRitems.push_back(std::move(backHealthBar));
		mRitems[(int)eUIList::Rect].push_back(frontBGHealthBar.get());
		mAllRitems.push_back(std::move(frontBGHealthBar));
		mRitems[(int)eUIList::Rect].push_back(backBGHealthBar.get());
		mAllRitems.push_back(std::move(backBGHealthBar));
		mRitems[(int)eUIList::Rect].push_back(frontNameBar.get());
		mAllRitems.push_back(std::move(frontNameBar));
		mRitems[(int)eUIList::Rect].push_back(backNameBar.get());
		mAllRitems.push_back(std::move(backNameBar));
	}

	mWorldTransform.resize(numOfMonster);
}

void MonsterUI::UpdateUICBs(
	UploadBuffer<UIConstants>* currUICB,
	std::vector<XMMATRIX> playerWorlds,
	std::vector<XMVECTOR> inEyeLeft,
	bool mTransformDirty)
{
	int uIndex = 0;
	int mIndex = -1;
	int mOffset = GetSize() / playerWorlds.size();
	// need to check c Indexcheck
	for (auto& e : mAllRitems)
	{
		if (mTransformDirty) { e->NumFramesDirty = gNumFrameResources;	}
		if (mIndex != uIndex / mOffset)
		{
			mIndex = uIndex / mOffset;
		}

		if (e->NumFramesDirty > 0)
		{
			XMVECTOR UIoffset;

			int res = e->ObjCBIndex % 6;
			// front HP bar 6n
			if (e->ObjCBIndex % 6 == 0)
			{
				UIoffset = (1.0f - mWorldTransform[mIndex].Scale.x) * (-inEyeLeft[mIndex]) * 0.8f;
			}
			// back HP bar 6n + 3
			else if (e->ObjCBIndex % 6 == 2)
			{
				UIoffset = (1.0f - mWorldTransform[mIndex].Scale.x) * inEyeLeft[mIndex] * 0.8f;
			}

			if (mIndex == 0) { UIoffset *= 2.0f; }

			XMMATRIX T = XMMatrixTranslation(
				mWorldTransform[mIndex].Position.x + UIoffset.m128_f32[0],
				mWorldTransform[mIndex].Position.y + UIoffset.m128_f32[1],
				mWorldTransform[mIndex].Position.z + UIoffset.m128_f32[2]);;
			XMMATRIX S = XMMatrixScaling(
				mWorldTransform[mIndex].Scale.x,
				mWorldTransform[mIndex].Scale.y,
				mWorldTransform[mIndex].Scale.z);

			// Background Health Bar / 6n + 2, 3, 4, 5
			if (res == 1 || res == 5 || res == 3 || res == 4)
			{
				T = XMMatrixTranslation(
					mWorldTransform[mIndex].Position.x,
					mWorldTransform[mIndex].Position.y,
					mWorldTransform[mIndex].Position.z);
				S = XMMatrixIdentity();
			}


			XMMATRIX world = XMLoadFloat4x4(&e->World) * playerWorlds[mIndex] * T;
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			UIConstants uiConstants;
			XMStoreFloat4x4(&uiConstants.World, XMMatrixTranspose(world) * S);
			XMStoreFloat4x4(&uiConstants.TexTransform, XMMatrixTranspose(texTransform));

			currUICB->CopyData(e->ObjCBIndex, uiConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
		
		uIndex++;
	}
}