#pragma once

#include "SkinnedData.h"
#include "CharacterMovement.h"

enum class eClipList
{
	Idle,
	StartWalking,
	Walking,
	StopWalking,
	Kick,
	FlyingKick
};

enum class eUIList : int
{
	Rect,
	Background,
	I_Punch,
	I_Kick,
	I_Kick2,
	Count
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Sky,
	Architecture,
	Wall,
	Character,
	Monster,
	Shadow,
	UI,
	Count
};

struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
	float TimePos = 0.0f;
	eClipList mState;
	
	void UpdateSkinnedAnimation(std::string ClipName, float dt)
	{
		TimePos += dt;

		// Loop animation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
		{
			if (ClipName == "Idle" || ClipName == "Walking")
			{
				TimePos = 0.0f;
			}
		}

		eClipList state = eClipList::Idle;
		if (ClipName == "Idle")
		{
			state = eClipList::Idle;
		}
		else if (ClipName == "StartWalking")
			state = eClipList::StartWalking;
		else if (ClipName == "Walking" || ClipName == "WalkingBackward" || ClipName == "run" || ClipName == "playerWalking")
			state = eClipList::Walking;
		else if (ClipName == "StopWalking")
			state = eClipList::StopWalking;
		else if (ClipName == "Kick")
			state = eClipList::Kick;
		else if (ClipName == "FlyingKick")
			state = eClipList::FlyingKick;

		if (state != mState)
		{
			TimePos = 0.0f;
			mState = state;
		}

		// Compute the final transforms for this time position.
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

struct CharacterInfo
{
	ChracterMovement mMovement;
	DirectX::BoundingBox mBoundingBox;
	
	std::string mClipName;
	float mAttackTime;
	int mHealth;
	int mFullHealth;

	bool isDeath;
	
	CharacterInfo()
		: mClipName("Idle"),
		mHealth(100), 
		mFullHealth(100),
		isDeath(false)
	{ }
};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;
	int PlayerCBIndex = -1;
	int MonsterCBIndex = -1;

	int NumFramesDirty = gNumFrameResources;
	
	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	SkinnedModelInstance* SkinnedModelInst = nullptr;
	DirectX::BoundingBox Bounds;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};
