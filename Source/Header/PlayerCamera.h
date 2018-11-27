#pragma once
#include "Camera.h"

class PlayerCamera : public Camera
{
public:
	PlayerCamera();
	~PlayerCamera();

	void UpdatePosition(DirectX::FXMVECTOR inPlayerPosition, DirectX::FXMVECTOR inPlayerLook, DirectX::FXMVECTOR inPlayerUp, DirectX::GXMVECTOR inPlayerRight);

};

