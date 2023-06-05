#include <directx12.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#define SWAPCHAIN_BUFFERS_COUNT 2
#define RELEASE(elt) if (NULL != elt) elt->Release()

struct dx12_specifics {
	IDXGIFactory2* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device* device;
	ID3D12CommandQueue* command_queue;
	IDXGISwapChain3* swapchain;
	ID3D12DescriptorHeap* rtv_descriptor_heap;
	ID3D12Resource* render_target_views[SWAPCHAIN_BUFFERS_COUNT];
	ID3D12CommandAllocator* command_allocator;
	ID3D12PipelineState* pipeline_state_object;
	ID3D12GraphicsCommandList* command_list;
	ID3D12Fence* fence;
	unsigned int fence_value;
	HANDLE fence_event;
};

void wait_for_sync(dx12_renderer& renderer);
void add_transition_barrier(dx12_renderer& renderer, bool to_present_mode);

int dx12_allocate(dx12_renderer* renderer) {
	if (NULL == renderer) {
		return 1;
	}

	struct dx12_specifics* specifics = (struct dx12_specifics*)malloc(sizeof(struct dx12_specifics));
	if (NULL == specifics) {
		return 2;
	}

	specifics->factory = NULL;
	specifics->adapter = NULL;
	specifics->device = NULL;
	specifics->command_queue = NULL;
	specifics->swapchain = NULL;
	specifics->rtv_descriptor_heap = NULL;
	for (int i = 0; i < SWAPCHAIN_BUFFERS_COUNT; i++) {
		specifics->render_target_views[i] = NULL;
	}
	specifics->command_allocator = NULL;
	specifics->pipeline_state_object = NULL;
	specifics->command_list = NULL;
	specifics->fence = NULL;
	specifics->fence_value = 0;

	renderer->specifics = specifics;
	return 0;
}

void dX12_destroy(dx12_renderer* renderer) {
	if (NULL == renderer) {
		return;
	}

	CloseHandle(renderer->specifics->fence_event);
	RELEASE(renderer->specifics->fence);
	RELEASE(renderer->specifics->command_list);
	RELEASE(renderer->specifics->pipeline_state_object);
	RELEASE(renderer->specifics->command_allocator);
	for (int i = 0; i < SWAPCHAIN_BUFFERS_COUNT; i++) {
		RELEASE(renderer->specifics->render_target_views[i]);
	}
	RELEASE(renderer->specifics->rtv_descriptor_heap);
	RELEASE(renderer->specifics->swapchain);
	RELEASE(renderer->specifics->command_queue);
	RELEASE(renderer->specifics->device);
	RELEASE(renderer->specifics->adapter);
	RELEASE(renderer->specifics->factory);
	free(renderer->specifics);
}

int dx12_initialize(dx12_renderer& renderer, HWND attached_window) {
	HRESULT result = S_OK;
	struct dx12_specifics* pipeline = renderer.specifics;

#if defined(_DEBUG)
	ID3D12Debug* debug_controller;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller));
	debug_controller->EnableDebugLayer();
#endif

	// Factory
	result = CreateDXGIFactory1(IID_PPV_ARGS(&pipeline->factory));
	if (S_OK != result) {
		return 1;
	}

	// Get the appropriate Adapter
	IDXGIAdapter1* tmp_adapter = NULL;
	IDXGIAdapter1* choosen_adapter = NULL;
	DXGI_ADAPTER_DESC1 choosen_adapter_description = {};
	int adapter_idx = 0;
	while (true) {
		result = pipeline->factory->EnumAdapters1(adapter_idx, &tmp_adapter);
		if (DXGI_ERROR_NOT_FOUND == result) {
			break;
		} 
		DXGI_ADAPTER_DESC1 adapter_description = {};
		tmp_adapter->GetDesc1(&adapter_description);
		if (NULL == choosen_adapter || choosen_adapter_description.DedicatedVideoMemory < adapter_description.DedicatedVideoMemory) {
			choosen_adapter_description = adapter_description;
			if (NULL != choosen_adapter) {
				choosen_adapter->Release();
			}
			choosen_adapter = tmp_adapter;
		}
		adapter_idx++;
	}
	wprintf(L"Using Graphic card : %s\n", choosen_adapter_description.Description);
	pipeline->adapter = choosen_adapter;

	// Get the device
	result = D3D12CreateDevice(pipeline->adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pipeline->device));
	if (S_OK != result) {
		return 2;
	}

	// Command queue
	D3D12_COMMAND_QUEUE_DESC command_queue_description = {};
	command_queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	command_queue_description.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	command_queue_description.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	command_queue_description.NodeMask = 0;
	result = pipeline->device->CreateCommandQueue(&command_queue_description, IID_PPV_ARGS(&pipeline->command_queue));
	if (S_OK != result) {
		return 3;
	}

	// Swapchain
	DXGI_SWAP_CHAIN_DESC1 swapchain_description = {};
	swapchain_description.Width = 0;
	swapchain_description.Height = 0; // By default DXGI_SWAP_CHAIN_DESC1 will take the window dimensions if those are 0.
	swapchain_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_description.SampleDesc.Count = 1;
	swapchain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_description.BufferCount = SWAPCHAIN_BUFFERS_COUNT;
	swapchain_description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchain_description.Flags = 0;
	IDXGISwapChain1* tmp_swapchain;
	result = pipeline->factory->CreateSwapChainForHwnd(pipeline->command_queue, attached_window, &swapchain_description, NULL, NULL, &tmp_swapchain);
	if (S_OK != result) {
		return 4;
	}
	pipeline->swapchain = (IDXGISwapChain3*)tmp_swapchain;

	// DescriptorHeap
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_description = {};
	descriptor_heap_description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptor_heap_description.NumDescriptors = SWAPCHAIN_BUFFERS_COUNT;
	descriptor_heap_description.NodeMask = 0;
	descriptor_heap_description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	result = pipeline->device->CreateDescriptorHeap(&descriptor_heap_description, IID_PPV_ARGS(&pipeline->rtv_descriptor_heap));
	if (S_OK != result) {
		return 5;
	}

	// Render Target Views
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle = pipeline->rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	unsigned int descriptor_unit_size = pipeline->device->GetDescriptorHandleIncrementSize(descriptor_heap_description.Type);
	for (int i = 0; i < SWAPCHAIN_BUFFERS_COUNT; i++) {
		pipeline->swapchain->GetBuffer(i, IID_PPV_ARGS(&pipeline->render_target_views[i]));
		pipeline->device->CreateRenderTargetView(pipeline->render_target_views[i], NULL, descriptor_handle);
		descriptor_handle.ptr += descriptor_unit_size;
	}

	// Command Allocator
	result = pipeline->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pipeline->command_allocator));
	if (S_OK != result) {
		return 6;
	}

	// Command List
	result = pipeline->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pipeline->command_allocator, pipeline->pipeline_state_object, IID_PPV_ARGS(&pipeline->command_list));
	if (S_OK != result) {
		return 7;
	}
	pipeline->command_list->Close();

	// Fence
	result = pipeline->device->CreateFence(pipeline->fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pipeline->fence));
	if (S_OK != result) {
		return 8;
	}
	pipeline->fence_event = CreateEvent(NULL, false, false, NULL);
	if (NULL == pipeline->fence_event) {
		return 9;
	}
	wait_for_sync(renderer);

	return 0;	
}

void dx12_prepare(dx12_renderer& renderer) {
	renderer.specifics->command_allocator->Reset();
	renderer.specifics->command_list->Reset(renderer.specifics->command_allocator, renderer.specifics->pipeline_state_object);
	add_transition_barrier(renderer, false);
}

void dx12_clear(dx12_renderer& renderer, float red, float green, float blue, float alpha) {
	const float bg_color[] = { red, green, blue, alpha };
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = renderer.specifics->rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	unsigned int descriptor_unit_size = renderer.specifics->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	unsigned int current_frame_idx = renderer.specifics->swapchain->GetCurrentBackBufferIndex();
	rtv_handle.ptr += current_frame_idx * descriptor_unit_size;
	renderer.specifics->command_list->ClearRenderTargetView(rtv_handle, bg_color, 0, NULL);
}

int dx12_present(dx12_renderer& renderer) {
	HRESULT result = S_OK;
	add_transition_barrier(renderer, true);
	renderer.specifics->command_list->Close();
	ID3D12CommandList* command_lists[] = { renderer.specifics->command_list };
	renderer.specifics->command_queue->ExecuteCommandLists(1, command_lists);
	result = renderer.specifics->swapchain->Present(1, 0);
	if (S_OK != result) {
		return 1;
	}
	wait_for_sync(renderer);
	return 0;
}

void wait_for_sync(dx12_renderer& renderer) {
	struct dx12_specifics* pipeline = renderer.specifics;
	unsigned int current_value = pipeline->fence_value;
	pipeline->command_queue->Signal(pipeline->fence, current_value);
	pipeline->fence_value++;
	if (pipeline->fence->GetCompletedValue() < current_value) {
		pipeline->fence->SetEventOnCompletion(current_value, pipeline->fence_event);
		WaitForSingleObject(pipeline->fence_event, INFINITE);
	}
}

void add_transition_barrier(dx12_renderer& renderer, bool to_present_mode) {
	D3D12_RESOURCE_BARRIER barrier = {};
	unsigned int current_frame_idx = renderer.specifics->swapchain->GetCurrentBackBufferIndex();
	barrier.Transition.pResource = renderer.specifics->render_target_views[current_frame_idx];
	barrier.Transition.StateBefore = to_present_mode ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = to_present_mode ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	renderer.specifics->command_list->ResourceBarrier(1, &barrier);
}