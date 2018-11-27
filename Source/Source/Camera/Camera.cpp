#include "Camera.h"

using namespace DirectX;

Camera::Camera()
{
	SetProj(0.25f*MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
	UpdateViewMatrix();
}

Camera::~Camera()
{
}

void Camera::SetProj(float fovY, float aspect, float zn, float zf)
{
	// cache properties
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f*mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f*mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

void Camera::AddYaw(float dx)
{
	XMMATRIX R = XMMatrixRotationY(dx);
	XMVECTOR EyeRight = DirectX::XMVector3TransformNormal(XMLoadFloat3(&mEyeRight), R);
	XMVECTOR EyeUp = DirectX::XMVector3TransformNormal(XMLoadFloat3(&mEyeUp), R);
	XMVECTOR EyeLook = DirectX::XMVector3TransformNormal(XMLoadFloat3(&mEyeLook), R);

	XMStoreFloat3(&mEyeRight, EyeRight);
	XMStoreFloat3(&mEyeUp, EyeUp);
	XMStoreFloat3(&mEyeLook, EyeLook);

	mViewDirty = true;
}
void Camera::AddPitch(float dy)
{
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mEyeRight), dy);

	XMStoreFloat3(&mEyeUp, XMVector3TransformNormal(XMLoadFloat3(&mEyeUp), R));
	XMStoreFloat3(&mEyeLook, XMVector3TransformNormal(XMLoadFloat3(&mEyeLook), R));

	mViewDirty = true;
}
void Camera::Walk(float velocity)
{
	XMVECTOR Velocity	= XMVectorSet(velocity, velocity, velocity, velocity);
	XMVECTOR Position	= XMLoadFloat3(&mEyePosition);
	XMVECTOR Look	= XMLoadFloat3(&mEyeLook);
	XMStoreFloat3(&mEyePosition, XMVectorMultiplyAdd(Velocity, Look, Position));

	mViewDirty = true;
}
void Camera::WalkSideway(float inVelocity)
{
	XMVECTOR Velocity = XMVectorSet(inVelocity, inVelocity, inVelocity, inVelocity);
	XMVECTOR Position = XMLoadFloat3(&mEyePosition);
	XMVECTOR Right = XMLoadFloat3(&mEyeRight);
	XMStoreFloat3(&mEyePosition, XMVectorMultiplyAdd(Velocity, Right, Position));

	mViewDirty = true;
}

XMVECTOR Camera::GetEyePosition() const
{
	return XMLoadFloat3(&mEyePosition);
}
XMFLOAT3 Camera::GetEyePosition3f() const
{
	return mEyePosition;
}
XMVECTOR Camera::GetEyeLook() const
{
	return XMLoadFloat3(&mEyeLook);
}
XMVECTOR Camera::GetEyeUp() const
{
	return XMLoadFloat3(&mEyeUp);
}
XMVECTOR Camera::GetEyeRight() const
{
	return XMLoadFloat3(&mEyeRight);
}
XMMATRIX Camera::GetView() const
{
	return XMLoadFloat4x4(&mView);
}
XMMATRIX Camera::GetProj() const
{
	return XMLoadFloat4x4(&mProj);
}

void Camera::SetEyePosition(DirectX::XMVECTOR inEyePosition)
{
	XMStoreFloat3(&mEyePosition, inEyePosition);
}
void Camera::SetEyeLook(DirectX::XMVECTOR inEyeLook)
{
	XMStoreFloat3(&mEyeLook, inEyeLook);
}
void Camera::SetEyeUp(DirectX::XMVECTOR inEyeUp)
{
	XMStoreFloat3(&mEyeUp, inEyeUp);
}
void Camera::SetEyeRight(DirectX::XMVECTOR inEyeRight)
{
	XMStoreFloat3(&mEyeRight, inEyeRight);
}

void Camera::UpdateViewMatrix()
{
	if (mViewDirty)
	{
		XMVECTOR P = XMLoadFloat3(&mEyePosition);
		XMVECTOR L = XMLoadFloat3(&mEyeLook);
		XMVECTOR U = XMLoadFloat3(&mEyeUp);
		XMVECTOR R = XMLoadFloat3(&mEyeRight);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mEyeRight, R);
		XMStoreFloat3(&mEyeUp, U);
		XMStoreFloat3(&mEyeLook, L);

		mView(0, 0) = mEyeRight.x;
		mView(1, 0) = mEyeRight.y;
		mView(2, 0) = mEyeRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mEyeUp.x;
		mView(1, 1) = mEyeUp.y;
		mView(2, 1) = mEyeUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mEyeLook.x;
		mView(1, 2) = mEyeLook.y;
		mView(2, 2) = mEyeLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
	}
}
