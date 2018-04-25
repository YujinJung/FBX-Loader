#pragma once
#include "FrameResource.h"
#include <wincodec.h>
#include <DirectXMath.h>

namespace DirectX
{
	// get the dxgi format equivilent of a wic format
	DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID);

	// get a dxgi compatible wic format from another wic format
	WICPixelFormatGUID GetConvertToWICFormat(WICPixelFormatGUID& wicFormatGUID);

	// get the number of bits per pixel for a dxgi format
	int GetDXGIFormatBitsPerPixel(DXGI_FORMAT& dxgiFormat);

	int LoadImageDataFromFile(BYTE ** imageData, D3D12_RESOURCE_DESC & textureDesc, LPCWSTR filename, int & bytesPerRow);

	HRESULT CreateImageDataTextureFromFile(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, const wchar_t * szFileName, Microsoft::WRL::ComPtr<ID3D12Resource>& texture, Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap);
}