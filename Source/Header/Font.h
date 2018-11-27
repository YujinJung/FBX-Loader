#pragma once

#include <GeometryGenerator.h>

class Font
{
public:
	Font();
	~Font();

	GeometryGenerator::MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);
};