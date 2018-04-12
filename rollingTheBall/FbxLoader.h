#pragma once


class FbxLoader
{
public:
	FbxLoader();
	~FbxLoader();

	struct FbxVertex
	{
		float pos[3];
	};

	HRESULT LoadFBX(std::vector<FbxVertex>& pOutVertexVector);

private:

};