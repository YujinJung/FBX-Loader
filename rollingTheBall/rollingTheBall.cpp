//***************************************************************************************
// ShapesApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"

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

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
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

class rollingTheBall : public D3DApp
{
public:
	rollingTheBall(HINSTANCE hInstance);
	rollingTheBall(const rollingTheBall& rhs) = delete;
	rollingTheBall& operator=(const rollingTheBall& rhs) = delete;
	~rollingTheBall();

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

	void LoadTextures();
	void BuildDescriptorHeaps();
	void BuildTextureBufferViews();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		rollingTheBall theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

rollingTheBall::rollingTheBall(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

rollingTheBall::~rollingTheBall()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

//-------------------------------------------------------------------------------------------------------------------------------
bool rollingTheBall::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildObjectShadows();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildTextureBufferViews();
	BuildConstantBufferViews();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

//-------------------------------------------------------------------------------------------------------------------------------
void rollingTheBall::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void rollingTheBall::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	UpdateMaterialCB(gt);
}

void rollingTheBall::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(3, passCbvHandle);
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Opaque]);

	mCommandList->OMSetStencilRef(0);
	mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Shadow]);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void rollingTheBall::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void rollingTheBall::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void rollingTheBall::OnMouseMove(WPARAM btnState, int x, int y)
{
	XMVECTOR eyePos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR eyeTarget = XMVectorSet(mEyeTarget.x, mEyeTarget.y, mEyeTarget.z, 0.0f);
	XMVECTOR EyePosCalc = XMVectorSet(mEyePosCalc.x, mEyePosCalc.y, mEyePosCalc.z, 1.0f);

	XMFLOAT3 mPlayerTarget = mPlayer.getTarget();
	XMFLOAT3 mPlayerPos = mPlayer.getPos();
	float mPlayerYaw = mPlayer.getYaw();
	float mPlayerVelocity = mPlayer.getVelocity();

	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.5f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.5f*static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mCameraTheta += dx;
		mCameraPhi += dy;
		mCameraPhi = MathHelper::Clamp(mCameraPhi, 0.1f, MathHelper::Pi - 0.1f);

		mEyeTarget.x = mEyePos.x + 2 * mCameraRadius * sinf(mCameraPhi) * sinf(mPlayerYaw + mCameraTheta);
		mEyeTarget.z = mEyePos.z + 2 * mCameraRadius * sinf(mCameraPhi) * cosf(mPlayerYaw + mCameraTheta);
		mEyeTarget.y = mEyePos.y + 2 * mCameraRadius * cosf(mCameraPhi);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		XMVECTOR playerPos = XMVectorSet(mPlayerPos.x, mPlayerPos.y, mPlayerPos.z, 1.0f);
		mCameraTheta = 0.0f;

		float dx = XMConvertToRadians(0.5f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.5f*static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mPlayerYaw += dx;
		mCameraPhi += dy;
		mCameraPhi = MathHelper::Clamp(mCameraPhi, 0.1f, MathHelper::Pi - 0.1f);

		XMVECTOR EyePos = XMVectorSet(sinf(mPlayerYaw + XM_PI), 1.0f, cosf(mPlayerYaw + XM_PI), 1.0f);
		eyePos = XMVectorMultiplyAdd(EyePosCalc, EyePos, playerPos);
		XMStoreFloat3(&mEyePos, eyePos);

		mPlayerTarget.x = mPlayerPos.x + mCameraRadius * sinf(mCameraPhi) * sinf(mPlayerYaw);
		mPlayerTarget.z = mPlayerPos.z + mCameraRadius * sinf(mCameraPhi) * cosf(mPlayerYaw);
	}

	mPlayer.setPos(mPlayerPos);
	mPlayer.setTarget(mPlayerTarget);
	mPlayer.setYaw(mPlayerYaw);
	mPlayer.setVelocity(mPlayerVelocity);

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void rollingTheBall::OnKeyboardInput(const GameTimer& gt)
{
	XMFLOAT3 mPlayerTarget = mPlayer.getTarget();
	XMFLOAT3 mPlayerPos = mPlayer.getPos();
	float mPlayerYaw = mPlayer.getYaw();
	float mPlayerVelocity = mPlayer.getVelocity();
	bool pushKey = false;
	bool restart = false;

	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;


	if ((GetAsyncKeyState('W') & 0x8000) && mPlayerPos.z < mTargetPos.z - 20.0f)
	{
		pushKey = true;
		mCameraTheta = 0.0f;
		if (mPlayerVelocity <= 0.01f)
			mPlayerVelocity += 0.000001f;
	}
	else if ((GetAsyncKeyState('S') & 0x8000) && mPlayerPos.z < mTargetPos.z - 20.0f)
	{
		pushKey = true;
		mCameraTheta = 0.0f;
		if (mPlayerVelocity >= -0.01f)
			mPlayerVelocity -= 0.000001f;
	}
	else
	{
		if (mPlayerVelocity > 0.0f)
		{
			pushKey = true;
			mPlayerVelocity -= 0.0000005f;
		}
		else if (mPlayerVelocity < 0.0f)
		{
			pushKey = true;
			mPlayerVelocity += 0.0000005f;
		}
	}

	if ((GetAsyncKeyState('A') & 0x8000) && mPlayerPos.z < mTargetPos.z - 20.0f)
	{
		pushKey = true;
		mPlayerYaw -= 0.0001f;

		mPlayerTarget.x = mPlayerPos.x + mCameraRadius * sinf(mCameraPhi) * sinf(mPlayerYaw);
		mPlayerTarget.z = mPlayerPos.z + mCameraRadius * sinf(mCameraPhi) * cosf(mPlayerYaw);
	}
	else if ((GetAsyncKeyState('D') & 0x8000) && mPlayerPos.z < mTargetPos.z - 20.0f)
	{
		pushKey = true;
		mPlayerYaw += 0.0001f;

		mPlayerTarget.x = mPlayerPos.x + mCameraRadius * sinf(mCameraPhi) * sinf(mPlayerYaw);
		mPlayerTarget.z = mPlayerPos.z + mCameraRadius * sinf(mCameraPhi) * cosf(mPlayerYaw);
	}
	
	if (GetAsyncKeyState('R') & 0x8000)
	{
		pushKey = true;
		mPlayerPos = { 0.0f, 2.0f, 0.0f };
		mEyePos = { 0.0f, 30.0f, -30.0f };
		mPlayerTarget = { 0.0f, 2.0f, 15.0f };
		mEyeTarget = mPlayerPos;

		mPlayerVelocity = 0.0f;
		mPlayerYaw = 0.0f;

		srand(gt.TotalTime());
		mTargetPos.x = (float)(rand() % 10);
		mTargetPos.x -= 5.0f;
		restart = true;
	}

	if (mPlayerYaw > XM_PI)
		mPlayerYaw -= XM_2PI;
	else if (mPlayerYaw < -XM_PI)
		mPlayerYaw += XM_2PI;

	mPlayer.setPos(mPlayerPos);
	mPlayer.setTarget(mPlayerTarget);
	mPlayer.setYaw(mPlayerYaw);
	mPlayer.setVelocity(mPlayerVelocity);
	if (restart == true)
	{
		UpdateObjectShadows();
	}
	if (pushKey == true)
	{
		UpdatePlayerPosition(gt);
	}
	
}

//-------------------------------------------------------------------------------------------------------------------------------
void rollingTheBall::UpdatePlayerPosition(const GameTimer& gt)
{
	XMFLOAT3 mPlayerTarget = mPlayer.getTarget();
	XMFLOAT3 mPlayerPos = mPlayer.getPos();
	float mPlayerYaw = mPlayer.getYaw();
	float mPlayerRoll= mPlayer.getRoll();
	float mPlayerVelocity = mPlayer.getVelocity();
	static float prePlayerVelocity = mPlayerVelocity;
	float mPlayerRadius = mPlayer.getRadius();

	XMVECTOR EyePos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR PlayerPos = XMVectorSet(mPlayerPos.x, mPlayerPos.y, mPlayerPos.z, 1.0f);
	XMVECTOR PlayerTarget = XMVectorSet(mPlayerTarget.x, mPlayerTarget.y, mPlayerTarget.z, 0.0f);
	XMVECTOR EyePosCalc = XMVectorSet(mEyePosCalc.x, mEyePosCalc.y, mEyePosCalc.z, 1.0f);
	XMVECTOR PlayerDirection = XMVector3Normalize(XMVectorSubtract(PlayerTarget, PlayerPos));
	XMVECTOR PlayerMove;
	XMMATRIX world = XMLoadFloat4x4(&mPlayerRitem->World);

	for (auto& e : mCollisionRitems)
	{
		XMVECTOR ObjPos = XMVectorSet(e->originVector.x, e->originVector.y, e->originVector.z, 1.0f);

		if (MathHelper::getDistance(PlayerPos, ObjPos) <= e->radius + mPlayerRadius)
		{
			XMVECTOR ObjDirection = XMVector3Normalize(XMVectorSubtract(ObjPos, PlayerPos));

			PlayerPos = XMVectorSubtract(PlayerPos, 0.1f * ObjDirection);
			PlayerTarget = XMVectorSubtract(PlayerTarget, 0.1f * ObjDirection);

			mPlayerVelocity *= 0.95f;

			break;
		}
	}

	// Fsor LEFT MOUSE control
	if ((mPlayerVelocity != prePlayerVelocity) || (mPlayerVelocity >= 0.01f) || (mPlayerVelocity <= -0.01f))
	{
		mPlayerRitem->NumFramesDirty = gNumFrameResources;
		PlayerMove = mPlayerVelocity * PlayerDirection;

		PlayerPos = XMVectorAdd(PlayerPos, PlayerMove);
		PlayerTarget = XMVectorAdd(PlayerTarget, PlayerMove);
		XMStoreFloat3(&mEyeTarget, PlayerPos);

		// Roll value according to direction
		if (mPlayerYaw < 0.0f)
		{
			PlayerMove.m128_f32[0] *= -1.0f;
		}
		if (mPlayerYaw < -1.0f * XM_PIDIV2 || mPlayerYaw > XM_PIDIV2)
		{
			PlayerMove.m128_f32[2] *= -1.0f;
		}

		mPlayerRoll += PlayerMove.m128_f32[0] + PlayerMove.m128_f32[2];

		if (mPlayerRoll > XM_PI)
			mPlayerRoll -= XM_2PI;
		else if (mPlayerRoll < -XM_PI)
			mPlayerRoll += XM_2PI;
		world = XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixRotationX(mPlayerRoll) * XMMatrixRotationY(mPlayerYaw) * XMMatrixTranslationFromVector(PlayerPos);

		// Distance between player and target 
		mDistanceToTarget = MathHelper::getDistance(PlayerPos, XMVectorSet(mTargetPos.x, mTargetPos.y, mTargetPos.z, 1.0f));

		// Update Shadow
		UpdatePlayerShadow(gt);

		prePlayerVelocity = mPlayerVelocity;
	}
	
	XMVECTOR EyePosOne = XMVectorSet(sinf(mPlayerYaw + XM_PI), 1.0f, cosf(mPlayerYaw + XM_PI), 1.0f);
	EyePos = XMVectorMultiplyAdd(EyePosCalc, EyePosOne, PlayerPos);

	XMStoreFloat3(&mEyePos, EyePos);
	XMStoreFloat3(&mPlayerPos, PlayerPos);
	XMStoreFloat3(&mPlayerTarget, PlayerTarget);
	XMStoreFloat4x4(&mPlayerRitem->World, world);

	mPlayer.setPos(mPlayerPos);
	mPlayer.setTarget(mPlayerTarget);
	mPlayer.setVelocity(mPlayerVelocity);
	mPlayer.setRoll(mPlayerRoll);
}

void rollingTheBall::UpdatePlayerShadow(const GameTimer& gt)
{
	XMFLOAT3 mPlayerPos = mPlayer.getPos();
	XMMATRIX playerWorld = XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(mPlayerPos.x, mPlayerPos.y, mPlayerPos.z);

	XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	XMStoreFloat4x4(&mShadowedPlayerRitem->World, playerWorld * S * shadowOffsetY);
	mShadowedPlayerRitem->NumFramesDirty = gNumFrameResources;
}

void rollingTheBall::UpdateCamera(const GameTimer& gt)
{
	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorSet(mEyeTarget.x, mEyeTarget.y, mEyeTarget.z, 0.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void rollingTheBall::UpdateObjectCBs(const GameTimer& gt)
{
	XMVECTOR PlayerPos = XMLoadFloat3(&mPlayer.getPos());
	XMVECTOR PlayerTarget = XMLoadFloat3(&mPlayer.getTarget());
	float mYaw = mPlayer.getYaw();

	auto currObjectCB = mCurrFrameResource->ObjectCB.get();

	for (auto& e : mRitems[(int)RenderLayer::Opaque])
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		XMMATRIX world = XMLoadFloat4x4(&e->World);
		XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

		// Target Circle
		if (e->ObjCBIndex >= mTargetIndexOffset)
		{
			world *= XMMatrixTranslation(mTargetPos.x, 0.0f, 0.0f);

			if (e->ObjCBIndex < mTargetIndexEndOffset)
			{
				// Outer Circle -> Inner Circle
				// Distinguish based on 'y' coord 
				if (mDistanceToTarget < mTargetRadius[0])
				{
					e->Mat = mMaterials["bricks3"].get();
				}
				else if (mDistanceToTarget < mTargetRadius[1])
				{
					if (e->World.m[3][1] >= 1.1f)
						e->Mat = mMaterials["bricks3"].get();
					else
						e->Mat = mMaterials["stone0"].get();
				}
				else if (mDistanceToTarget < mTargetRadius[2])
				{
					if (e->World.m[3][1] == 1.2f)
						e->Mat = mMaterials["bricks3"].get();
					else
						e->Mat = mMaterials["stone0"].get();
				}
				else
				{
					e->Mat = mMaterials["stone0"].get();
				}
			}
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
		}

		if (e->NumFramesDirty > 0)
		{
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void rollingTheBall::UpdateObjectShadows()
{
	// for (auto& e : mRitems[(int)RenderLayer::Shadow])
	// 3 means Player, player shadow, grid 
	for (int i = mTargetIndexOffset - 3; i < mTargetIndexEndOffset - 3; ++i)
	{
		auto& e = mRitems[(int)RenderLayer::Shadow][i];
		
		// Load the object world
		auto& o = mRitems[(int)RenderLayer::Opaque][i + 1];
		XMMATRIX shadowWorld = XMLoadFloat4x4(&o->World);

		shadowWorld *= XMMatrixTranslation(mTargetPos.x, 0.0f, 0.0f);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, shadowWorld * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;
	}
}

void rollingTheBall::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mMainLight.Direction;
	mMainPassCB.Lights[0].Strength = mMainLight.Strength;
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void rollingTheBall::UpdateMaterialCB(const GameTimer & gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

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

//-------------------------------------------------------------------------------------------------------------------------------
void rollingTheBall::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto bricks3Tex = std::make_unique<Texture>();
	bricks3Tex->Name = "bricks3Tex";
	bricks3Tex->Filename = L"../Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricks3Tex->Filename.c_str(),
		bricks3Tex->Resource, bricks3Tex->UploadHeap));

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));


	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->Filename = L"../Textures/tile.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tileTex->Filename.c_str(),
		tileTex->Resource, tileTex->UploadHeap));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[bricks3Tex->Name] = std::move(bricks3Tex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[tileTex->Name] = std::move(tileTex);
}

void rollingTheBall::BuildDescriptorHeaps()
{
	mObjCbvOffset = (UINT)mTextures.size();
	UINT objCount = (UINT)mRitems[(int)RenderLayer::Opaque].size() + (UINT)mRitems[(int)RenderLayer::Shadow].size();
	UINT matCount = (UINT)mMaterials.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	// +matCount for the Materials for each frame resources.
	UINT numDescriptors = mObjCbvOffset + (objCount + matCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mMatCbvOffset = objCount * gNumFrameResources + mObjCbvOffset;
	mPassCbvOffset = mMatCbvOffset + matCount * gNumFrameResources;
	// mPassCbvOffset + (passSize)
	// passSize = 1 * gNumFrameResources

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)));
}

void rollingTheBall::BuildTextureBufferViews()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto bricks3Tex = mTextures["bricks3Tex"]->Resource;
	auto stoneTex = mTextures["stoneTex"]->Resource;
	auto tileTex = mTextures["tileTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = bricks3Tex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = bricks3Tex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(bricks3Tex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);
}

void rollingTheBall::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT objCount = (UINT)mRitems[(int)RenderLayer::Opaque].size() + (UINT)mRitems[(int)RenderLayer::Shadow].size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mObjCbvOffset + frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT materialCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT materialCount = (UINT)mMaterials.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto materialCB = mFrameResources[frameIndex]->MaterialCB->Resource();
		for (UINT i = 0; i < materialCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = materialCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * materialCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = mMatCbvOffset + frameIndex * materialCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = materialCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void rollingTheBall::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable[3];

	cbvTable[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	cbvTable[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	cbvTable[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	// Objects, Materials, Passes
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Create root CBVs.

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable[0]);
	slotRootParameter[2].InitAsDescriptorTable(1, &cbvTable[1]);
	slotRootParameter[3].InitAsDescriptorTable(1, &cbvTable[2]);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void rollingTheBall::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void rollingTheBall::BuildShapeGeometry()
{
	mMainLight.Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainLight.Strength = { 0.6f, 0.6f, 0.6f };

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void rollingTheBall::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto bricks3 = std::make_unique<Material>();
	bricks3->Name = "bricks3";
	bricks3->MatCBIndex = 1;
	bricks3->DiffuseSrvHeapIndex = 1;
	bricks3->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks3->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks3->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 2;
	stone0->DiffuseSrvHeapIndex = 2;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 3;
	tile0->DiffuseSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.2f;

	auto grass0 = std::make_unique<Material>();
	grass0->Name = "grass0";
	grass0->MatCBIndex = 4;
	grass0->DiffuseSrvHeapIndex = 4;
	grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass0->FresnelR0 = XMFLOAT3(0.05f, 0.02f, 0.02f);
	grass0->Roughness = 0.1f;

	auto shadow0 = std::make_unique<Material>();
	shadow0->Name = "shadow0";
	shadow0->MatCBIndex = 5;
	shadow0->DiffuseSrvHeapIndex = 4;
	shadow0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadow0->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadow0->Roughness = 0.0f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["bricks3"] = std::move(bricks3);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["grass0"] = std::move(grass0);
	mMaterials["shadow0"] = std::move(shadow0);
}

void rollingTheBall::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	//opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// PSO for Shadow
	D3D12_RENDER_TARGET_BLEND_DESC shadowBlendDesc;
	shadowBlendDesc.BlendEnable = true;
	shadowBlendDesc.LogicOpEnable = false;
	shadowBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	shadowBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	shadowBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	shadowBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	shadowBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	shadowBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	shadowBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	shadowBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = opaquePsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	shadowPsoDesc.BlendState.RenderTarget[0] = shadowBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void rollingTheBall::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void rollingTheBall::BuildRenderItems()
{
	UINT objCBIndex = 2;

	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(2.0f, 1.0f, 10.0f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 80.0f, 1.0f));
	gridRitem->ObjCBIndex = objCBIndex++;
	gridRitem->Mat = mMaterials["grass0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	auto lineRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&lineRitem->World, XMMatrixScaling(20.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, mTargetPos.z - 20.0f));
	XMStoreFloat4x4(&lineRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	lineRitem->ObjCBIndex = objCBIndex++;
	lineRitem->Mat = mMaterials["stone0"].get();
	lineRitem->Geo = mGeometries["shapeGeo"].get();
	lineRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	lineRitem->IndexCount = lineRitem->Geo->DrawArgs["box"].IndexCount;
	lineRitem->StartIndexLocation = lineRitem->Geo->DrawArgs["box"].StartIndexLocation;
	lineRitem->BaseVertexLocation = lineRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(lineRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	for (int i = 0; i < 15; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		auto leftColSphere = std::make_unique<CollisionSphere>();
		auto rightColSphere = std::make_unique<CollisionSphere>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-15.0f, 1.5f, 20.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+15.0f, 1.5f, 20.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-15.0f, 3.5f, 20.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+15.0f, 3.5f, 20.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		leftColSphere->radius = 1.5f;
		leftColSphere->originVector = { -15.0f, 2.0f, 20.0f + i * 5.0f };

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		rightColSphere->radius = 1.5f;
		rightColSphere->originVector = { +15.0f, 2.0f, 20.0f + i * 5.0f };

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));

		mCollisionRitems.push_back(std::move(leftColSphere));
		mCollisionRitems.push_back(std::move(rightColSphere));
	}

	for (int i = 0; i < 7; ++i)
	{
		auto backSphereRItem = std::make_unique<RenderItem>();
		auto backCylRItem = std::make_unique<RenderItem>();

		auto ColSphere = std::make_unique<CollisionSphere>();

		XMMATRIX backSphereWorld = XMMatrixTranslation(-15.0f + i * 5.0f, 3.5f, 95.0f);
		XMMATRIX backCylWorld = XMMatrixTranslation(-15.0f + i * 5.0f, 1.5f, 95.0f);

		XMStoreFloat4x4(&backCylRItem->World, backCylWorld);
		XMStoreFloat4x4(&backCylRItem->TexTransform, brickTexTransform);
		backCylRItem->ObjCBIndex = objCBIndex++;
		backCylRItem->Geo = mGeometries["shapeGeo"].get();
		backCylRItem->Mat = mMaterials["bricks0"].get();
		backCylRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backCylRItem->IndexCount = backCylRItem->Geo->DrawArgs["cylinder"].IndexCount;
		backCylRItem->StartIndexLocation = backCylRItem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		backCylRItem->BaseVertexLocation = backCylRItem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&backSphereRItem->World, backSphereWorld);
		backSphereRItem->TexTransform = MathHelper::Identity4x4();
		backSphereRItem->ObjCBIndex = objCBIndex++;
		backSphereRItem->Geo = mGeometries["shapeGeo"].get();
		backSphereRItem->Mat = mMaterials["stone0"].get();
		backSphereRItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		backSphereRItem->IndexCount = backSphereRItem->Geo->DrawArgs["sphere"].IndexCount;
		backSphereRItem->StartIndexLocation = backSphereRItem->Geo->DrawArgs["sphere"].StartIndexLocation;
		backSphereRItem->BaseVertexLocation = backSphereRItem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		ColSphere->radius = 1.5f;
		ColSphere->originVector = { -15.0f + i * 5.0f, 2.0f, 95.0f };

		mAllRitems.push_back(std::move(backCylRItem));
		mAllRitems.push_back(std::move(backSphereRItem));

		mCollisionRitems.push_back(std::move(ColSphere));
	}

	mTargetIndexOffset = objCBIndex;
	for (int i = 0; i < 12; ++i)
	{
		auto circle1Item = std::make_unique<RenderItem>();
		auto circle2Item = std::make_unique<RenderItem>();
		auto circle3Item = std::make_unique<RenderItem>();

		float theta = XM_2PI / 12 * i;
		XMMATRIX circle1 = XMMatrixTranslation(mTargetPos.x + mTargetRadius[0] * cosf(theta), 1.0f, mTargetPos.z + mTargetRadius[0] * sinf(theta));
		XMMATRIX circle2 = XMMatrixTranslation(mTargetPos.x + mTargetRadius[1] * cosf(theta), 1.1f, mTargetPos.z + mTargetRadius[1] * sinf(theta));
		XMMATRIX circle3 = XMMatrixTranslation(mTargetPos.x + mTargetRadius[2] * cosf(theta), 1.2f, mTargetPos.z + mTargetRadius[2] * sinf(theta));

		XMStoreFloat4x4(&circle1Item->World, circle1);
		circle1Item->TexTransform = MathHelper::Identity4x4();
		circle1Item->ObjCBIndex = objCBIndex++;
		circle1Item->Geo = mGeometries["shapeGeo"].get();
		circle1Item->Mat = mMaterials["stone0"].get();
		circle1Item->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		circle1Item->IndexCount = circle1Item->Geo->DrawArgs["sphere"].IndexCount;
		circle1Item->StartIndexLocation = circle1Item->Geo->DrawArgs["sphere"].StartIndexLocation;
		circle1Item->BaseVertexLocation = circle1Item->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&circle2Item->World, circle2);
		circle2Item->TexTransform = MathHelper::Identity4x4();
		circle2Item->ObjCBIndex = objCBIndex++;
		circle2Item->Geo = mGeometries["shapeGeo"].get();
		circle2Item->Mat = mMaterials["stone0"].get();
		circle2Item->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		circle2Item->IndexCount = circle2Item->Geo->DrawArgs["sphere"].IndexCount;
		circle2Item->StartIndexLocation = circle2Item->Geo->DrawArgs["sphere"].StartIndexLocation;
		circle2Item->BaseVertexLocation = circle2Item->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&circle3Item->World, circle3);
		circle3Item->TexTransform = MathHelper::Identity4x4();
		circle3Item->ObjCBIndex = objCBIndex++;
		circle3Item->Geo = mGeometries["shapeGeo"].get();
		circle3Item->Mat = mMaterials["stone0"].get();
		circle3Item->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		circle3Item->IndexCount = circle1Item->Geo->DrawArgs["sphere"].IndexCount;
		circle3Item->StartIndexLocation = circle1Item->Geo->DrawArgs["sphere"].StartIndexLocation;
		circle3Item->BaseVertexLocation = circle1Item->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRitems.push_back(std::move(circle1Item));
		mAllRitems.push_back(std::move(circle2Item));
		mAllRitems.push_back(std::move(circle3Item));
	}
	mTargetIndexEndOffset = objCBIndex;
	// All the render items are opaque.
	// playerRitem / shadowedPlayerRitem (2) SUBTRACT 
	UINT endIndex = objCBIndex - 2;
	//for (auto& e : mAllRitems)
	for(int i = 0; i < endIndex; ++i)
	{
		auto& e = mAllRitems.at(i);
		mRitems[(int)RenderLayer::Opaque].push_back(e.get());

		// Skip the Grid shadow
		if (i == 0) continue;

		auto shadowedObjectRitem = std::make_unique<RenderItem>();
		*shadowedObjectRitem = *e;
		shadowedObjectRitem->ObjCBIndex = objCBIndex++;
		shadowedObjectRitem->Mat = mMaterials["shadow0"].get();
		shadowedObjectRitem->NumFramesDirty = gNumFrameResources;
		mRitems[(int)RenderLayer::Shadow].push_back(shadowedObjectRitem.get());
		mAllRitems.push_back(std::move(shadowedObjectRitem));
	}
	
	//
	// Player
	auto playerRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&playerRitem->World, XMMatrixScaling(4.0f, 4.0f, 4.0f)*XMMatrixTranslation(0.0f, 2.0f, 0.0f));
	XMStoreFloat4x4(&playerRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	playerRitem->ObjCBIndex = 0;
	playerRitem->Mat = mMaterials["grass0"].get();
	playerRitem->Geo = mGeometries["shapeGeo"].get();
	playerRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	playerRitem->IndexCount = playerRitem->Geo->DrawArgs["sphere"].IndexCount;
	playerRitem->StartIndexLocation = playerRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	playerRitem->BaseVertexLocation = playerRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mPlayerRitem = playerRitem.get();
	mPlayerRitem->NumFramesDirty = gNumFrameResources;
	mRitems[(int)RenderLayer::Opaque].push_back(playerRitem.get());

	// Player's shadow Render item
	auto shadowedPlayerRitem = std::make_unique<RenderItem>();
	*shadowedPlayerRitem = *playerRitem;
	shadowedPlayerRitem->ObjCBIndex = 1;
	shadowedPlayerRitem->Mat = mMaterials["shadow0"].get();
	mShadowedPlayerRitem = shadowedPlayerRitem.get();
	mShadowedPlayerRitem->NumFramesDirty = gNumFrameResources;
	mRitems[(int)RenderLayer::Shadow].push_back(shadowedPlayerRitem.get());

	mAllRitems.push_back(std::move(playerRitem));
	mAllRitems.push_back(std::move(shadowedPlayerRitem));
}

void rollingTheBall::BuildObjectShadows()
{
	for (auto& e : mRitems[(int)RenderLayer::Shadow])
	{
		XMMATRIX world = XMLoadFloat4x4(&e->World);

		XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR toMainLight = -XMLoadFloat3(&mMainLight.Direction);
		XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
		XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
		XMStoreFloat4x4(&e->World, world * S * shadowOffsetY);
		e->NumFramesDirty = gNumFrameResources;
	}
}
//-------------------------------------------------------------------------------------------------------------------------------
void rollingTheBall::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	/*auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();*/

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		// ri->ObjCBIndex = object number 0, 1, 2, --- , n
		// frame size  +  ��ü index

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		UINT cbvIndex = mObjCbvOffset + mCurrFrameResourceIndex * ((UINT)mRitems[(int)RenderLayer::Opaque].size() + (UINT)mRitems[(int)RenderLayer::Shadow].size()) + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvDescriptorSize);

		UINT matCbvIndex = mMatCbvOffset + mCurrFrameResourceIndex * (UINT)mMaterials.size() + ri->Mat->MatCBIndex;
		auto matCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		matCbvHandle.Offset(matCbvIndex, mCbvSrvDescriptorSize);

		//if(ritems.size() != 1)
			cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);
		cmdList->SetGraphicsRootDescriptorTable(2, matCbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

//-------------------------------------------------------------------------------------------------------------------------------
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> rollingTheBall::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
