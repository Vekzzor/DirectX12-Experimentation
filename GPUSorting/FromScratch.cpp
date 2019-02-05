#include "pch.h"
#include "DX12Common.h"

using namespace Microsoft::WRL;

ComPtr<ID3D12Device4> Device;
ComPtr<ID3D12GraphicsCommandList3> CommandList;

ComPtr<ID3D12CommandQueue> CommandQueue;
ComPtr<ID3D12CommandAllocator> CommandAllocator;

ComPtr<ID3D12Fence1> Fence = nullptr;
int fenceValue = 0;
HANDLE EventHandle = nullptr;

ComPtr<ID3D12RootSignature> RootSignature;
ComPtr<ID3D12PipelineState> PipeLineState;

ComPtr<ID3D12DescriptorHeap> OutputHeap;
ComPtr<ID3D12DescriptorHeap> ReadbackHeap;

ComPtr<ID3D12Resource1> ConstantBufferResource;
ComPtr<ID3D12Resource1> VertexBufferResource;

D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

UINT outputBufferSize = 5;
ComPtr<ID3D12Resource1> outputBuffer;
ComPtr<ID3D12Resource1> readbackBuffer;

int threadCount = 2;
int values = threadCount * 16;

struct m_ConstantBuffer
{
	float colorChannel[4];
} m_ConstantBufferCPU;

struct float4
{
	float x;
	float filler[3];
};

void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore,
	D3D12_RESOURCE_STATES StateAfter);

void Present();
void CreateDevice();
void CreateCommandInterface();
void CreateFenceAndEvent();
void CreateRootSignature();
void CreateShadersAndPipeLineState();
void CreateConstantBufferResources();
void CreateTriangleData();
void CreateBuffers();
void WaitForGPU();

void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore,
	D3D12_RESOURCE_STATES StateAfter);

int main()
{
	CreateDevice();
	CreateCommandInterface();
	CreateFenceAndEvent();
	CreateRootSignature();
	CreateShadersAndPipeLineState();
	CreateBuffers();
	WaitForGPU();
	Present();
	WaitForGPU();


	return 0;
}

void Present()
{
	CommandAllocator->Reset();
	CommandList->Reset(CommandAllocator.Get(), PipeLineState.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = { OutputHeap.Get(), ReadbackHeap.Get() };

	CommandList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

	CommandList->SetComputeRootSignature(RootSignature.Get());

	//CommandList->SetComputeRootDescriptorTable(
	//	0, CBVHeap.Get()->GetGPUDescriptorHandleForHeapStart());
	CommandList->CopyResource(readbackBuffer.Get(), outputBuffer.Get());

	CommandList->Close();

	ID3D12CommandList* listsToExecute[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);

	D3D12_RANGE readbackBufferRange{ 0, outputBufferSize };
	FLOAT * pReadbackBufferData{};
	ThrowIfFailed(
		readbackBuffer->Map
		(
			0,
			&readbackBufferRange,
			reinterpret_cast<void**>(&pReadbackBufferData)
		)
	);

	D3D12_RANGE emptyRange{ 0, 0 };
	readbackBuffer->Unmap
	(
		0,
		&emptyRange
	);
}

void CreateDevice()
{
	ComPtr<ID3D12Debug> debugController;

	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}

	IDXGIFactory6* factory = nullptr;
	IDXGIAdapter1* adapter = nullptr;

	CreateDXGIFactory(IID_PPV_ARGS(&factory));
	//Loop through and find adapter
	for (UINT adapterIndex = 0;; ++adapterIndex)
	{
		adapter = nullptr;
		if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
		{
			break;
		}

		if (SUCCEEDED(
			D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
		{
			break;
		}

		SafeRelease(&adapter);
	}

	if (adapter)
	{
		HRESULT hr = S_OK;
		ThrowIfFailed(
			hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&Device)));

		SafeRelease(&adapter);
	}

	SafeRelease(&factory);
}

void CreateCommandInterface()
{
	D3D12_COMMAND_QUEUE_DESC cqd = {};
	ThrowIfFailed(Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&CommandQueue)));

	ThrowIfFailed(Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&CommandAllocator)));

	ThrowIfFailed(Device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		CommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&CommandList)));
	CommandList->Close();
}

void CreateFenceAndEvent()
{
	Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
	fenceValue = 1;

	EventHandle = CreateEvent(0, false, false, 0);
}

void CreateRootSignature()
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

	ComPtr<ID3DBlob> sBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sBlob, nullptr));

	ThrowIfFailed(Device->CreateRootSignature(
		0, sBlob->GetBufferPointer(), sBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
}

void CreateShadersAndPipeLineState()
{
	ID3DBlob* computeBlob;
	D3DCompileFromFile(
		L"ComputeShader2.hlsl", nullptr, nullptr, "CS_main", "cs_5_0", 0, 0, &computeBlob, nullptr);


	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
	cpsd.pRootSignature = RootSignature.Get();
	cpsd.CS.pShaderBytecode = computeBlob->GetBufferPointer();
	cpsd.CS.BytecodeLength = computeBlob->GetBufferSize();
	cpsd.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	cpsd.NodeMask = 0;

	Device->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&PipeLineState));
}


void CreateBuffers()
{
	D3D12_DESCRIPTOR_HEAP_DESC outputHeapDescriptorDesc = {};
	outputHeapDescriptorDesc.NumDescriptors = 1;
	outputHeapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	outputHeapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	Device->CreateDescriptorHeap(&outputHeapDescriptorDesc, IID_PPV_ARGS(&OutputHeap));

	/*D3D12_DESCRIPTOR_HEAP_DESC readbackHeapDescriptorDesc = {};
	readbackHeapDescriptorDesc.NumDescriptors = 1;
	readbackHeapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	readbackHeapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	Device->CreateDescriptorHeap(&readbackHeapDescriptorDesc, IID_PPV_ARGS(&ReadbackHeap));*/


	// The output buffer (created below) is on a default heap, so only the GPU can access it.
	std::vector<FLOAT> data;
	data.resize(values);
	const UINT dataSize = values * sizeof(float4);

	for (int i = 0; i < values; i++)
	{
		FLOAT test = (float)values - i;

		data.push_back(test);
		//arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
	}

	UINT cbSizeAligned = (sizeof(FLOAT) + 255) & ~255;

	D3D12_HEAP_PROPERTIES defaultHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT) };
	D3D12_RESOURCE_DESC outputBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) };
	ThrowIfFailed(Device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&outputBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&outputBuffer)));

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

	Device->CreateUnorderedAccessView(outputBuffer.Get(), nullptr, &UAVDesc,  
		OutputHeap->GetCPUDescriptorHandleForHeapStart());

	// The readback buffer (created below) is on a readback heap, so that the CPU can access it.

	D3D12_HEAP_PROPERTIES readbackHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK) };
	D3D12_RESOURCE_DESC readbackBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize) };
	ThrowIfFailed(Device->CreateCommittedResource(
		&readbackHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&readbackBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&outputBuffer)));

	/*Device->CreateUnorderedAccessView(readbackBuffer.Get(), nullptr, &UAVDesc,
		ReadbackHeap->GetCPUDescriptorHandleForHeapStart());*/

	void* dataBegin = nullptr;
	D3D12_RANGE range = { 0, 0 };
	outputBuffer->Map(0, &range, &dataBegin);
	memcpy(dataBegin, &data, sizeof(data));
	outputBuffer->Unmap(0, nullptr);

}

void WaitForGPU()
{
	const UINT64 fence = fenceValue;
	CommandQueue->Signal(Fence.Get(), fence);

	if (Fence->GetCompletedValue() < fence)
	{
		Fence->SetEventOnCompletion(fence, EventHandle);
		WaitForSingleObject(EventHandle, INFINITE);
	}

	fenceValue++;
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