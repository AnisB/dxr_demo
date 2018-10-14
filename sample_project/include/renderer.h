#pragma once

// Internal includes
#include "gpu_backend.h"

// External includes
#include <windows.h>
#include <stdint.h>
#include <vector>

namespace dxr_demo
{
	class TRenderer
	{
	public:
		TRenderer(HINSTANCE hInstance, int nCmdShow);
		~TRenderer();

		// Init and destruction
		void init(TGraphicSettings& graphicsSettings);
		void run();
		void destroy();

		// input
		void key_down(int keyID);
		void key_up(int keyID);

		// main loop
		void update();
		void render();

		// Return the current render environement
		RenderEnvironment render_environement();

	private:
		// D3D Data
		RenderEnvironment _renderEnvironement;
		RenderWindow _renderWindow;
		const GPUBackendAPI* _gpuBackendAPI;

		// Input data
		std::vector<char> _inputData;

		// Rendering data
		bool _isRunning;
	};
}