#pragma once

// Internal includes
#include "gpu_backend.h"

namespace dxr_demo
{
	namespace d3d12
	{
		TGraphicSettings default_settings();

		namespace render_system
		{
			bool init_render_system();
			void shutdown_render_system();

			RenderEnvironment create_render_environment(const TGraphicSettings& graphic_settings);
			void destroy_render_environment(RenderEnvironment render_environment);

			RenderWindow render_window(RenderEnvironment render_environement);
			void resize_window(RenderEnvironment renderEnv, uint32_t width, uint32_t height);

			Framebuffer default_frame_buffer(RenderEnvironment renderEnv);

			uint64_t frame_index(RenderEnvironment renderEnv);

			float get_time(RenderEnvironment render_environement);

			bool initialize_frame(RenderEnvironment render_environement);
			bool flush_command_list(RenderEnvironment render_environement);
			bool present(RenderEnvironment render_environement);
		}

		namespace window
		{
			void show(RenderWindow window);
			void hide(RenderWindow window);
			bool is_active(RenderWindow window);
		}

		namespace framebuffer
		{
			void clear(Framebuffer frame_buffer, const float* clearColor);
		}
	}
}
