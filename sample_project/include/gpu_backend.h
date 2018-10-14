#pragma once

// Internal includes
#include "gpu_types.h"
#include "texture_descriptor.h"

// External includes
#include <stdint.h>
#include <string>

namespace dxr_demo
{
	namespace RenderingBackEnd
	{
		enum Type
		{
			D3D12
		};
	}

	struct TGraphicSettings
	{
		std::string window_name;
		uint32_t width;
		uint32_t height;
		bool fullscreen;
		uint64_t platformData[6];
	};

	struct GPURenderSystemAPI
	{
		bool(*init_render_system)();
		void(*shutdown_render_system)();

		RenderEnvironment(*create_render_environment)(const TGraphicSettings& graphic_settings);
		void(*destroy_render_environment)(RenderEnvironment render_environment);

		RenderWindow(*render_window)(RenderEnvironment _render);
		Framebuffer (*default_frame_buffer)(RenderEnvironment renderEnv);

		float (*get_time)(RenderEnvironment render_environement);
		uint64_t (*frame_index)(RenderEnvironment renderEnv);

		bool (*initialize_frame)(RenderEnvironment render_environement);
		bool (*flush_command_list)(RenderEnvironment render_environement);
		bool (*present)(RenderEnvironment render_environement);
	};

	struct GPUWindowAPI
	{
		void(*show)(RenderWindow window);
		void(*hide)(RenderWindow window);
		bool(*is_active)(RenderWindow window);
		void(*swap)(RenderWindow window);
	};

	struct GPUFrameBufferAPI
	{
		// Framebuffer manipulation functions
		void(*clear)(Framebuffer frame_buffer, const float* color);
	};

	struct GPUBackendAPI
	{
		GPURenderSystemAPI render_system_api;
		GPUWindowAPI window_api;
		GPUFrameBufferAPI frame_buffer_api;
	};

	// Initialize the target api
	void initialize_gpu_backend(RenderingBackEnd::Type backend_type);

	// Request the target gpu API
	const GPUBackendAPI& gpu_api();
}