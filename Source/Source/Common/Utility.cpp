#include "Utility.h"

void Utility::printMatrix(const std::wstring& Name, const int& i, const DirectX::XMMATRIX &M)
{
	std::wstring text = Name + std::to_wstring(i) + L"\n";
	::OutputDebugString(text.c_str());

	for (int j = 0; j < 4; ++j)
	{
		for (int k = 0; k < 4; ++k)
		{
			std::wstring text =
				std::to_wstring(M.r[j].m128_f32[k]) + L" ";

			::OutputDebugString(text.c_str());
		}
		std::wstring text = L"\n";
		::OutputDebugString(text.c_str());
	}
}
