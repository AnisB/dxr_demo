// Internal includes
#include "gpu_backend.h"
#include "d3d12_backend.h"

// External includes
#include <d3d12.h>

namespace dxr_demo
{
	// Variable that holds the gpu backend
	GPUBackendAPI gpuBackendAPI = { nullptr };

	void initialize_gpu_backend(RenderingBackEnd::Type backend_type)
	{
		switch (backend_type)
		{
		case RenderingBackEnd::D3D12:
		{
			// Render system API
			gpuBackendAPI.render_system_api.init_render_system = d3d12::render_system::init_render_system;
			gpuBackendAPI.render_system_api.shutdown_render_system = d3d12::render_system::shutdown_render_system;
			gpuBackendAPI.render_system_api.create_render_environment = d3d12::render_system::create_render_environment;
			gpuBackendAPI.render_system_api.destroy_render_environment = d3d12::render_system::destroy_render_environment;
			gpuBackendAPI.render_system_api.render_window = d3d12::render_system::render_window;
			gpuBackendAPI.render_system_api.default_frame_buffer = d3d12::render_system::default_frame_buffer;
			gpuBackendAPI.render_system_api.frame_index = d3d12::render_system::frame_index;
			
			gpuBackendAPI.render_system_api.initialize_frame = d3d12::render_system::initialize_frame;
			gpuBackendAPI.render_system_api.flush_command_list = d3d12::render_system::flush_command_list;
			gpuBackendAPI.render_system_api.present = d3d12::render_system::present;

			// Window API
			gpuBackendAPI.window_api.hide = d3d12::window::hide;
			gpuBackendAPI.window_api.is_active = d3d12::window::is_active;
			gpuBackendAPI.window_api.show = d3d12::window::show;

			// Frame buffer API
			gpuBackendAPI.frame_buffer_api.clear = d3d12::framebuffer::clear;
		}
		break;
		};
	}

	const GPUBackendAPI& gpu_api()
	{
		return gpuBackendAPI;
	}
}