// Internal includes
#include "renderer.h"

// Extenral includes
#include <assert.h>

namespace dxr_demo
{
	#define D3D_NUM_KEYS 254

	TRenderer::TRenderer(HINSTANCE hInstance, int nCmdShow)
	: _renderEnvironement(0)
	, _renderWindow(0)
	, _gpuBackendAPI(nullptr)
	, _isRunning(false)
	{

	}

	TRenderer::~TRenderer()
	{

	}

	void TRenderer::init(TGraphicSettings& graphicsSettings)
	{
		graphicsSettings.platformData[1] = (uint64_t)this;

		// Initialize and fetch the API
		initialize_gpu_backend(RenderingBackEnd::D3D12);
		_gpuBackendAPI = &gpu_api();

		// Create the render environement
		_renderEnvironement = _gpuBackendAPI->render_system_api.create_render_environment(graphicsSettings);

		// Fetch the render window
		_renderWindow = _gpuBackendAPI->render_system_api.render_window(_renderEnvironement);

		// Allocate the input buffer
		_inputData.resize(D3D_NUM_KEYS);
	}

	void TRenderer::run()
	{
		// Raise the flag
		_isRunning = true;

		// Display the window
		_gpuBackendAPI->window_api.show(_renderWindow);
		
		// Main rendering loop
		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));

		while (_isRunning)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
					break;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				update();
				render();
			}
		}
	}

	void TRenderer::destroy()
	{
		_gpuBackendAPI->render_system_api.destroy_render_environment(_renderEnvironement);
	}

	void TRenderer::key_down(int keyID)
	{
		assert(keyID < D3D_NUM_KEYS);
		_inputData[keyID] = true;
	}

	void TRenderer::key_up(int keyID)
	{
		assert(keyID < D3D_NUM_KEYS);
		_inputData[keyID] = false;
	}

	RenderEnvironment TRenderer::render_environement()
	{
		return _renderEnvironement;
	}

	void TRenderer::update()
	{
	}

	void TRenderer::render()
	{
		_isRunning &= _gpuBackendAPI->render_system_api.initialize_frame(_renderEnvironement);

		uint64_t currentFrameIndex = _gpuBackendAPI->render_system_api.frame_index(_renderEnvironement);
		if (currentFrameIndex % 2 == 0)
		{
			FLOAT clearColor0[] = { 1.0f, 0.0f, 0.0f, 1.0f };
			_gpuBackendAPI->frame_buffer_api.clear(_gpuBackendAPI->render_system_api.default_frame_buffer(_renderEnvironement), clearColor0);
		}
		else
		{
			FLOAT clearColor1[] = { 0.0f, 1.0f, 0.0f, 1.0f };
			_gpuBackendAPI->frame_buffer_api.clear(_gpuBackendAPI->render_system_api.default_frame_buffer(_renderEnvironement), clearColor1);
		}

		_isRunning &= _gpuBackendAPI->render_system_api.flush_command_list(_renderEnvironement);
		_isRunning &= _gpuBackendAPI->render_system_api.present(_renderEnvironement);
	}
}