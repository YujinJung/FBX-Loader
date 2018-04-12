#pragma once


class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	HRESULT LoadFBX(std::vector<Vertex>& outVertexVector, std::vector<int32_t>& outIndexVector);

private:

};