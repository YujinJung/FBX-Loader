//***************************************************************************************
// ShapesApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
// 
// FBXLoaderApp.cpp by Yujin Jung
// 
// Hold down '2' key to view FBX model in wireframe mode
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include <random>
#include <chrono>
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "RenderItem.h"
#include "SkinnedData.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "FBXGenerator.h"

#include "Player.h"
#include "Textures.h"
#include "Materials.h"
#include "TextureLoader.h"
#include "Utility.h"

#include "Portfolio_Game.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		PortfolioGameApp theApp(hInstance);
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

PortfolioGameApp::PortfolioGameApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
	for (int i = 0; i < (int)eUIList::Count; ++i)
	{
		HitTime[i] = 0.0f;
		DelayTime[i] = 0.0f;
	}
}

PortfolioGameApp::~PortfolioGameApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

///
bool PortfolioGameApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// TODO : DELETE
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	BuildShapeGeometry();
	BuildMaterials();
	BuildFbxGeometry();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildRenderItems();
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

void PortfolioGameApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	//XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	mPlayer.mCamera.SetProj(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void PortfolioGameApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

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
	UpdateCharacterCBs(gt);
	UpdateMainPassCB(gt);
	UpdateObjectShadows(gt);
	UpdateMaterialCB(gt);
}

void PortfolioGameApp::Draw(const GameTimer& gt)
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
	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(5, passCbvHandle);
	// Object
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Opaque]);

	// SkyTex
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	//skyTexDescriptor.Offset(mTexDiffuseOffset, mCbvSrvDescriptorSize);
	skyTexDescriptor.Offset(mTexSkyCubeOffset, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(2, skyTexDescriptor);

	// Sky 
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitems[(int)RenderLayer::Sky]);



	// Character
	if (!mFbxWireframe)
	{
		mCommandList->SetPipelineState(mPSOs["Player"].Get());
	}
	else
	{
		mCommandList->SetPipelineState(mPSOs["Player_wireframe"].Get());
	}
	DrawRenderItems(mCommandList.Get(), mPlayer.GetRenderItem(RenderLayer::Character));


	// Shadow
	mCommandList->OMSetStencilRef(0);
	mCommandList->SetPipelineState(mPSOs["Player_shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mPlayer.GetRenderItem(RenderLayer::Shadow));


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


void PortfolioGameApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PortfolioGameApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void PortfolioGameApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
	float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

	if ((btnState & MK_LBUTTON) != 0)
	{
		mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, dx);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Rotate Camera only
		mPlayer.mCamera.AddPitch(dy);
		mPlayer.mCamera.AddYaw(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;

	mPlayer.UpdateTransformationMatrix();
}

void PortfolioGameApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	bool isForward = true;
	bool isBackward = true;

	// Architecture
	// Collision Chk
	XMVECTOR playerLook = mPlayer.GetCharacterInfo().mMovement.GetPlayerLook();
	XMVECTOR playerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();

	// Input
	if (GetAsyncKeyState('7') & 0x8000)
		mIsWireframe = true;
	else if (GetAsyncKeyState('8') & 0x8000)
		mFbxWireframe = true;
	else if (GetAsyncKeyState('9') & 0x8000)
		mCameraDetach = true;
	else if (GetAsyncKeyState('0') & 0x8000)
		mCameraDetach = false;
	else
	{
		mIsWireframe = false;
		mFbxWireframe = false;
	}

	if (GetAsyncKeyState('W') & 0x8000)
	{
		if (!mCameraDetach)
		{
			if (GetAsyncKeyState(VK_LSHIFT))
			{
				mPlayer.SetClipName("run");
			}
			else
			{
				mPlayer.SetClipName("playerWalking");
			}

			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(10.0f * dt);
		}
	}
	else if (GetAsyncKeyState('S') & 0x8000)
	{
		if (!mCameraDetach)
		{
			mPlayer.SetClipName("WalkingBackward");

			if (mPlayer.GetCurrentClip() == eClipList::Walking)
			{
				if (mPlayer.isClipEnd())
					mPlayer.SetClipTime(0.0f);
			}
		}
		else
		{
			mPlayer.mCamera.Walk(-10.0f * dt);
		}
	}
	else if (GetAsyncKeyState('1') & 0x8000)
	{
		// Kick Delay, 5 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Punch] > 3.0f)
		{
			mPlayer.SetClipTime(0.0f);
			HitTime[(int)eUIList::I_Punch] = gt.TotalTime();
		}
	}
	else if (GetAsyncKeyState('2') & 0x8000)
	{
		// Hook Delay, 3 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Kick] > 5.0f)
		{
			mPlayer.SetClipTime(0.0f);
			HitTime[(int)eUIList::I_Kick] = gt.TotalTime();
		}
	}
	else if (GetAsyncKeyState('3') & 0x8000)
	{
		// Hook Delay, 3 seconds
		if (gt.TotalTime() - HitTime[(int)eUIList::I_Kick2] > 10.0f)
		{
			mPlayer.SetClipTime(0.0f);
			HitTime[(int)eUIList::I_Kick2] = gt.TotalTime();
		}
	}
	else
	{
		if (mPlayer.isClipEnd())
			mPlayer.SetClipName("Idle");
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, -1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(-10.0f * dt);
	}
	else if (GetAsyncKeyState('D') & 0x8000)
	{
		if (!mCameraDetach)
			mPlayer.UpdatePlayerPosition(ePlayerMoveList::AddYaw, 1.0f * dt);
		else
			mPlayer.mCamera.WalkSideway(10.0f * dt);
	}

	mPlayer.UpdateTransformationMatrix();

	// Update Remaining Time
	float totalTime = gt.TotalTime();
	for (int i = (int)eUIList::I_Punch; i < (int)eUIList::Count; ++i)
	{
		// 10.0 is max delay time
		float Delay = totalTime - HitTime[i];
		if (Delay > 10.0f)
			continue;

		DelayTime[i] = totalTime - HitTime[i];
	}
}


void PortfolioGameApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	auto playerBounds = mPlayer.GetCharacterInfo().mBoundingBox;
	XMVECTOR playerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();
	static int i = 0;

	for (auto& e : mAllRitems)
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

void PortfolioGameApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mPlayer.mCamera.GetView();
	XMMATRIX proj = mPlayer.mCamera.GetProj();

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
	mMainPassCB.EyePosW = mPlayer.mCamera.GetEyePosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.FogColor = { 0.8f, 0.8f, 0.8f, 0.5f };
	mMainPassCB.FogRange = 0.0f;
	mMainPassCB.FogStart = 1000.0f;

	mMainPassCB.Lights[0].Direction = mMainLight.Direction;
	mMainPassCB.Lights[0].Strength = mMainLight.Strength;
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void PortfolioGameApp::UpdateMaterialCB(const GameTimer & gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	mMaterials.UpdateMaterialCB(currMaterialCB);
}

void PortfolioGameApp::UpdateCharacterCBs(const GameTimer & gt)
{
	static bool playerDeathCamFinished = false;
	XMVECTOR PlayerPos = mPlayer.GetCharacterInfo().mMovement.GetPlayerPosition();

	if (mPlayer.GetHealth() <= 0 && !playerDeathCamFinished)
	{
		mCameraDetach = true;

		XMVECTOR CameraPos = mPlayer.mCamera.GetEyePosition();

		if (MathHelper::getDistance(PlayerPos, CameraPos) < 100.0f)
		{
			mPlayer.mCamera.Walk(-0.25f);
			mPlayer.mCamera.UpdateViewMatrix();
		}
		else
		{
			mPlayer.mUI.SetGameover();
			playerDeathCamFinished = true;
		}
	}

	mPlayer.UpdateCharacterCBs(mCurrFrameResource, mMainLight, DelayTime, gt);
}

void PortfolioGameApp::UpdateObjectShadows(const GameTimer& gt)
{
	//auto currSkinnedCB = mCurrFrameResource->PlayerCB.get();
	//mCharacter.UpdateCharacterShadows(mMainLight);
}


///
void PortfolioGameApp::BuildDescriptorHeaps()
{
	UINT texCount = mTexDiffuse.GetSize();
	UINT texNormalCount = mTexDiffuse.GetSize();
	UINT texSkyCount = mTexSkyCube.GetSize();
	UINT objCount = (UINT)mAllRitems.size();
	UINT passCount = 1;
	UINT matCount = mMaterials.GetSize();
	UINT chaCount = mPlayer.GetAllRitemsSize();
	UINT uiCount = mPlayer.mUI.GetSize();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	// +matCount for the Materials for each frame resources.
	UINT numDescriptors = texCount + texNormalCount + texSkyCount + (objCount + passCount + matCount + chaCount + uiCount) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	// o m p
	mTexNormalOffset = texCount;
	mTexSkyCubeOffset = texCount + texNormalCount;
	mObjCbvOffset = mTexSkyCubeOffset + texSkyCount;
	mMatCbvOffset = objCount * gNumFrameResources + mObjCbvOffset;
	mPassCbvOffset = matCount * gNumFrameResources + mMatCbvOffset;
	mChaCbvOffset = passCount * gNumFrameResources + mPassCbvOffset;
	mUICbvOffset = chaCount * gNumFrameResources + mChaCbvOffset;

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

void PortfolioGameApp::BuildTextureBufferViews()
{
	mTexDiffuse.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	mTexDiffuse.BuildConstantBufferViews(Textures::Type::TWO_DIMENTION);
	mTexDiffuse.End();

	mTexNormal.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	mTexNormal.BuildConstantBufferViews(Textures::Type::TWO_DIMENTION, mTexNormalOffset);
	mTexNormal.End();
	
	mTexSkyCube.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());
	mTexSkyCube.BuildConstantBufferViews(Textures::Type::CUBE, mTexSkyCubeOffset);
	mTexSkyCube.End();
}

void PortfolioGameApp::BuildConstantBufferViews(int cbvOffset, UINT itemSize, UINT cbSize, eUploadBufferIndex e)
{
	UINT CBByteSize = d3dUtil::CalcConstantBufferByteSize(cbSize);

	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto cbResource = FrameResource::GetResourceByIndex(mFrameResources[frameIndex].get(), e);

		for (UINT i = 0; i < itemSize; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbResource->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * CBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = cbvOffset + frameIndex * itemSize + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = CBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
}

void PortfolioGameApp::BuildConstantBufferViews()
{
	// Object 
	BuildConstantBufferViews(mObjCbvOffset, mAllRitems.size(), sizeof(ObjectConstants), eUploadBufferIndex::ObjectCB);

	// Material 
	BuildConstantBufferViews(mMatCbvOffset, mMaterials.GetSize(), sizeof(MaterialConstants), eUploadBufferIndex::MaterialCB);

	// Pass - 4
	// Last three descriptors are the pass CBVs for each frame resource.
	BuildConstantBufferViews(mPassCbvOffset, 1, sizeof(PassConstants), eUploadBufferIndex::PassCB);

	// Character
	BuildConstantBufferViews(mChaCbvOffset, mPlayer.GetAllRitemsSize(), sizeof(CharacterConstants), eUploadBufferIndex::PlayerCB);

	// UI
	BuildConstantBufferViews(mUICbvOffset, mPlayer.mUI.GetSize(), sizeof(UIConstants), eUploadBufferIndex::UICB);

}

void PortfolioGameApp::BuildRootSignature()
{
	const int texTableNumber = 3;
	const int cbvTableNumber = 6;
	const int tableNumber = texTableNumber + cbvTableNumber;

	CD3DX12_DESCRIPTOR_RANGE cbvTable[cbvTableNumber];
	CD3DX12_DESCRIPTOR_RANGE texTable[texTableNumber];

	for (int i = 0; i < texTableNumber; ++i)
		texTable[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, i);
	for (int i = 0; i < cbvTableNumber; ++i)
		cbvTable[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, i);

	// Root parameter can be a table, root descriptor or root constants.
	// Objects, Materials, Passes
	CD3DX12_ROOT_PARAMETER slotRootParameter[tableNumber];

	// Create root CBVs.
	for (int i = 0; i < texTableNumber; ++i)
		slotRootParameter[i].InitAsDescriptorTable(1, &texTable[i], D3D12_SHADER_VISIBILITY_PIXEL);
	for (int i = texTableNumber; i < tableNumber; ++i)
		slotRootParameter[i].InitAsDescriptorTable(1, &cbvTable[i - texTableNumber]);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(tableNumber, slotRootParameter,
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

void PortfolioGameApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(),
			1, (UINT)mAllRitems.size(),
			mMaterials.GetSize(),
			mPlayer.GetAllRitemsSize(),
			mPlayer.mUI.GetSize()));
	}
}

void PortfolioGameApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};
	const D3D_SHADER_MACRO playerUIDefines[] =
	{
		"PLAYER", "1",
		NULL, NULL
	};
	

	mShaders["standardVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["uiVS"] = d3dUtil::CompileShader(L"..\\Shaders\\UI.hlsl", playerUIDefines, "VS", "vs_5_1");
	mShaders["skyVS"] = d3dUtil::CompileShader(L"..\\Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");

	mShaders["opaquePS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skinnedPS"] = d3dUtil::CompileShader(L"..\\Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_5_1");
	mShaders["uiPS"] = d3dUtil::CompileShader(L"..\\Shaders\\UI.hlsl", playerUIDefines, "PS", "ps_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"..\\Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mUIInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "ROW", 0, DXGI_FORMAT_R32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void PortfolioGameApp::BuildPSOs()
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

	// PSO for Player 
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerPsoDesc = opaquePsoDesc;
	PlayerPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	PlayerPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	PlayerPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
		mShaders["skinnedPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerPsoDesc, IID_PPV_ARGS(&mPSOs["Player"])));

	// PSO for ui
	D3D12_GRAPHICS_PIPELINE_STATE_DESC UIPsoDesc = opaquePsoDesc;
	UIPsoDesc.InputLayout = { mUIInputLayout.data(), (UINT)mUIInputLayout.size() };
	UIPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["uiVS"]->GetBufferPointer()),
		mShaders["uiVS"]->GetBufferSize()
	};
	UIPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["uiPS"]->GetBufferPointer()),
		mShaders["uiPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&UIPsoDesc, IID_PPV_ARGS(&mPSOs["UI"])));

	// PSO for skinned wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerWireframePsoDesc = PlayerPsoDesc;
	PlayerWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerWireframePsoDesc, IID_PPV_ARGS(&mPSOs["Player_wireframe"])));

	// PSO for opaque wireframe objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));


	// PSO for sky.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));



	//
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


	// Skinned Shadow
	D3D12_GRAPHICS_PIPELINE_STATE_DESC PlayerShadowPsoDesc = shadowPsoDesc;
	PlayerShadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	PlayerShadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	PlayerShadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedPS"]->GetBufferPointer()),
		mShaders["skinnedPS"]->GetBufferSize()
	};
	PlayerShadowPsoDesc.DepthStencilState = shadowDSS;
	PlayerShadowPsoDesc.BlendState.RenderTarget[0] = shadowBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&PlayerShadowPsoDesc, IID_PPV_ARGS(&mPSOs["Player_shadow"])));
}


///
void PortfolioGameApp::BuildShapeGeometry()
{
	mMainLight.Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainLight.Strength = { 0.6f, 0.6f, 0.6f };

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(1000.0f, 1000.0f, 200, 200);
	GeometryGenerator::MeshData hpBar = geoGen.CreateGrid(20.0f, 20.0f, 20, 20);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT hpBarVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT sphereVertexOffset = hpBarVertexOffset + (UINT)hpBar.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT hpBarIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT sphereIndexOffset = hpBarIndexOffset + (UINT)hpBar.Indices32.size();
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

	SubmeshGeometry hpBarSubmesh;
	hpBarSubmesh.IndexCount = (UINT)hpBar.Indices32.size();
	hpBarSubmesh.StartIndexLocation = hpBarIndexOffset;
	hpBarSubmesh.BaseVertexLocation = hpBarVertexOffset;

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
		hpBar.Vertices.size() +
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

	for (size_t i = 0; i < hpBar.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = hpBar.Vertices[i].Position;
		vertices[k].Normal = hpBar.Vertices[i].Normal;
		vertices[k].TexC = hpBar.Vertices[i].TexC;
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
	indices.insert(indices.end(), std::begin(hpBar.GetIndices16()), std::end(hpBar.GetIndices16()));
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
	geo->DrawArgs["hpBar"] = hpBarSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);


	// UI
	std::vector<UIVertex> outUIVertices;
	std::vector<uint32_t> outUIIndices;

	GeometryGenerator::CreateGrid(
		outUIVertices, outUIIndices,
		10.0f, 10.0f, 10, 10);
	mPlayer.mUI.BuildGeometry(
		md3dDevice.Get(),
		mCommandList.Get(),
		outUIVertices,
		outUIIndices,
		"SkillUI"
	);
}

void PortfolioGameApp::BuildFbxGeometry()
{
	FBXGenerator fbxGen;

	fbxGen.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	fbxGen.LoadFBXPlayer(mPlayer, mTexDiffuse, mTexNormal, mMaterials);

	//LoadFBXArchitecture();
	//fbxGen.LoadFBXArchitecture(mGeometries, mTextures, mMaterials);

	fbxGen.End();
}


void PortfolioGameApp::LoadTextures()
{
	mTexDiffuse.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	std::vector<std::string> texNames;
	std::vector<std::wstring> texPaths;

	texNames.push_back("bricksTex");
	texPaths.push_back(L"../Resource/Textures/bricks.dds");

	texNames.push_back("bricks3Tex");
	texPaths.push_back(L"../Resource/Textures/bricks3.dds");

	texNames.push_back("stoneTex");
	texPaths.push_back(L"../Resource/Textures/stone.dds");

	texNames.push_back("tundraTex");
	texPaths.push_back(L"../Resource/Textures/tundra.jpg");

	texNames.push_back("snowTex");
	texPaths.push_back(L"../Resource/Textures/snow.jpg");

	texNames.push_back("iceTex");
	texPaths.push_back(L"../Resource/Textures/ice.dds");

	texNames.push_back("redTex");
	texPaths.push_back(L"../Resource/Textures/red.png");
	
	mTexDiffuse.SetTexture(texNames, texPaths);

	mTexDiffuse.End();

	mTexNormal.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	for(int i = 0; i < texPaths.size(); ++i)
	{
		std::wstring TextureNormalFileName;
		TextureNormalFileName = texPaths[i].substr(0, texPaths[i].size() - 4);
		TextureNormalFileName.append(L"_normal.jpg");

		struct stat buffer;
		std::string fileCheck;
		fileCheck.assign(TextureNormalFileName.begin(), TextureNormalFileName.end());
		if (stat(fileCheck.c_str(), &buffer) == 0)
		{
			mTexNormal.SetTexture(
				texNames[i],
				TextureNormalFileName);
		}
	}

	mTexNormal.End();

	// Cube Map
	mTexSkyCube.Begin(md3dDevice.Get(), mCommandList.Get(), mCbvHeap.Get());

	mTexSkyCube.SetTexture("skyCubeMap", L"../Resource/Textures/snowcube1024.dds");

	mTexSkyCube.End();
}

void PortfolioGameApp::BuildMaterials()
{
	int MatIndex = mMaterials.GetSize();

	std::vector<std::string> matName;
	std::vector<XMFLOAT4> diffuses;
	std::vector<XMFLOAT3> fresnels;
	std::vector<float> roughnesses;
	std::vector<int> texIndices;
	std::vector<int> texNormalIndices;

	matName.push_back("bricks0");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("bricksTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("bricksTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.02f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("bricks3");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("bricks3Tex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("bricks3Tex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.02f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("stone0");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("stoneTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("stoneTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.05f, 0.05f));
	roughnesses.push_back(0.3f);

	matName.push_back("tundra0");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("tundraTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("tundraTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("snow");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("snowTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("snowTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("ice0");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("iceTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("iceTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("red");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("redTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("Transparency");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("redTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
	fresnels.push_back(XMFLOAT3(0.05f, 0.02f, 0.02f));
	roughnesses.push_back(0.1f);

	matName.push_back("shadow0");
	texIndices.push_back(mTexDiffuse.GetTextureIndex("redTex"));
	texNormalIndices.push_back(mTexNormal.GetTextureIndex("redTex"));
	diffuses.push_back(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f));
	fresnels.push_back(XMFLOAT3(0.001f, 0.001f, 0.001f));
	roughnesses.push_back(0.0f);

	mMaterials.SetMaterial(
		matName, diffuses, fresnels, roughnesses, 
		MatIndex, texIndices,texNormalIndices);

	// CUBE MAP
	mMaterials.SetMaterial(
		"sky", XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f), XMFLOAT3(0.001f, 0.001f, 0.001f), 0.0f, 
		mMaterials.GetSize(), mTexSkyCubeOffset);
}

void PortfolioGameApp::BuildRenderItems()
{
	UINT objCBIndex = -1;

	//BuildLandscapeRitems(objCBIndex);
	auto subRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&subRitem->TexTransform, XMMatrixScaling(50.0f, 50.0f, 1.0f));
	subRitem->ObjCBIndex = ++objCBIndex;
	subRitem->Geo = mGeometries["shapeGeo"].get();
	subRitem->Mat = mMaterials.Get("bricks0");
	subRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	subRitem->IndexCount = subRitem->Geo->DrawArgs["grid"].IndexCount;
	subRitem->StartIndexLocation = subRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	subRitem->BaseVertexLocation = subRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	subRitem->Geo->DrawArgs["grid"].Bounds.Transform(subRitem->Bounds, XMLoadFloat4x4(&subRitem->World));
	mRitems[(int)RenderLayer::Opaque].push_back(subRitem.get());
	mAllRitems.push_back(std::move(subRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(5.0f, 2.0f, 5.0f) * XMMatrixScaling(1.0f, 4.0f, 1.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = ++objCBIndex;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->Mat = mMaterials.Get("bricks0");
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->Geo->DrawArgs["box"].Bounds.Transform(boxRitem->Bounds, XMLoadFloat4x4(&boxRitem->World));
	mRitems[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = ++objCBIndex;
	skyRitem->Mat = mMaterials.Get("sky");
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mRitems[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	// Player
	mPlayer.BuildRenderItem(mMaterials, "playerMat0");
	mPlayer.mUI.BuildRenderItem(mGeometries, mMaterials);

}

///
void PortfolioGameApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		const int texOffset = 3;
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, tex);

		if (ri->Mat->NormalSrvHeapIndex >= 0)
		{
			UINT normalOffset = mTexNormalOffset + ri->Mat->NormalSrvHeapIndex;
			CD3DX12_GPU_DESCRIPTOR_HANDLE normal(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
			normal.Offset(normalOffset, mCbvSrvDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(1, normal);
		}

		if (ri->ObjCBIndex >= 0)
		{
			UINT cbvIndex = mObjCbvOffset + mCurrFrameResourceIndex * (UINT)mAllRitems.size() + ri->ObjCBIndex;
			auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
			cbvHandle.Offset(cbvIndex, mCbvSrvDescriptorSize);
			cmdList->SetGraphicsRootDescriptorTable(texOffset, cbvHandle);
		}

		UINT matCbvIndex = mMatCbvOffset + mCurrFrameResourceIndex * mMaterials.GetSize() + ri->Mat->MatCBIndex;
		auto matCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		matCbvHandle.Offset(matCbvIndex, mCbvSrvDescriptorSize);

		UINT skinnedIndex = mChaCbvOffset + mCurrFrameResourceIndex * mPlayer.GetAllRitemsSize() + ri->PlayerCBIndex;
		auto skinCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		skinCbvHandle.Offset(skinnedIndex, mCbvSrvDescriptorSize);

		UINT uiIndex = mUICbvOffset + mCurrFrameResourceIndex * mPlayer.mUI.GetSize() + ri->ObjCBIndex;
		auto uiCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		uiCbvHandle.Offset(uiIndex, mCbvSrvDescriptorSize);
		
		// 0
		cmdList->SetGraphicsRootDescriptorTable(texOffset + 1, matCbvHandle);
		//cmdList->SetGraphicsRootDescriptorTable(texOffset + 4, uiCbvHandle);

		if (ri->PlayerCBIndex >= 0)
		{
			cmdList->SetGraphicsRootDescriptorTable(texOffset + 3, skinCbvHandle);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> PortfolioGameApp::GetStaticSamplers()
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
