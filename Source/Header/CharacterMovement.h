#pragma once

#include "d3dUtil.h"

struct WorldTransform
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4X4 Rotation;

	WorldTransform()
		: Position(0.0f, 0.0f, 0.0f),
		Scale(1.0f, 1.0f, 1.0f)
	{
		Rotation = MathHelper::Identity4x4();
	}
};

struct ViewInfo
{
	DirectX::XMFLOAT3 Look;
	DirectX::XMFLOAT3 Up;
	DirectX::XMFLOAT3 Right;

	ViewInfo()
		: Look(0.0f, 0.0f, 1.0f),
		Up(0.0f, 1.0f, 0.0f),
		Right(1.0f, 0.0f, 0.0f)
	{}
};

class ChracterMovement
{
public:
	ChracterMovement();
	~ChracterMovement();
	
public:
	// Get Member variable
	DirectX::XMVECTOR GetPlayerScale() const;
	DirectX::XMVECTOR GetPlayerPosition() const;
	DirectX::XMFLOAT3 GetPlayerPosition3f() const;
	DirectX::XMMATRIX GetPlayerRotation() const;
	DirectX::XMVECTOR GetPlayerUp() const;
	DirectX::XMVECTOR GetPlayerLook() const;
	DirectX::XMVECTOR GetPlayerRight() const;

	ViewInfo GetViewTransformInfo() const;
	WorldTransform GetWorldTransformInfo() const;

	void SetPlayerScale(DirectX::XMVECTOR S);
	void SetPlayerPosition(DirectX::XMVECTOR P);
	void SetPlayerRotation(DirectX::XMMATRIX R);
	void SetPlayerUp(DirectX::XMVECTOR U);
	void SetPlayerLook(DirectX::XMVECTOR L);
	void SetPlayerRight(DirectX::XMVECTOR R);

	void SetViewTransformInfo(const ViewInfo& inWorld);
	void SetWorldTransformInfo(const WorldTransform& inWorld);

public:
	void AddYaw(float dx);
	void AddPitch(float dy);

	void Walk(float velocity);
	void SideWalk(float inVelocity);

	bool UpdateTransformationMatrix();

private:
	ViewInfo mCharacterViewInfo;
	WorldTransform mCharacterWorldInfo;

	bool mTransformDirty = false;
};

