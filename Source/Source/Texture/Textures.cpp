#include "TextureLoader.h"
#include "Textures.h"

Textures::Textures()
	: mInBeginEndPair(false)
{
}

Textures::~Textures()
{
}

UINT Textures::GetSize() const
{
	return (UINT)mTextures.size();
}

int Textures::GetTextureIndex(std::string Name) const
{
	if (Name == "skyCubdeMap") return mTextures.size();

	int result = 0;
	for (auto & e : mOrderTexture)
	{
		if (e->Name == "skyCubeMap")
			continue;
		if (e->Name == Name)
			return result;
		++result;
	}
	return -1;
}

void Textures::SetTexture(
	const std::string& Name,
	const std::wstring& szFileName)
{
	if (!mInBeginEndPair)
		throw std::exception("Begin must be called before Set Texture");

	auto temp = std::make_unique<Texture>();
	temp->Name = Name;
	temp->Filename = szFileName;
	std::string format;
	for (int i = szFileName.size() - 3; i < szFileName.size(); ++i)
		format.push_back(szFileName[i]);
		
	if (format == "dds")
	{
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice,
			mCommandList, temp->Filename.c_str(),
			temp->Resource, temp->UploadHeap));
	}
	else
	{
		ThrowIfFailed(DirectX::CreateImageDataTextureFromFile(mDevice,
			mCommandList, temp->Filename.c_str(),
			temp->Resource, temp->UploadHeap));
	}

	mOrderTexture.push_back(temp.get());
	mTextures[temp->Name] = std::move(temp);
}

void Textures::SetTexture(
	const std::vector<std::string>& Name,
	const std::vector<std::wstring>& szFileName)
{
	if (!mInBeginEndPair)
		throw std::exception("Begin must be called before Set Texture");

	for (int i = 0; i < Name.size(); ++i)
	{
		auto temp = std::make_unique<Texture>();
		temp->Name = Name[i];
		temp->Filename = szFileName[i];
		std::string format;
		for (int j = szFileName[i].size() - 3; j < szFileName[i].size(); ++j)
			format.push_back(szFileName[i][j]);

		if (format == "dds")
		{
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(mDevice,
				mCommandList, temp->Filename.c_str(),
				temp->Resource, temp->UploadHeap));
		}
		else
		{
			ThrowIfFailed(DirectX::CreateImageDataTextureFromFile(mDevice,
				mCommandList, temp->Filename.c_str(),
				temp->Resource, temp->UploadHeap));
		}

		mOrderTexture.push_back(temp.get());
		mTextures[temp->Name] = std::move(temp);
	}
}



void Textures::Begin(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList, ID3D12DescriptorHeap* cbvHeap)
{
	if (mInBeginEndPair)
		throw std::exception("Cannot nest Begin calls on a Texture");

	mDevice = device;
	mCommandList = cmdList;
	mCbvHeap = cbvHeap;
	
	mInBeginEndPair = true;
}

void Textures::End()
{
	if (!mInBeginEndPair)
		throw std::exception("Begin must be called before End");

	mDevice = nullptr;
	mCommandList = nullptr;
	mInBeginEndPair = false;
}

void Textures::BuildCBVTex2D(const int offset)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
	UINT mCbvSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> vTex;

	for (auto& e : mOrderTexture)
		vTex.push_back(e->Resource);

	hDescriptor.Offset(offset, mCbvSrvDescriptorSize);
	for (auto &e : vTex)
	{
		srvDesc.Format = e->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = e->GetDesc().MipLevels;
		mDevice->CreateShaderResourceView(e.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}
}

void Textures::BuildCBVTexCube(const int offset)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
	UINT mCbvSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	auto skyTex = mTextures["skyCubeMap"].get()->Resource;
	hDescriptor = mCbvHeap->GetCPUDescriptorHandleForHeapStart();
	hDescriptor.Offset(offset, mCbvSrvDescriptorSize);

	srvDesc.Format = skyTex->GetDesc().Format;
	srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
	mDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);
}

void Textures::BuildConstantBufferViews(Textures::Type texType, const int offset)
{
	switch (texType)
	{
	case Textures::Type::TWO_DIMENTION:
		BuildCBVTex2D(offset);
		break;
	case Textures::Type::CUBE:
		BuildCBVTexCube(offset);
		break;
	case Textures::Type::Count:
		break;
	default:
		break;
	}
	
}