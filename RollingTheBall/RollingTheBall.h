#pragma once

#include "SkinnedData.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct CollisionSphere
{
	CollisionSphere() = default;

	XMFLOAT3 originVector = { 0.0f, 0.0f, 0.0f };
	float radius = 0.0f;
};

class PlayerInfo
{
private:
	XMFLOAT3 mPos;
	XMFLOAT3 mTarget;
	float mYaw, mRoll;
	float mRadius;
	float mPlayerVelocity;
	UINT score;

public:
	PlayerInfo() : mPos(0.0f, 2.0f, 0.0f), mTarget(0.0f, 2.0f, 15.0f), mRadius(2.0f), mPlayerVelocity(0.0f), mYaw(0.0f), mRoll(0.0f)
	{  }

	XMFLOAT3 getPos() const { return mPos; }
	XMFLOAT3 getTarget() const { return mTarget; }
	float getYaw() const { return mYaw; }
	float getRoll() const { return mRoll; }
	float getRadius() const { return mRadius; }
	float getVelocity() const { return mPlayerVelocity; }

	void setPos(const XMFLOAT3& pos) { mPos = pos; }
	void setTarget(const XMFLOAT3& target) { mTarget = target; }
	void setYaw(const float& yaw) { mYaw = yaw; }
	void setRoll(const float& roll) { mRoll = roll; }
	void setRadius(const float& radius) { mRadius = radius; }
	void setVelocity(const float& velocity) { mPlayerVelocity = velocity; }
};

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
	std::string ClipName;
	float TimePos = 0.0f;

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		// Loop animation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
			TimePos = 0.0f;

		// Compute the final transforms for this time position.
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

class RollingTheBall : public D3DApp
{
public:
	RollingTheBall(HINSTANCE hInstance);
	RollingTheBall(const RollingTheBall& rhs) = delete;
	RollingTheBall& operator=(const RollingTheBall& rhs) = delete;
	~RollingTheBall();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdatePlayerPosition(const GameTimer & gt);
	void UpdatePlayerShadow(const GameTimer & gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateMaterialCB(const GameTimer& gt);
	void UpdateAnimationCBs(const GameTimer & gt);

	void LoadTextures();
	void BuildDescriptorHeaps();
	void BuildTextureBufferViews();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildCone();
	void BuildMaterials();
	void BuildPSOs();
	void BuildFrameResources();
	void UpdateObjectShadows();
	void BuildRenderItems();
	void BuildObjectShadows();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	UINT mSkinnedSrvHeapStart = 0;
	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	std::vector<std::string> mSkinnedTextureNames;
	//std::string mSkinnedModelFilename = "Models\\soldier.m3d";
	/*std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;*/

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<std::unique_ptr<CollisionSphere>> mCollisionRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitems[(int)RenderLayer::Count];

	PassConstants mMainPassCB;

	UINT mObjCbvOffset = 0;
	UINT mPassCbvOffset = 0;
	UINT mMatCbvOffset = 0;
	UINT mCbvSrvDescriptorSize = 0;

	bool mIsWireframe = false;
	bool attackMonster = false;

	/*
	* EyePos - EyeTarget
	* player - playerTarget
	* a d - playerTarget Move(Yaw)
	* w s - playerTarget, EyePos Move // EyeDirection based on playerTarget
	* mouse left click - EyeTarget Move
	* mouse right click - EyeTarget, playerTarget(x, z)(Yaw) Move
	*/
	const float mCameraRadius = 15.0f;
	float mCameraPhi = XM_PIDIV2;
	float mCameraTheta = 0.0f;

	Light mMainLight;

	// Player Infomation and Cache render items of player
	PlayerInfo mPlayer;
	RenderItem* mPlayerRitem = nullptr;
	RenderItem* mShadowedPlayerRitem = nullptr;

	// Target Circle
	const std::vector<float> mTargetRadius = { 3.0f, 8.0f, 11.0f };
	XMFLOAT3 mTargetPos = { 0.0f, 1.0f, 70.0f };
	UINT mTargetIndexOffset = 0;
	UINT mTargetIndexEndOffset = 0;
	float mDistanceToTarget = 30.0f;

	XMFLOAT3 mEyePos = { 0.0f, 30.0f, -30.0f };
	XMFLOAT3 mEyeTarget = mPlayer.getPos();
	XMFLOAT3 mEyePosCalc = { mCameraRadius * 2 * sinf(XM_PI / 3.0f), mCameraRadius * 2 * cosf(XM_PI / 3.0f), mCameraRadius * 2 * sinf(XM_PI / 3.0f) };

	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	POINT mLastMousePos;
};
