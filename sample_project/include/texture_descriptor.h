#pragma once

// External includes
#include <stdint.h>
#include <vector>

namespace dxr_demo
{
	struct TTextureDescriptor
	{
		uint32_t width;
		uint32_t height;
		std::vector<float> data;
	};
}