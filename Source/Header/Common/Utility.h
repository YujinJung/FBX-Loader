#pragma once

#include "../Common/d3dUtil.h"

#define PRINT_DEBUG(String) ::OutputDebugString(String.c_str())


class Utility {

	void printMatrix(const std::wstring& Name, const int& i, const DirectX::XMMATRIX &M);
};
