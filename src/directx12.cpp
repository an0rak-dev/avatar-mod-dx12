#include <directx12.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi.h>

struct dx12_specifics {
	IDXGIFactory1* factory;
	IDXGIAdapter1* adapter;
	ID3D12Device* device;
};

int dx12_allocate(dx12_renderer* renderer) {
	if (NULL == renderer) {
		return 1;
	}

	struct dx12_specifics* specifics = (struct dx12_specifics*)malloc(sizeof(struct dx12_specifics));
	if (NULL == specifics) {
		return 2;
	}

	renderer->specifics = specifics;
	return 0;
}

void dX12_destroy(dx12_renderer* renderer) {
	if (NULL == renderer) {
		return;
	}

	renderer->specifics->device->Release();
	renderer->specifics->adapter->Release();
	renderer->specifics->factory->Release();
	free(renderer->specifics);
}

int dx12_initialize(dx12_renderer& renderer) {
	HRESULT result = S_OK;
	// Factory
	result = CreateDXGIFactory1(IID_PPV_ARGS(&renderer.specifics->factory));
	if (S_OK != result) {
		return 1;
	}

	// Get the appropriate Adapter
	IDXGIAdapter1* tmp_adapter = NULL;
	IDXGIAdapter1* choosen_adapter = NULL;
	DXGI_ADAPTER_DESC1 choosen_adapter_description = {};
	int adapter_idx = 0;
	while (true) {
		result = renderer.specifics->factory->EnumAdapters1(adapter_idx, &tmp_adapter);
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
	renderer.specifics->adapter = choosen_adapter;

	// Get the device
	result = D3D12CreateDevice(renderer.specifics->adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&renderer.specifics->device));
	if (S_OK != result) {
		return 2;
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