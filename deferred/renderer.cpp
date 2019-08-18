/*AMDG*/
#include "renderer.h"
#include "util.h"
#include "settings.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

bool initDevice(Dx12Renderer* pRenderer, HWND hwnd)
{
    HRESULT hr = S_OK;

    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    CComPtr<IDXGIFactory1> dxgiFactory;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        ErrorMsg("Failed to create DXGI Factory");
        return false;
    }

    CComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiFactory->EnumAdapters(0, &dxgiAdapter);
    if (hr == DXGI_ERROR_NOT_FOUND)
    {
        ErrorMsg("No adapters found");
        return false;
    }

#if defined(_DEBUG)
	CComPtr<ID3D12Debug> debugController;
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (SUCCEEDED(hr))
    {
		debugController->EnableDebugLayer();
    }
    else
    {
        WarningMsg("Failed to create debug interface.");
    }
#endif

    // create device
    hr = D3D12CreateDevice(dxgiAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pRenderer->pDevice));
    if (FAILED(hr))
    {
        ErrorMsg("D3D12CreateDevice failed");
        return false;
    }

	ID3D12Device* pDevice = pRenderer->pDevice;

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {
        D3D12_COMMAND_LIST_TYPE_DIRECT,  // D3D12_COMMAND_LIST_TYPE Type;
        0,                               // INT Priority;
        D3D12_COMMAND_QUEUE_FLAG_NONE,   // D3D12_COMMAND_QUEUE_FLAG Flags;
        0                                // UINT NodeMask;
    };

    hr = pDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&pRenderer->pCommandQueue));
    if (FAILED(hr))
    {
        ErrorMsg("Couldn't create command queue");
        return false;
    }

	for (UINT i = 0; i < renderer::submitQueueDepth; i++)
	{
		CmdSubmission* sub = &pRenderer->cmdSubmissions[i];

		// command allocator
		hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&sub->cmdAlloc));
		if (FAILED(hr)) { return 0; }
	}

	hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pRenderer->cmdSubmissions[0].cmdAlloc, nullptr, IID_PPV_ARGS(&pRenderer->pGfxCmdList));
	if (FAILED(hr)) { ErrorMsg("Couldn't create command list"); }

	ID3D12CommandList* pGfxCmdList = pRenderer->pGfxCmdList;

    // create swapchain
    CComPtr<IDXGISwapChain> pSwapChain;
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = renderer::swapChainBufferCount;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = 0;
    swapChainDesc.BufferDesc.Width = renderer::width;
    swapChainDesc.BufferDesc.Height = renderer::height;
    swapChainDesc.BufferDesc.Format = colorFormat;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;

    hr = dxgiFactory->CreateSwapChain(pRenderer->pCommandQueue, &swapChainDesc, &pSwapChain);
    if (FAILED(hr))
    {
        ErrorMsg("Couldn't create swapchain");
        return false;
    }

    // Query for swapchain3 swapchain
    hr = pSwapChain->QueryInterface(&pRenderer->pSwapChain);
    if (FAILED(hr))
    {
		ErrorMsg("Couldn't create swapchain3");
        return 0;
    }

	// Submission fence
	uint64 submitCount = 0;
	hr = pDevice->CreateFence(submitCount, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pRenderer->pSubmitFence));
	if (FAILED(hr))
	{
		ErrorMsg("Couldn't create submission fence");
		return 0;
	}

	// create render target view descriptor heap for displayable back buffers
	D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
										renderer::swapChainBufferCount,
										D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
										0 };

	hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pRenderer->rtvHeap));
	if (FAILED(hr))
	{
		ErrorMsg("Failed to create render target heap");
		return 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvDescHandle = pRenderer->rtvHeap->GetCPUDescriptorHandleForHeapStart();
	uint rtvDescHandleIncSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Set up backbuffer heap
	for (uint i = 0; i < renderer::swapChainBufferCount; ++i)
	{
		hr = pRenderer->pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pRenderer->backbuf[i]));
		if (FAILED(hr))
		{
			ErrorMsg("Failed to retrieve back buffer from swapchain");
			return 0;
		}

		// Create RTVs in heap
		pDevice->CreateRenderTargetView(pRenderer->backbuf[i], nullptr, rtvDescHandle);
		rtvDescHandle.ptr += rtvDescHandleIncSize;
	}

	pRenderer->backbufCurrent = pRenderer->pSwapChain->GetCurrentBackBufferIndex();

	// default scissor rect and viewport
	pRenderer->defaultScissor = {};
	pRenderer->defaultScissor.left   = 0;
	pRenderer->defaultScissor.top    = 0;
	pRenderer->defaultScissor.right  = renderer::width;
	pRenderer->defaultScissor.bottom = renderer::height;

	pRenderer->defaultViewport = {};
	pRenderer->defaultViewport.Height   = static_cast<float>(renderer::height);
	pRenderer->defaultViewport.Width    = static_cast<float>(renderer::width);
	pRenderer->defaultViewport.TopLeftX = 0;
	pRenderer->defaultViewport.TopLeftY = 0;
	pRenderer->defaultViewport.MaxDepth = 1.0f;
	pRenderer->defaultViewport.MinDepth = 0.0f;

    return true;
}

ID3D12Resource* createBuffer(Dx12Renderer* pRenderer, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES states)
{
	HRESULT hr = S_OK;

	D3D12_HEAP_PROPERTIES heapProps = {};
	D3D12_RESOURCE_DESC desc = {};
	ID3D12Resource* resource;

	heapProps.Type = heapType;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	hr = pRenderer->pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
		&desc, states, nullptr, IID_PPV_ARGS(&resource));

	if (FAILED(hr)) 
	{ 
		ErrorMsg("Buffer creation failed.");  
		return nullptr;
	}

	return resource;
}

void transitionResource(Dx12Renderer* pRenderer, ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = res;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	pRenderer->pGfxCmdList->ResourceBarrier(1, &barrier);
}

bool uploadBuffer(Dx12Renderer* pRenderer, ID3D12Resource* pResource, void const* data, UINT rowPitch, UINT slicePitch)
{
	HRESULT hr = S_OK;
	ID3D12Device* pDevice = pRenderer->pDevice;

	D3D12_RESOURCE_DESC desc = pResource->GetDesc();

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
	UINT64 rowSize, totalBytes;
	pDevice->GetCopyableFootprints(&desc, 0, 1, 0, &fp, nullptr, &rowSize, &totalBytes);

	// create upload buffer
	CComPtr<ID3D12Resource> uploadTemp;
	uploadTemp.Attach(createBuffer(pRenderer, D3D12_HEAP_TYPE_UPLOAD, totalBytes, D3D12_RESOURCE_STATE_GENERIC_READ));

	// map the upload resource
	BYTE* pBufData;
	hr = uploadTemp->Map(0, nullptr, (void**)& pBufData);
	if (FAILED(hr)) 
	{ 
		ErrorMsg("Mapping upload heap failed.");  
		return false;
	}

	// write data to upload resource
	for (UINT z = 0; z < desc.DepthOrArraySize; ++z)
	{
		BYTE const* pSource = (BYTE const*)data + z * slicePitch;
		for (UINT y = 0; y < desc.Height; ++y)
		{
			memcpy(pBufData, pSource, SIZE_T(desc.Width));
			pBufData += rowSize;
			pSource += rowPitch;
		}
	}

	// unmap 
	D3D12_RANGE written = { 0, (SIZE_T)totalBytes };
	uploadTemp->Unmap(0, &written);

	pRenderer->pGfxCmdList->CopyResource(pResource, uploadTemp);

	pRenderer->cmdSubmissions[pRenderer->currentSubmission].deferredFrees.push_back((ID3D12DeviceChild*)uploadTemp);

	return true;
}

void setDefaultPipelineState(Dx12Renderer* pRenderer, D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc)
{
	ZeroMemory(desc, sizeof(*desc));
	desc->BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc->SampleMask = ~0u;
	desc->RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc->RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc->RasterizerState.DepthClipEnable = TRUE;
	desc->IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc->PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc->NumRenderTargets = 1;
	desc->RTVFormats[0] = pRenderer->backbufFormat;
	desc->SampleDesc.Count = 1;
}