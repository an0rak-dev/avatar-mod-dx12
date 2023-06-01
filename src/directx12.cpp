#include <directx12.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_2.h>

#define SWAPCHAIN_BUFFERS_COUNT 2

struct dx12_specifics {
	IDXGIFactory2* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device* device;
	ID3D12CommandQueue* command_queue;
	IDXGISwapChain1* swapchain;
	ID3D12DescriptorHeap* rtv_descriptor_heap;
	ID3D12Resource* render_target_views[SWAPCHAIN_BUFFERS_COUNT];
	ID3D12CommandAllocator* command_allocator;
};

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

	renderer->specifics = specifics;
	return 0;
}

void dX12_destroy(dx12_renderer* renderer) {
	if (NULL == renderer) {
		return;
	}

	renderer->specifics->command_allocator->Release();
	for (int i = 0; i < SWAPCHAIN_BUFFERS_COUNT; i++) {
		renderer->specifics->render_target_views[i]->Release();
	}
	renderer->specifics->rtv_descriptor_heap->Release();
	renderer->specifics->swapchain->Release();
	renderer->specifics->command_queue->Release();
	renderer->specifics->device->Release();
	renderer->specifics->adapter->Release();
	renderer->specifics->factory->Release();
	free(renderer->specifics);
}

int dx12_initialize(dx12_renderer& renderer, HWND attached_window) {
	HRESULT result = S_OK;
	struct dx12_specifics* pipeline = renderer.specifics;
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
	result = pipeline->factory->CreateSwapChainForHwnd(pipeline->command_queue, attached_window, &swapchain_description, NULL, NULL, &pipeline->swapchain);
	if (S_OK != result) {
		return 4;
	}

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

	// Create Command Allocator
	result = pipeline->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pipeline->command_allocator));
	if (S_OK != result) {
		return 6;
	}

	return 0;	
}

void dx12_clear(dx12_renderer& renderer, double red, double green, double blue, double alpha) {
	// TODO
}

int dx12_present(dx12_renderer& renderer) {
	// TODO
	return 0;
}