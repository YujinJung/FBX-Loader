#include "PlayerCamera.h"

using namespace DirectX;

PlayerCamera::PlayerCamera()
{
}


PlayerCamera::~PlayerCamera()
{
}

void PlayerCamera::UpdatePosition(
	FXMVECTOR P,
	FXMVECTOR L,
	FXMVECTOR U,
	GXMVECTOR R)
{
	const float eyePositionOffsetXZ = 30.0f;
	const float eyePositionOffsetY = 20.0f;
	const float eyeLookOffsetY = 5.0f;

	XMVECTOR mP = XMVectorAdd(XMVectorSubtract(P, eyePositionOffsetXZ * L), eyePositionOffsetY * U);
	XMVECTOR mL = XMVector3Normalize(XMVectorSubtract(XMVectorAdd(P, eyeLookOffsetY * U), mP));
	XMVECTOR mU = XMVector3Normalize(XMVector3Cross(R, mL));

	SetEyePosition(mP);
	SetEyeLook(mL);
	SetEyeUp(mU);
	SetEyeRight(R);

	mViewDirty = true;
}
