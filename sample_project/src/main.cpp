// Internal includes
#include "renderer.h"
#include "gpu_backend.h"

// External includes
#include "windows.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	// Create the graphics settings
	dxr_demo::TGraphicSettings graphicsSettings;
	graphicsSettings.width = 1280;
	graphicsSettings.height = 720;
	graphicsSettings.fullscreen = false;
	graphicsSettings.window_name = "DXR Demo";
	graphicsSettings.platformData[0] = (uint64_t)hInstance;
	graphicsSettings.platformData[1] = 666;

	// Create the renderer and run it
	dxr_demo::TRenderer renderer(hInstance, nCmdShow);
	renderer.init(graphicsSettings);
	renderer.run();
	renderer.destroy();

	return 0;
}