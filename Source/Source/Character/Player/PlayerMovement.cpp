#include "RenderItem.h"
#include "PlayerMovement.h"

using namespace DirectX;

PlayerMovement::PlayerMovement()
	: mPlayerPosition(0.0f, 0.0f, 0.0f),
		mPlayerLook(0.0f, 0.0f, 1.0f),
		mPlayerUp(0.0f, 1.0f, 0.0f),
		mPlayerRight(1.0f, 0.0f, 0.0f)
{
}

PlayerMovement::PlayerMovement(
	XMFLOAT3 inPlayerPosition,
	XMFLOAT3 inPlayerLook,
	XMFLOAT3 inPlayerUp,
	XMFLOAT3 inPlayerRight)
	: mPlayerPosition(inPlayerPosition),
		mPlayerLook(inPlayerLook),
		mPlayerUp(inPlayerUp),
		mPlayerRight(inPlayerRight)
{

}

PlayerMovement::~PlayerMovement()
{
}

DirectX::XMVECTOR PlayerMovement::GetPlayerPosition() const
{
	return XMLoadFloat3(&mPlayerPosition);
}
DirectX::XMVECTOR PlayerMovement::GetPlayerLook() const
{
	return XMLoadFloat3(&mPlayerLook);
}
DirectX::XMVECTOR PlayerMovement::GetPlayerUp() const
{
	return XMLoadFloat3(&mPlayerUp);
}
DirectX::XMVECTOR PlayerMovement::GetPlayerRight() const
{
	return XMLoadFloat3(&mPlayerRight);
}

void PlayerMovement::AddYaw(float dx)
{
	XMMATRIX R = XMMatrixRotationY(dx);
	XMVECTOR PlayerRight = XMVector3TransformNormal(XMLoadFloat3(&mPlayerRight), R);
	XMVECTOR PlayerUp = XMVector3TransformNormal(XMLoadFloat3(&mPlayerUp), R);
	XMVECTOR PlayerLook = XMVector3TransformNormal(XMLoadFloat3(&mPlayerLook), R);

	XMStoreFloat3(&mPlayerRight, PlayerRight);
	XMStoreFloat3(&mPlayerUp, PlayerUp);
	XMStoreFloat3(&mPlayerLook, PlayerLook);

	XMStoreFloat4x4(&mRotation, XMLoadFloat4x4(&mRotation) * R);

	mTransformDirty = true;
}

void PlayerMovement::AddPitch(float dy)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mPlayerRight), dy);

	XMStoreFloat3(&mPlayerUp, XMVector3TransformNormal(XMLoadFloat3(&mPlayerUp), R));
	XMStoreFloat3(&mPlayerLook, XMVector3TransformNormal(XMLoadFloat3(&mPlayerLook), R));

	mTransformDirty = true;
}

void PlayerMovement::Walk(float inVelocity)
{
	XMVECTOR Velocity = XMVectorSet(inVelocity, inVelocity, inVelocity, inVelocity);
	XMVECTOR Position = XMLoadFloat3(&mPlayerPosition);
	XMVECTOR Look = XMLoadFloat3(&mPlayerLook);
	XMStoreFloat3(&mPlayerPosition, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

void PlayerMovement::SideWalk(float inVelocity)
{
	XMVECTOR Velocity = XMVectorSet(inVelocity, inVelocity, inVelocity, inVelocity);
	XMVECTOR Position = XMLoadFloat3(&mPlayerPosition);
	XMVECTOR Look = XMLoadFloat3(&mPlayerLook);
	XMStoreFloat3(&mPlayerPosition, XMVectorMultiplyAdd(Velocity, Look, Position));

	mTransformDirty = true;
}

bool PlayerMovement::UpdateTransformationMatrix(WorldTransform& outTransform)
{
	if (mTransformDirty)
	{
		XMVECTOR P = XMLoadFloat3(&mPlayerPosition);
		XMVECTOR L = XMLoadFloat3(&mPlayerLook);
		XMVECTOR U = XMLoadFloat3(&mPlayerUp);
		XMVECTOR R = XMLoadFloat3(&mPlayerRight);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));
		R = XMVector3Cross(U, L);

		XMStoreFloat3(&mPlayerRight, R);
		XMStoreFloat3(&mPlayerUp, U);
		XMStoreFloat3(&mPlayerLook, L);

		mTransformDirty = false;

		outTransform.Position = mPlayerPosition;
		outTransform.Rotation = mRotation;

		return true;
	}
	return false;
}
