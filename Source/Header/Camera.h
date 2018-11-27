#pragma once

#include "../Common/d3dUtil.h"

class Camera
{
public:
	Camera();
	~Camera();

	void SetProj(float fovY, float aspect, float zn, float zf);

	void AddYaw(float dx);
	void AddPitch(float dy);

	void Walk(float velocity);
	void WalkSideway(float velocity);

	DirectX::XMVECTOR GetEyePosition() const;
	DirectX::XMFLOAT3 GetEyePosition3f() const;
	DirectX::XMVECTOR GetEyeLook() const;
	DirectX::XMVECTOR GetEyeUp() const;
	DirectX::XMVECTOR GetEyeRight() const;
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

	void SetEyePosition(DirectX::XMVECTOR inEyePosition);
	void SetEyeLook(DirectX::XMVECTOR inEyeLook);
	void SetEyeUp(DirectX::XMVECTOR inEyeUp);
	void SetEyeRight(DirectX::XMVECTOR inEyeRight);

	void UpdateViewMatrix();

	bool mViewDirty = true;

private:
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	/*
	* EyePos - EyeTarget
	* a d - playerTarget Move(Yaw)
	* w s - playerTarget, EyePos Move // EyeDirection based on playerTarget
	* mouse click - EyeTarget Move
	*/
	// Character View
	DirectX::XMFLOAT3 mEyePosition = { -10.0f, 3.0f, 12.0f };
	DirectX::XMFLOAT3 mEyeLook = { 0.5f, 0.0f, -0.5f };
	DirectX::XMFLOAT3 mEyeUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mEyeRight = { -0.5f, 0.0f, -0.5f };

	// Cache frustum properties.
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;
};