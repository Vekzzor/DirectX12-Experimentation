#pragma once

// DirectX
#include "pch.h"
#include "DX12Common.h"


using namespace Microsoft::WRL;

ComPtr<ID3D12Device> g_cDevice;
ComPtr<ID3D12CommandQueue> g_cCommandQueue;
ComPtr<ID3D12CommandAllocator> g_cCommandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_cCommandList;
ComPtr<IDXGISwapChain3> g_cSwapChain;

ComPtr<ID3D12RootSignature> g_cRootSignature;
ComPtr<ID3D12PipelineState> g_cPipeLineState;

ComPtr<ID3D12Fence1> g_cFence;
int g_fenceValue = 0;
HANDLE g_EventHandle = nullptr;

ComPtr<ID3D12DescriptorHeap> g_cCBVHeap;
ComPtr<ID3D12DescriptorHeap> g_cComputeHeap;
ComPtr<ID3D12DescriptorHeap> g_cUploadComputeHeap;

ComPtr<ID3D12Resource1> g_cConstantBufferResource;
ComPtr<ID3D12Resource1> g_cComputeResource;
ComPtr<ID3D12Resource1> g_cUploadComputeResource;

UINT g_rtvDescriptorSize;
UINT g_srvUavDescriptorSize;

float* arr = nullptr;
float* arr2 = nullptr;
int threadCount = 2;
int values = threadCount * 16;

struct ConstantBuffer
{
	float x;
	float filler[3];
};

struct float4
{
	float x;
	float filler[3];
};

IDXGIAdapter1* _getHardwareAdapter(IDXGIFactory2* _pFactory);
void WaitForGPU();
void runCompute();
int init();
void createDat();
void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore,
	D3D12_RESOURCE_STATES StateAfter);


int main()
{
	init();

	//g_cCommandAllocator->Reset();
	//g_cCommandList->Reset(g_cCommandAllocator.Get(), g_cPipeLineState.Get());


	///*SetResourceTransitionBarrier(g_cCommandList.Get(),
	//	g_cComputeResource.Get(),
	//	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
	//	D3D12_RESOURCE_STATE_UNORDERED_ACCESS);*/

	//g_cCommandList->SetPipelineState(g_cPipeLineState.Get());
	//g_cCommandList->SetComputeRootSignature(g_cRootSignature.Get());

	//ID3D12DescriptorHeap* descriptorHeaps[] = { g_cComputeHeap.Get() };
	//g_cCommandList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);


	//g_cCommandList->SetComputeRootDescriptorTable(
	//	0, g_cComputeHeap.Get()->GetGPUDescriptorHandleForHeapStart());

	//g_cCommandList->Dispatch(threadCount, 0, 0);

	///*SetResourceTransitionBarrier(g_cCommandList.Get(),
	//	g_cComputeResource.Get(),
	//	D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	//	D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);*/

	//g_cCommandList->Close();
	//ID3D12CommandList* listsToExecute[] = { g_cCommandList.Get() };
	//g_cCommandQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);
	runCompute();
	WaitForGPU();
	return 0;
}

void runCompute()
{
	ID3D12Resource *pUavResource;
	pUavResource = g_cComputeResource.Get();
	g_cCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pUavResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	g_cCommandList->SetPipelineState(g_cPipeLineState.Get());
	g_cCommandList->SetComputeRootSignature(g_cRootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { g_cComputeHeap.Get() };
	g_cCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(g_cComputeHeap->GetGPUDescriptorHandleForHeapStart(), 0, g_srvUavDescriptorSize);

	g_cCommandList->SetComputeRootDescriptorTable(0, uavHandle);

	g_cCommandList->Dispatch(1, 1, 1);

	g_cCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

int init()
{
	ComPtr<ID3D12Debug> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	// Create factory controller
	ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory)));


	// Create device
	{
		ComPtr<IDXGIAdapter1> adapter = _getHardwareAdapter(dxgiFactory.Get());

		ThrowIfFailed(
			D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_cDevice)));
	}

	// Command Queue
	{
		D3D12_COMMAND_QUEUE_DESC commandDesc;
		commandDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		commandDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		commandDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		commandDesc.NodeMask = 0;

		ThrowIfFailed(g_cDevice->CreateCommandQueue(&commandDesc, IID_PPV_ARGS(&g_cCommandQueue)));

		D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
		srvUavHeapDesc.NumDescriptors = 1;
		srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(g_cDevice->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&g_cComputeHeap)));

		g_rtvDescriptorSize = g_cDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		g_srvUavDescriptorSize = g_cDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		ThrowIfFailed(g_cDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
			IID_PPV_ARGS(&g_cCommandAllocator)));




		ThrowIfFailed(g_cDevice->CreateCommandList(0,
			D3D12_COMMAND_LIST_TYPE_COMPUTE,
			g_cCommandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&g_cCommandList)));
	}

	// Fence
	{
		ThrowIfFailed(g_cDevice->CreateFence(g_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_cFence)));
		g_fenceValue = 1;

		g_EventHandle = CreateEvent(0, false, false, 0);
	}

	// Root Signature
	{
		D3D12_DESCRIPTOR_RANGE dtRanges[1];
		dtRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		dtRanges[0].NumDescriptors = 1;
		dtRanges[0].BaseShaderRegister = 0;
		dtRanges[0].RegisterSpace = 0;
		dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_DESCRIPTOR_TABLE dt;
		dt.NumDescriptorRanges = ARRAYSIZE(dtRanges);
		dt.pDescriptorRanges = dtRanges;

		D3D12_ROOT_PARAMETER rootParam[1];
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable = dt;
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rsDesc;
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rsDesc.NumParameters = ARRAYSIZE(rootParam);
		rsDesc.pParameters = rootParam;
		rsDesc.NumStaticSamplers = 0;
		rsDesc.pStaticSamplers = nullptr;

		ID3DBlob* sBlob;
		ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sBlob, nullptr));

		ThrowIfFailed(g_cDevice->CreateRootSignature(
			0, sBlob->GetBufferPointer(), sBlob->GetBufferSize(), IID_PPV_ARGS(&g_cRootSignature)));
	}

	//Compute Shaders & Pipeline State
	{
		ID3DBlob* computeBlob;
		ThrowIfFailed(D3DCompileFromFile(L"ComputeShader2.hlsl", nullptr, nullptr, "CS_main", "cs_5_1", 0, 0, &computeBlob, nullptr));

		D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
		cpsd.pRootSignature = g_cRootSignature.Get();
		cpsd.CS.pShaderBytecode = computeBlob->GetBufferPointer();
		cpsd.CS.BytecodeLength = computeBlob->GetBufferSize();
		cpsd.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		cpsd.NodeMask = 0;

		ThrowIfFailed(g_cDevice->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&g_cPipeLineState)));
	}

	//Constant Buffer Resources
	/*{
		D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
		heapDescriptorDesc.NumDescriptors = 1;
		heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		g_cDevice->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&g_cCBVHeap));

		UINT cbSizeAligned = (sizeof(ConstantBuffer) + 255) & ~255;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = cbSizeAligned;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		g_cDevice->CreateCommittedResource(&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&g_cConstantBufferResource));

		g_cConstantBufferResource->SetName(L"cb heap");

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = g_cConstantBufferResource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = cbSizeAligned;

		g_cDevice->CreateConstantBufferView(&cbvDesc,
			g_cCBVHeap->GetCPUDescriptorHandleForHeapStart());
	}*/

	//Input Data
	{
		std::vector<float4> data;
		data.resize(values);
		const UINT dataSize = values * sizeof(float4);

		for (int i = 0; i < values; i++)
		{
			float4 test;
			test.x = (float)values - i;

			data.push_back(test);
			//arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
		}

		UINT cbSizeAligned = (sizeof(float4) + 255) & ~255;

		createDat();
		D3D12_HEAP_PROPERTIES hp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);


		D3D12_HEAP_PROPERTIES uhp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);


		D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		D3D12_RESOURCE_DESC urd = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

		g_cDevice->CreateCommittedResource(&hp,
			D3D12_HEAP_FLAG_NONE,
			&rd,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&g_cComputeResource));

		g_cDevice->CreateCommittedResource(&uhp,
			D3D12_HEAP_FLAG_NONE,
			&urd,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&g_cUploadComputeResource));

		g_cComputeResource->SetName(L"cb heap");
		g_cUploadComputeResource->SetName(L"ucb heap");

		D3D12_SUBRESOURCE_DATA vectorData = {};
		vectorData.pData = reinterpret_cast<UINT8*>(&data[0]);
		vectorData.RowPitch = dataSize;
		vectorData.SlicePitch = vectorData.RowPitch;
		UpdateSubresources<1>(g_cCommandList.Get(), g_cComputeResource.Get(), g_cUploadComputeResource.Get(), 0, 0, 1, &vectorData);
		g_cCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(g_cComputeResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = values;
		uavDesc.Buffer.StructureByteStride = sizeof(float4);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle0(g_cComputeHeap->GetCPUDescriptorHandleForHeapStart(), 0, g_srvUavDescriptorSize);
		g_cDevice->CreateUnorderedAccessView(g_cComputeResource.Get(), nullptr, &uavDesc, uavHandle0);


		/*void* dataBegin = nullptr;
		D3D12_RANGE range = { 0, 0 };
		g_cComputeResource->Map(0, &range, &dataBegin);
		memcpy(dataBegin, arr, sizeof(arr));
		g_cComputeResource->Unmap(0, nullptr);*/
	}


	g_cCommandList->Close();
	ID3D12CommandList* listsToExecute[] = { g_cCommandList.Get() };
	g_cCommandQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);


	WaitForGPU();

	return 0;
}



void createDat()
{
	arr = new float[values];//{ 5, 2, 7, 3, 9, 1 };
	arr2 = new float[values];
	for (int i = 0; i < values; i++)
	{
		arr[i] = (float)values - i;
		arr2[i] = (float)values - i;
		//arr[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
		//arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
	}
}




void WaitForGPU()
{
	const UINT64 fence = g_fenceValue;
	g_cCommandQueue->Signal(g_cFence.Get(), fence);

	if (g_cFence->GetCompletedValue() < fence)
	{
		g_cFence->SetEventOnCompletion(fence, g_EventHandle);
		WaitForSingleObject(g_EventHandle, INFINITE);
	}

	g_fenceValue++;
}

void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore,
	D3D12_RESOURCE_STATES StateAfter)
{
	D3D12_RESOURCE_BARRIER barrierDesc = {};

	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = resource;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = StateBefore;
	barrierDesc.Transition.StateAfter = StateAfter;

	commandList->ResourceBarrier(1, &barrierDesc);
}

IDXGIAdapter1* _getHardwareAdapter(IDXGIFactory2* _pFactory)
{
	Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter = nullptr;

	for (UINT adapterIndex = 0;
		_pFactory->EnumAdapters1(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND;
		adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 adaptDesc;
		pAdapter->GetDesc1(&adaptDesc);
		printf("%ls\n", adaptDesc.Description);
		if (adaptDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(
			pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	return pAdapter.Detach();
}