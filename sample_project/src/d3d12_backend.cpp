// Internal includes
#include "d3d12_backend.h"
#include "renderer.h"

// External includes
#include <d3d12.h>
#include <dxgi1_5.h>
#include "d3dx12.h"
#include <chrono>
#include <algorithm>

#undef max

namespace dxr_demo
{
	namespace d3d12
	{
		// D3D12 platform specific data
		#define PLATFORM_DATA_HINSTANCE 0

		// Frame buffer index
		#define NUM_SWAP_FRAME_BUFFERS 2

		// The size of a descriptor heap page
		#define DESCRIPTOR_HEAP_PAGE_SIZE 512

		// Forward declaration
		struct D3D12RenderEnvironement;

		// Structure that encapsulates everything relative to the window instance
		struct D3D12Window
		{
			// Pointer to the Windows window instance that will recieve the result of our rendering.
			HWND nativeWindow;

			// Dimension of the target window
			uint32_t width;
			uint32_t height;
		};

		// Structure that hold everything related to command submission and execution
		struct D3D12CommandSystem
		{
			// The command queue is a object that allows us to submit command lists. (at least that is what i got from it at the moment)
			ID3D12CommandQueue* commandQueue;

			// The command allocator allows us to add new commands into the comand list, it cannot be shared between command lists
			ID3D12CommandAllocator* commandAllocator;

			// The command list is an object that allows to submit individual rendering requests (draw, compute, copy, dispatch)
			ID3D12GraphicsCommandList* commandList;

			// Fence to wait on the command list to be executed
			ID3D12Fence* fence;

			// Value that matches the fence
			UINT64 fenceValue;

			// This manages to makes us hold on the fence to be passed
			HANDLE fenceEvent;
		};

		struct D3D12IndividualDescriptorHeap
		{
			// Pointer to the d3d12 heap
			ID3D12DescriptorHeap* descriptorHeap;

			// Pointer to the start of the heap on the CPU
			CD3DX12_CPU_DESCRIPTOR_HANDLE heapCpuStart;

			// Pointer to the current state of the heap on the CPU
			CD3DX12_CPU_DESCRIPTOR_HANDLE heapCpuCurrent;

			// Pointer to the start of the heap on the GPU
			D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart;

			// Pointer to the start of the heap on the GPU
			D3D12_GPU_DESCRIPTOR_HANDLE heapGpuCurrent;
			
			// Value that tracks the current size of the descriptor heap
			uint32_t heapCurrentSize;

			// Value that keep track of the maximal size of the heap
			uint32_t heapMaxSize;

			// Value that keeps track of the size of the elements that are in this heap
			uint32_t elementSize;
		};

		// Structure that handles everything relative to a heapdescriptor
		struct D3D12DescriptorHeapSystem
		{
			// The set of heaps that have been allocated by the system
			std::vector<D3D12IndividualDescriptorHeap> system[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
		};

		struct D3D12FrameBuffer
		{
			// We keep a pointer to the render environement
			D3D12RenderEnvironement* renderEnvironement;

			// Index of the Descriptor heap where this frame buffer lies
			uint32_t heapIndex;

			// Index of the resource in the source descriptor heap
			uint32_t rtvIndex;

			// The rtv of the frame buffer
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtv;

			// Fence value for the frame buffer
			UINT64 fenceValue;

			// The set of texture that are associated to this frame buffer
			ID3D12Resource* resource;
		};

		// This structure holds everything relative to the swap chain /present mechanic
		struct D3D12SwapChainSystem
		{
			// The swap chain, it is used for presenting the rendeed image into the window
			IDXGISwapChain4* swapChain;

			// Index of the current back buffer
			uint32_t current_back_buffer;

			// The buffers that allow us to present to the window
			D3D12FrameBuffer swap_buffer_array[NUM_SWAP_FRAME_BUFFERS];
		};

		struct D3D12RenderEnvironement
		{
			// Hinstance from the main function. This holds a reference to the current program. It is mainly used simply for the creation of the window
			HINSTANCE hInstance;

			// Structure that hold the data of the window
			D3D12Window window;

			// The factory is required for the creation of pretty much every DXD12 structure, so it is the first thing we create
			IDXGIFactory4* dxgiFactory;

			// A reference to the native device that matches the feature level that we require
			ID3D12Device2* device;
			uint64_t maxVideoMemory;

			// Descriptor heap system that hold everything relative to the the heaps
			D3D12DescriptorHeapSystem descriptorHeapSystem;

			// Command system that hold everything relative to command submission and execution
			D3D12CommandSystem commandSystem;

			// Swap chain system that hold everything relative to the swap mechanic
			D3D12SwapChainSystem swapSystem;

			// Index of the current frame
			uint64_t frameIndex;

			// Status check flag
			HRESULT status_flag;
		};

		// Main message handler for the sample.
		LRESULT CALLBACK d3d_window_callback(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
		{
			// Convert the parameter to the renderer
			TRenderer* renderer = reinterpret_cast<TRenderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

			switch (message)
			{
				case WM_CREATE:
				{
					// Save the DXSample* passed in to CreateWindow.
					LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
					SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
					return 0;
				}
				break;
				case WM_KEYDOWN:
				{
					renderer->key_down(static_cast<UINT8>(wParam));
				}
				break;
				case WM_KEYUP:
					renderer->key_up(static_cast<UINT8>(wParam));
					return 0;

				case WM_PAINT:
					renderer->update();
					renderer->render();
					return 0;

				case WM_DESTROY:
					PostQuitMessage(0);
					return 0;
				case WM_SIZE:
				{

					// Grab the render environement
					D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)renderer->render_environement();

					RECT newWindowDimensions = {};
					::GetClientRect(renderEnv->window.nativeWindow, &newWindowDimensions);

					uint32_t width = (uint32_t)(newWindowDimensions.right - newWindowDimensions.left);
					uint32_t height = (uint32_t)(newWindowDimensions.bottom - newWindowDimensions.top);

					d3d12::render_system::resize_window(renderer->render_environement(), width, height);
				}
				break;
			};

			// Handle any messages the switch statement didn't.
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		TGraphicSettings default_settings()
		{
			TGraphicSettings settings;
			settings.width = 1280;
			settings.height = 720;
			settings.fullscreen = false;
			return settings;
		}

		namespace render_system
		{
			bool init_render_system()
			{
				return true;
			}
			void shutdown_render_system()
			{
			}

			bool create_window(const TGraphicSettings& graphicsSettings, D3D12RenderEnvironement& renderEnv)
			{
				// Fill the window class
				WNDCLASSEX windowClass = { 0 };
				windowClass.cbSize = sizeof(WNDCLASSEX);
				windowClass.style = CS_HREDRAW | CS_VREDRAW;
				windowClass.lpfnWndProc = d3d_window_callback;
				windowClass.hInstance = renderEnv.hInstance;
				windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
				windowClass.lpszClassName = graphicsSettings.window_name.c_str();

				// Register the window class first.
				RegisterClassEx(&windowClass);

				// Set the dimensions of the window
				RECT newWindowDimensions = { 0, 0, (LONG)graphicsSettings.width, (LONG)graphicsSettings.height };
				AdjustWindowRect(&newWindowDimensions, WS_OVERLAPPEDWINDOW, FALSE);

				// Create the window
				renderEnv.window.nativeWindow = CreateWindow(windowClass.lpszClassName, graphicsSettings.window_name.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
					graphicsSettings.width, graphicsSettings.height, nullptr, nullptr, renderEnv.hInstance, (LPVOID)graphicsSettings.platformData[1]);

				// Keep track of the dimensions
				renderEnv.window.width = graphicsSettings.width;
				renderEnv.window.height = graphicsSettings.height;

				// Alright, all set
				return true;
			}

			bool create_dxgi_factory(D3D12RenderEnvironement& renderEnv)
			{
				// The set of flags that should be applied at the factory creation
				UINT createFactoryFlags = 0;
			#ifdef _DEBUG
				createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
			#endif

				// Actually create the factory
				renderEnv.status_flag = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&renderEnv.dxgiFactory));

				// Make sure everything went well
				return SUCCEEDED(renderEnv.status_flag);
			}

			bool create_device(const TGraphicSettings& graphic_settings, D3D12RenderEnvironement& renderEnv)
			{
				IDXGIAdapter1* adapter = nullptr;
				int adapterIndex = 0;
				bool adapterFound = false;

				// find first hardware gpu that supports d3d 12
				while (renderEnv.dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
				{
					DXGI_ADAPTER_DESC1 desc;
					adapter->GetDesc1(&desc);

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						// we dont want a software device
						adapterIndex++; // add this line here. Its not currently in the downloadable project
						continue;
					}

					/*
					if (graphic_settings.platformData[1] == 666)
					{
						renderEnv.status_flag = D3D12EnableExperimentalFeatures(1, &D3D12RaytracingPrototype, NULL, NULL);
					}
					*/

					// we want a device that is compatible with direct3d 12 and the latest feature level
					renderEnv.status_flag = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr);
					if (SUCCEEDED(renderEnv.status_flag))
					{
						renderEnv.maxVideoMemory = desc.DedicatedVideoMemory;
						adapterFound = true;
						break;
					}

					adapterIndex++;
				}

				// Did we find any adapter?
				if (!adapterFound) return false;

				// Try to create a device
				renderEnv.status_flag = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&renderEnv.device) );
				return SUCCEEDED(renderEnv.status_flag);
			}

			bool create_command_queue(D3D12RenderEnvironement& renderEnv)
			{
				// Create a direct command queue with normal priority
				D3D12_COMMAND_QUEUE_DESC cqDesc = {};
				cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
				cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				cqDesc.NodeMask = 0;

				renderEnv.status_flag = renderEnv.device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&renderEnv.commandSystem.commandQueue));
				return SUCCEEDED(renderEnv.status_flag);
			}

			bool create_new_individual_heap(D3D12RenderEnvironement& renderEnv, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t heapSize, uint32_t& outHeapIndex)
			{
				// Fetch a reference to the current heap set
				std::vector<D3D12IndividualDescriptorHeap>& currentDescriptorHeapArray = renderEnv.descriptorHeapSystem.system[heapType];

				// Allocate a new individual heap
				uint32_t newHeapIndex = (uint32_t)currentDescriptorHeapArray.size();
				currentDescriptorHeapArray.resize(newHeapIndex + 1);
				// Fetch the newly created descriptor heap
				D3D12IndividualDescriptorHeap& currentIndividualHeap = currentDescriptorHeapArray[newHeapIndex];

				// Descriptor that defines 
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.NumDescriptors = heapSize;
				desc.Type = heapType;

				// Create the target descriptor heap
				renderEnv.status_flag = renderEnv.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&currentIndividualHeap.descriptorHeap));
				if (FAILED(renderEnv.status_flag))
				{
					return false;
				}

				// Set the CPU Start
				currentIndividualHeap.heapCpuStart = currentIndividualHeap.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
				currentIndividualHeap.heapCpuCurrent = currentIndividualHeap.heapCpuStart;

				// Set the GPU Start
				currentIndividualHeap.heapGpuStart = currentIndividualHeap.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
				currentIndividualHeap.heapGpuCurrent = currentIndividualHeap.heapGpuStart;

				// Init the heap's CPU current size
				currentIndividualHeap.heapCurrentSize = 0;

				// Keep track of the maximal size of the heap
				currentIndividualHeap.heapMaxSize = heapSize;

				// Keep track of the element's size
				currentIndividualHeap.elementSize = renderEnv.device->GetDescriptorHandleIncrementSize(heapType);

				// Return the new heap's index
				outHeapIndex = newHeapIndex;

				return SUCCEEDED(renderEnv.status_flag);
			}

			bool create_new_descriptor_heap_rtv_element(D3D12RenderEnvironement& renderEnv, D3D12IndividualDescriptorHeap& individualHeap, ID3D12Resource* resource, uint32_t& outElementIndex)
			{
				// We can only create an element in this heap is there is space left
				if (individualHeap.heapMaxSize == individualHeap.heapCurrentSize) return false;

				// Create the render target view in the heap
				renderEnv.device->CreateRenderTargetView(resource, nullptr, individualHeap.heapCpuCurrent);

				// Offset the heap
				individualHeap.heapCpuCurrent.Offset(individualHeap.elementSize);

				// Set the output element index
				outElementIndex = individualHeap.heapCurrentSize;

				// Increase the heap element count
				individualHeap.heapCurrentSize++;
				return true;
			}

			bool create_swap_chain(D3D12RenderEnvironement& renderEnv)
			{
				// Describe the multisampling state
				DXGI_SAMPLE_DESC sample_desc = {};
				sample_desc.Count = 1;
				sample_desc.Quality = 0;

				// Describe and create the swap chain.
				DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
				swapChainDesc.Width = renderEnv.window.width;
				swapChainDesc.Height = renderEnv.window.height;
				swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				swapChainDesc.Stereo = false;
				swapChainDesc.SampleDesc = sample_desc;
				swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapChainDesc.BufferCount = NUM_SWAP_FRAME_BUFFERS;
				swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
				swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

				// Create the swap chain for the target window
				IDXGISwapChain1* swapChain1 = nullptr;
				renderEnv.status_flag = renderEnv.dxgiFactory->CreateSwapChainForHwnd(renderEnv.commandSystem.commandQueue, renderEnv.window.nativeWindow, &swapChainDesc, nullptr, nullptr, &swapChain1);
				if (FAILED(renderEnv.status_flag))
				{
					return false;
				}
				renderEnv.swapSystem.swapChain = (IDXGISwapChain4*)swapChain1;
				// Disable alt+enter to full screen
				renderEnv.dxgiFactory->MakeWindowAssociation(renderEnv.window.nativeWindow, DXGI_MWA_NO_ALT_ENTER);
				return true;
			}

			bool create_swap_chain_rtvs(D3D12RenderEnvironement& renderEnv)
			{
				// First of all, let's create a new descriptor heap that matches the need of the swap chain
				uint32_t swapChainHeapIndex = 0;
				create_new_individual_heap(renderEnv, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_SWAP_FRAME_BUFFERS, swapChainHeapIndex);

				// Fetch the target descriptor Heap
				D3D12IndividualDescriptorHeap& currentDescriptorHeap = renderEnv.descriptorHeapSystem.system[D3D12_DESCRIPTOR_HEAP_TYPE_RTV][swapChainHeapIndex];

				for (uint32_t bufferIdx = 0; bufferIdx < NUM_SWAP_FRAME_BUFFERS; ++bufferIdx)
				{
					// Fetch the buffer's resource
					renderEnv.status_flag = renderEnv.swapSystem.swapChain->GetBuffer(bufferIdx, IID_PPV_ARGS(&renderEnv.swapSystem.swap_buffer_array[bufferIdx].resource));
					if (FAILED(renderEnv.status_flag))
					{
						return false;
					}

					// Set the data of the back buffer
					renderEnv.swapSystem.swap_buffer_array[bufferIdx].renderEnvironement = &renderEnv;
					renderEnv.swapSystem.swap_buffer_array[bufferIdx].heapIndex = swapChainHeapIndex;
					create_new_descriptor_heap_rtv_element(renderEnv, currentDescriptorHeap, renderEnv.swapSystem.swap_buffer_array[bufferIdx].resource, renderEnv.swapSystem.swap_buffer_array[bufferIdx].rtvIndex);
					renderEnv.swapSystem.swap_buffer_array[bufferIdx].rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(currentDescriptorHeap.heapCpuStart, bufferIdx, currentDescriptorHeap.elementSize);
					renderEnv.swapSystem.swap_buffer_array[bufferIdx].fenceValue = 0;
				}

				// AAAAND we are done.
				return true;
			}

			bool create_command_allocator(D3D12RenderEnvironement& renderEnvironement)
			{
				// Create a command allocator for our command list
				renderEnvironement.status_flag = renderEnvironement.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderEnvironement.commandSystem.commandAllocator));
				return SUCCEEDED(renderEnvironement.status_flag);
			}

			bool create_command_list(D3D12RenderEnvironement& renderEnvironement)
			{
				// create the command list with the our allocator
				renderEnvironement.status_flag = renderEnvironement.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderEnvironement.commandSystem.commandAllocator, NULL, IID_PPV_ARGS(&renderEnvironement.commandSystem.commandList));
				if (FAILED(renderEnvironement.status_flag))
				{
					return false;
				}

				// command lists are created in the recording state. our main loop will set it up for recording again so close it now
				renderEnvironement.commandSystem.commandList->Close();
				return true;
			}

			bool create_fence(D3D12RenderEnvironement& renderEnvironement)
			{    
				renderEnvironement.status_flag = renderEnvironement.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderEnvironement.commandSystem.fence));
				renderEnvironement.commandSystem.fenceValue = 0;
				return SUCCEEDED(renderEnvironement.status_flag);
			}

			bool create_fence_event(D3D12RenderEnvironement& renderEnvironement)
			{
				renderEnvironement.commandSystem.fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
				return renderEnvironement.commandSystem.fenceEvent != nullptr;
			}

			RenderEnvironment create_render_environment(const TGraphicSettings& graphic_settings)
			{
				D3D12RenderEnvironement* newRE = new D3D12RenderEnvironement();
				newRE->hInstance = nullptr;
				newRE->window.nativeWindow = nullptr;
				newRE->dxgiFactory = nullptr;

				// Initialize the device data
				newRE->device = nullptr;
				newRE->maxVideoMemory = 0;

				// Initialize the command system
				newRE->commandSystem.commandQueue = nullptr;
				newRE->commandSystem.commandList = nullptr;
				newRE->commandSystem.commandAllocator = nullptr;
				newRE->commandSystem.fence = nullptr;
				newRE->commandSystem.fenceValue = 0;
				newRE->commandSystem.fenceEvent = nullptr;

				// Initialize the swap chain
				newRE->swapSystem.swapChain = nullptr;

				// Initialize the frame count
				newRE->frameIndex = 0;

				newRE->status_flag = S_OK;

				// Grab the hinstance from the platform specific parameters
				newRE->hInstance = reinterpret_cast<HINSTANCE>(graphic_settings.platformData[PLATFORM_DATA_HINSTANCE]);

				// Create the window
				if (!create_window(graphic_settings, *newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the dxgi factory
				if (!create_dxgi_factory(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the d3d12 device
				if (!create_device(graphic_settings, *newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the command queue
				if (!create_command_queue(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the swap chain
				if (!create_swap_chain(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the swap chain buffers
				if (!create_swap_chain_rtvs(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the command allocator
				if (!create_command_allocator(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				// Create the command list
				if (!create_command_list(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				if (!create_fence(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}

				if (!create_fence_event(*newRE))
				{
					destroy_render_environment((RenderEnvironment)newRE);
					return 0;
				}
				return (RenderEnvironment)newRE;
			}

			void destroy_render_environment(RenderEnvironment render_environment)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environment;

				// Swap chain
				for (uint32_t bufferIdx = 0; bufferIdx < NUM_SWAP_FRAME_BUFFERS; ++bufferIdx)
				{
					if (renderEnv->swapSystem.swap_buffer_array[bufferIdx].resource)
					{
						renderEnv->swapSystem.swap_buffer_array[bufferIdx].resource->Release();
					}
				}
				if (renderEnv->swapSystem.swapChain)
				{
					renderEnv->swapSystem.swapChain->Release();
				}

				// Release the command system
				if (renderEnv->commandSystem.fenceEvent)
				{
					CloseHandle(renderEnv->commandSystem.fenceEvent);
				}
				if (renderEnv->commandSystem.fence)
				{
					renderEnv->commandSystem.fence->Release();
				}
				if (renderEnv->commandSystem.commandList)
				{
					renderEnv->commandSystem.commandList->Release();
				}
				if (renderEnv->commandSystem.commandAllocator)
				{
					renderEnv->commandSystem.commandAllocator->Release();
				}
				if (renderEnv->commandSystem.commandQueue)
				{
					renderEnv->commandSystem.commandQueue->Release();
				}

				// Release the device
				if (renderEnv->device)
				{
					renderEnv->device->Release();
				}

				// Release the dxgi factory
				if (renderEnv->dxgiFactory)
				{
					renderEnv->dxgiFactory->Release();
				}

				// Destroy the window
				DestroyWindow(renderEnv->window.nativeWindow);

				// Delete the render environement structure
				delete renderEnv;
			}

			RenderWindow render_window(RenderEnvironment render_environement)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;
				return (RenderWindow)(&renderEnv->window);
			}

			void resize_window(RenderEnvironment renderEnvPtr, uint32_t width, uint32_t height)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)renderEnvPtr;

				if (renderEnv->window.width != width || renderEnv->window.height != height)
				{
					// Don't allow 0 size swap chain back buffers.
					renderEnv->window.width = std::max(1u, width);
					renderEnv->window.height = std::max(1u, height);

					// Reset the buffers
					for (uint32_t bufferIdx = 0; bufferIdx < NUM_SWAP_FRAME_BUFFERS; ++bufferIdx)
					{
						renderEnv->swapSystem.swap_buffer_array[bufferIdx].resource = nullptr;
						renderEnv->swapSystem.swap_buffer_array[bufferIdx].fenceValue = 0;
					}

					// Get the previous descriptor of the swp chain
					DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
					renderEnv->swapSystem.swapChain->GetDesc(&swapChainDesc);
					renderEnv->swapSystem.swapChain->ResizeBuffers(NUM_SWAP_FRAME_BUFFERS, renderEnv->window.width, renderEnv->window.height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);

					create_swap_chain_rtvs(*renderEnv);
				}
			}

			Framebuffer default_frame_buffer(RenderEnvironment render_environement)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;
				return (Framebuffer)(&renderEnv->swapSystem.swap_buffer_array[renderEnv->swapSystem.current_back_buffer]);
			}

			uint64_t frame_index(RenderEnvironment render_environement)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;
				return renderEnv->frameIndex;
			}

			float get_time(RenderEnvironment render_environement)
			{
				return 0.0f;
			}

			bool initialize_frame(RenderEnvironment render_environement)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;

				// Prepare the command list for the following frame
				renderEnv->status_flag = renderEnv->commandSystem.commandAllocator->Reset();
				renderEnv->status_flag |= renderEnv->commandSystem.commandList->Reset(renderEnv->commandSystem.commandAllocator, nullptr);

				// Fetch which buffer is the current back buffer
				renderEnv->swapSystem.current_back_buffer = renderEnv->swapSystem.swapChain->GetCurrentBackBufferIndex();

				// We moved to the next frame
				renderEnv->frameIndex++;

				return SUCCEEDED(renderEnv->status_flag);
			}

			bool flush_command_list(RenderEnvironment render_environement)
			{
				// Cast the render environment
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;

				// Change the state of the back buffer from render buffer to present for the present
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderEnv->swapSystem.swap_buffer_array[renderEnv->swapSystem.current_back_buffer].resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
				renderEnv->commandSystem.commandList->ResourceBarrier(1, &barrier);

				// Everything that needed to be submitted is submitted, close the command list
				renderEnv->status_flag = renderEnv->commandSystem.commandList->Close();
				if (FAILED(renderEnv->status_flag))
				{
					return false;
				}

				// create an array of command lists (only one command list here)
				ID3D12CommandList* ppCommandLists[] = { renderEnv->commandSystem.commandList };

				// execute the array of command lists
				renderEnv->commandSystem.commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

				// Wait for the excecution to end
				uint64_t fenceValueForSignal = ++renderEnv->commandSystem.fenceValue;
				renderEnv->status_flag = renderEnv->commandSystem.commandQueue->Signal(renderEnv->commandSystem.fence, fenceValueForSignal);
				return SUCCEEDED(renderEnv->status_flag);
			}

			void wait_for_fence_value(D3D12RenderEnvironement render_environement)
			{
				uint64_t fenceCompletedValue = render_environement.commandSystem.fence->GetCompletedValue();
				if (fenceCompletedValue < render_environement.commandSystem.fenceValue)
				{
					render_environement.status_flag = render_environement.commandSystem.fence->SetEventOnCompletion(render_environement.commandSystem.fenceValue, render_environement.commandSystem.fenceEvent);
					if (FAILED(render_environement.status_flag))
					{
						return;
					}
					std::chrono::milliseconds duration = std::chrono::milliseconds::max();
					::WaitForSingleObject(render_environement.commandSystem.fenceEvent, static_cast<DWORD>(duration.count()));
				}
			}

			bool present(RenderEnvironment render_environement)
			{
				D3D12RenderEnvironement* renderEnv = (D3D12RenderEnvironement*)render_environement;

				bool running = true;
				// present the current backbuffer
				renderEnv->status_flag = renderEnv->swapSystem.swapChain->Present(0, 0);
				if (FAILED(renderEnv->status_flag))
				{
					HRESULT failReason = renderEnv->device->GetDeviceRemovedReason();
					running = false;
				}

				wait_for_fence_value(*renderEnv);

				return running;
			}
		}

		namespace window
		{
			void show(RenderWindow renderWindow)
			{
				D3D12Window* window = (D3D12Window*)renderWindow;
				ShowWindow(window->nativeWindow, SW_SHOW);
			}

			void hide(RenderWindow renderWindow)
			{
				D3D12Window* window = (D3D12Window*)renderWindow;
				ShowWindow(window->nativeWindow, SW_HIDE);

			}
			bool is_active(RenderWindow renderWindow)
			{
				D3D12Window* window = (D3D12Window*)renderWindow;
				HWND currentWindow = GetActiveWindow();
				return window->nativeWindow == currentWindow;
			}

		}

		namespace framebuffer
		{
			void clear(Framebuffer framebuffer, const float* clearColor)
			{
				D3D12FrameBuffer* currentFrameBuffer = (D3D12FrameBuffer*)framebuffer;
				D3D12RenderEnvironement* renderEnv = currentFrameBuffer->renderEnvironement;

				// Notify the GPU that this render frame buffer needs to become a render target and no more a presentation frame buffer
				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( currentFrameBuffer->resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
				// Wait for the state change
				currentFrameBuffer->renderEnvironement->commandSystem.commandList->ResourceBarrier(1, &barrier);

				// Grab the CPU descriptor handle of the render target
				currentFrameBuffer->renderEnvironement->commandSystem.commandList->ClearRenderTargetView(currentFrameBuffer->rtv, clearColor, 0, nullptr);
			}
		}
	}
}