#include "RenderItem.h"
#include "CharacterMovement.h"

using namespace DirectX;

ChracterMovement::ChracterMovement()
	: mCharacterViewInfo(), mCharacterWorldInfo()
{
}

ChracterMovement::~ChracterMovement()
{
}

XMVECTOR ChracterMovement::GetPlayerPosition() const
{
	return XMLoadFloat3(&mCharacterWorldInfo.Position);
}
XMFLOAT3 ChracterMovement::GetPlayerPosition3f() const
{
	return mCharacterWorldInfo.Position;
}
XMVECTOR ChracterMovement::GetPlayerScale() const
{
	return XMLoadFloat3(&mCharacterWorldInfo.Scale);
}
XMMATRIX ChracterMovement::GetPlayerRotation() const
{
	return XMLoadFloat4x4(&mCharacterWorldInfo.Rotation);
}
XMVECTOR ChracterMovement::GetPlayerLook() const
{
	return XMLoadFloat3(&mCharacterViewInfo.Look);
}
XMVECTOR ChracterMovement::GetPlayerUp() const
{
	return XMLoadFloat3(&mCharacterViewInfo.Up);
}
XMVECTOR ChracterMovement::GetPlayerRight() const
{
	return XMLoadFloat3(&mCharacterViewInfo.Right);
}

WorldTransform ChracterMovement::GetWorldTransformInfo() const
{
	return mCharacterWorldInfo;
}
ViewInfo ChracterMovement::GetViewTransformInfo() const
{
	return mCharacterViewInfo;
}

void ChracterMovement::SetPlayerPosition(DirectX::XMVECTOR P)
{
	XMStoreFloat3(&mCharacterWorldInfo.Position, P);
	mTransformDirty = true;
}
void ChracterMovement::SetPlayerScale(DirectX::XMVECTOR S)
{
	XMStoreFloat3(&mCharacterWorldInfo.Scale, S);
	mTransformDirty = true;
}
void ChracterMovement::SetPlayerRotation(DirectX::XMMATRIX R)
{
	XMStoreFloat4x4(&mCharacterWorldInfo.Rotation, R);
	mTransformDirty = true;
}
void ChracterMovement::SetPlayerRight(DirectX::XMVECTOR R)
{
	XMStoreFloat3(&mCharacterViewInfo.Right, R);
	mTransformDirty = true;
}
void ChracterMovement::SetPlayerLook(DirectX::XMVECTOR L)
{
	XMStoreFloat3(&mCharacterViewInfo.Look, L);
	mTransformDirty = true;
}
void ChracterMovement::SetPlayerUp(DirectX::XMVECTOR U)
{
	XMStoreFloat3(&mCharacterViewInfo.Up, U);
	mTransformDirty = true;
}

void ChracterMovement::SetWorldTransformInfo(const WorldTransform & inWorld)
{
	mCharacterWorldInfo = inWorld;
	mTransformDirty = true;
}
void ChracterMovement::SetViewTransformInfo(const ViewInfo & inWorld)
{
	mCharacterViewInfo = inWorld;
	mTransformDirty = true;
}

void ChracterMovement::AddYaw(float dx)
{
	XMMATRIX R = XMMatrixRotationY(dx);
	XMVECTOR PlayerRight = XMVector3TransformNormal(GetPlayerRight(), R);
	XMVECTOR PlayerUp = XMVector3TransformNormal(GetPlayerUp(), R);
	XMVECTOR PlayerLook = XMVector3TransformNormal(GetPlayerLook(), R);

	XMStoreFloat3(&mCharacterViewInfo.Right, PlayerRight);
	XMStoreFloat3(&mCharacterViewInfo.Up, PlayerUp);
	XMStoreFloat3(&mCharacterViewInfo.Look, PlayerLook);

	XMStoreFloat4x4(&mCharacterWorldInfo.Rotation, XMLoadFloat4x4(&mCharacterWorldInfo.Rotation) * R);

	mTransformDirty = true;
}

void ChracterMovement::AddPitch(float dy)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mCharacterViewInfo.Right), dy);

	auto& U = mCharacterViewInfo.Up;
	auto& L = mCharacterViewInfo.Look;
	XMStoreFloat3(&U, XMVector3TransformNormal(XMLoadFloat3(&U), R));
	XMStoreFloat3(&L, XMVector3TransformNormal(XMLoadFloat3(&L), R));

	mTransformDirty = true;
}

void ChracterMovement::Walk(float inVelocity)
{
	auto& P = mCharacterWorldInfo.Position;
	auto& L = mCharacterViewInfo.Look;

	XMVECTOR Velocity = XMVectorReplicate(inVelocity);
	XMVECTOR Position = XMLoadFloat3(&P);
	XMVECTOR Look = XMLoadFloat3(&L);
	XMStoreFloat3(&P, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

void ChracterMovement::SideWalk(float inVelocity)
{
	auto& P = mCharacterWorldInfo.Position;
	auto& L = mCharacterViewInfo.Look;

	XMVECTOR Velocity = XMVectorReplicate(inVelocity);
	XMVECTOR Position = XMLoadFloat3(&P);
	XMVECTOR Look = XMLoadFloat3(&L);
	XMStoreFloat3(&P, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

bool ChracterMovement::UpdateTransformationMatrix()
{
	if (mTransformDirty)
	{
		XMVECTOR P = XMLoadFloat3(&mCharacterWorldInfo.Position);
		XMVECTOR L = XMLoadFloat3(&mCharacterViewInfo.Look);
		XMVECTOR U = XMLoadFloat3(&mCharacterViewInfo.Up);
		XMVECTOR R = XMLoadFloat3(&mCharacterViewInfo.Right);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));
		R = XMVector3Cross(U, L);

		XMStoreFloat3(&mCharacterViewInfo.Right, R);
		XMStoreFloat3(&mCharacterViewInfo.Look, L);
		XMStoreFloat3(&mCharacterViewInfo.Up, U);

		mTransformDirty = false;

		return true;
	}
	return false;
}
