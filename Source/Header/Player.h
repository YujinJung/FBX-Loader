#pragma once

#include "PlayerCamera.h"
#include "PlayerUI.h"
#include "Character.h"

enum class ePlayerMoveList
{
	Walk,
	SideWalk,
	AddYaw,
	AddPitch,
	Death
};

class Player : public Character
{
public:
	Player();
	~Player();

	PlayerUI mUI;
	PlayerCamera mCamera;

public:
	virtual int GetHealth(int i = 0) const override;
	virtual CharacterInfo& GetCharacterInfo(int cIndex = 0);
	virtual void Damage(int damage, DirectX::XMVECTOR Position, DirectX::XMVECTOR Look) override;
	void Attack(Character* inMonster, std::string clipName);

public:
	bool isClipEnd();
	eClipList GetCurrentClip() const;

	DirectX::XMMATRIX GetWorldTransformMatrix() const;

	UINT GetAllRitemsSize() const;
	const std::vector<RenderItem*> GetRenderItem(RenderLayer Type) const;

	void SetClipName(const std::string & inClipName);
	void SetClipTime(float time);

public:
	virtual void BuildGeometry(
		ID3D12Device * device,
		ID3D12GraphicsCommandList * cmdList,
		const std::vector<CharacterVertex>& inVertices,
		const std::vector<std::uint32_t>& inIndices,
		const SkinnedData & inSkinInfo, std::string geoName);
	virtual void BuildRenderItem(
		Materials & mMaterials,
		std::string matrialPrefix) override;


	void UpdateCharacterCBs(
		FrameResource* mCurrFrameResource,
		const Light& mMainLight,
		float* Delay,
		const GameTimer & gt);

	virtual void UpdateCharacterShadows(const Light & mMainLight);

	void UpdatePlayerPosition(ePlayerMoveList move, float velocity);

	void UpdateTransformationMatrix();
	
private:
	CharacterInfo mPlayerInfo;
	SkinnedData mSkinnedInfo;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

private:
	UINT mDamage;
	UINT mFullHealth;
	bool DeathCamFinished = false;

};

