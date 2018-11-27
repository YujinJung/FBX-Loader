#pragma once

#include "FrameResource.h"
#include "RenderItem.h"

class Materials;
class PlayerUI
{
public:
	PlayerUI();
	~PlayerUI();

	virtual UINT GetSize() const;

	const std::vector<RenderItem*> GetRenderItem(eUIList Type);


	void SetPosition(DirectX::FXMVECTOR inPosition);

	void SetDamageScale(float inScale);

	void SetGameover();

	void BuildGeometry(
		ID3D12Device * device, 
		ID3D12GraphicsCommandList * cmdList,
		const std::vector<UIVertex>& inVertices, 
		const std::vector<std::uint32_t>& inIndices, 
		std::string geoName);

	void BuildRenderItem(
		std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>& mGeometries,
		Materials & mMaterials);


	void UpdateUICBs(
		UploadBuffer<UIConstants>* currUICB,
		DirectX::XMMATRIX playerWorld,
		DirectX::XMVECTOR inEyeLeft,
		float* Delay,
		bool mTransformDirty);

private:
	std::unique_ptr<MeshGeometry> mGeometry;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitems[(int)eUIList::Count];

	std::vector<float> skillFullTime;
	WorldTransform mWorldTransform;
};

