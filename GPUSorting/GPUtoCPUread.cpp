#include "pch.h"
#include "DX12Common.h"
#include <iostream>
using namespace Microsoft::WRL;

ComPtr<ID3D12Device4> device;

ComPtr<ID3D12CommandQueue> copyQueue;
ComPtr<ID3D12CommandAllocator> copyAllocator;
ComPtr<ID3D12GraphicsCommandList> copyList;

ComPtr<ID3D12CommandQueue> computeQueue;
ComPtr<ID3D12CommandAllocator> computeAllocator;
ComPtr<ID3D12GraphicsCommandList> computeList;

ComPtr<ID3D12Resource> bufferReadback;
const UINT outputBufferSize = 5;
ComPtr<ID3D12Resource1> outputBuffer;

ComPtr<ID3D12Resource1> uavBuffer;
ComPtr<ID3D12Resource1> srvBuffer;

ComPtr<ID3D12Fence1> Fence = nullptr;
int fenceValue = 0;
HANDLE EventHandle = nullptr;

ComPtr<ID3D12DescriptorHeap> srvUavHeap;
ComPtr<ID3D12DescriptorHeap> uploadHeap;
UINT srvUavDescriptorSize;

ComPtr<ID3D12RootSignature> outputRootSignature;
ComPtr<ID3D12PipelineState> outputPipeLineState;

void CreateDevice();
void CreateCommandInterface();
void CreateFenceAndEvent();
void CreateRootSignature(); //For Compute
void CreateShadersAndPipeLineState(); //For Compute
void CreateBuffers();
void CreateUAV(); //For Compute
void CreateData();
void WaitForGPU(ID3D12CommandQueue* queue);
int main()
{
	CreateDevice();
	CreateCommandInterface();
	CreateFenceAndEvent();
	CreateRootSignature();
	CreateShadersAndPipeLineState();
	CreateBuffers();
	CreateUAV();
	CreateData();

	//Copy upload data to uav
	copyAllocator->Reset();
	copyList->Reset(copyAllocator.Get(), nullptr);
	copyList->CopyResource(uavBuffer.Get(), outputBuffer.Get());
	copyList->CopyResource(srvBuffer.Get(), outputBuffer.Get());
	copyList->Close();

	ID3D12CommandList* copylistsToExecute[] = { copyList.Get() };
	copyQueue->ExecuteCommandLists(ARRAYSIZE(copylistsToExecute), copylistsToExecute);
	WaitForGPU(copyQueue.Get());

	//run computeShader
	{
		computeAllocator->Reset();
		computeList->Reset(computeAllocator.Get(), outputPipeLineState.Get());

		ID3D12DescriptorHeap* descriptorHeaps[] = { srvUavHeap.Get() };

		computeList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

		computeList->SetComputeRootSignature(outputRootSignature.Get());

		computeList->SetComputeRootDescriptorTable(
			0, srvUavHeap->GetGPUDescriptorHandleForHeapStart());

		computeList->Dispatch(5, 1, 1);
		computeList->Close();

		ID3D12CommandList* listsToExecute[] = { computeList.Get() };
		computeQueue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);

		WaitForGPU(computeQueue.Get());
	}

	//Copy uav data to readback
	copyAllocator->Reset();
	copyList->Reset(copyAllocator.Get(), nullptr);
	copyList->CopyResource(bufferReadback.Get(), uavBuffer.Get());
	copyList->Close();

	ID3D12CommandList* copylistsToExecute2[] = { copyList.Get() };
	copyQueue->ExecuteCommandLists(ARRAYSIZE(copylistsToExecute2), copylistsToExecute2);

	WaitForGPU(copyQueue.Get());

	unsigned int* bufferdata;
	D3D12_RANGE readRange = { 0, outputBufferSize * sizeof(unsigned int) };
	bufferReadback->Map(0, &readRange, reinterpret_cast<void**>(&bufferdata));
	std::cout << "\nParsed Data:\n";
	for (int i = 0; i < outputBufferSize; i++)
	{
		std::cout << bufferdata[i] << std::endl;
	}
	bufferReadback->Unmap(0, nullptr);
	return 0;
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
			hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

		SafeRelease(&adapter);
	}

	SafeRelease(&factory);
}

void CreateCommandInterface()
{
	D3D12_COMMAND_QUEUE_DESC descCopyQueue = {};
	descCopyQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	descCopyQueue.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(device->CreateCommandQueue(&descCopyQueue, IID_PPV_ARGS(&copyQueue)));
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copyAllocator)));
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copyAllocator.Get(), nullptr, IID_PPV_ARGS(&copyList)));
	copyList->SetName(L"copy list");
	copyList->Close();

	D3D12_COMMAND_QUEUE_DESC descComputeQueue = {};
	descComputeQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	descComputeQueue.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	device->CreateCommandQueue(&descComputeQueue, IID_PPV_ARGS(&computeQueue));
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&computeAllocator));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, computeAllocator.Get(), nullptr, IID_PPV_ARGS(&computeList));
	computeList->SetName(L"compute list");
	computeList->Close();
}

void CreateFenceAndEvent()
{
	device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
	fenceValue = 1;

	EventHandle = CreateEvent(0, false, false, 0);
}

void CreateRootSignature()
{
	D3D12_DESCRIPTOR_RANGE dtRanges[2];
	dtRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	dtRanges[0].NumDescriptors = 1;
	dtRanges[0].BaseShaderRegister = 0;
	dtRanges[0].RegisterSpace = 0;
	dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	dtRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[1].NumDescriptors = 1;
	dtRanges[1].BaseShaderRegister = 0;
	dtRanges[1].RegisterSpace = 0;
	dtRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

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
	D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sBlob, nullptr);

	device->CreateRootSignature(
		0, sBlob->GetBufferPointer(), sBlob->GetBufferSize(), IID_PPV_ARGS(&outputRootSignature));
}

void CreateShadersAndPipeLineState()
{
	ID3DBlob* computeBlob;
	D3DCompileFromFile(
		L"ComputeShader2.hlsl", nullptr, nullptr, "CS_main", "cs_5_1", 0, 0, &computeBlob, nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
	cpsd.pRootSignature = outputRootSignature.Get();
	cpsd.CS.pShaderBytecode = computeBlob->GetBufferPointer();
	cpsd.CS.BytecodeLength = computeBlob->GetBufferSize();
	cpsd.NodeMask = 0;

	device->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&outputPipeLineState));
}

void CreateBuffers()
{
	D3D12_HEAP_PROPERTIES srvHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT) };
	D3D12_RESOURCE_DESC srvBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize) };
	srvBufferDesc.Width = sizeof(unsigned int) * outputBufferSize;
	srvBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommittedResource(
		&srvHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&srvBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&srvBuffer)));

	srvBuffer->SetName(L"srv buffer");


	D3D12_HEAP_PROPERTIES uavHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT) };
	D3D12_RESOURCE_DESC uavBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize) };
	uavBufferDesc.Width = sizeof(unsigned int) * outputBufferSize;
	uavBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(device->CreateCommittedResource(
		&uavHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uavBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&uavBuffer)));

	uavBuffer->SetName(L"uav buffer");

	D3D12_HEAP_PROPERTIES defaultHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD) };
	D3D12_RESOURCE_DESC outputBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize) };
	outputBufferDesc.Width = sizeof(unsigned int) * outputBufferSize;
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&outputBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&outputBuffer)));

	outputBuffer->SetName(L"output buffer");

	D3D12_HEAP_PROPERTIES const heapPropertiesReadback =
	{
		/*Type*/                    D3D12_HEAP_TYPE_READBACK
		/*CPUPageProperty*/         ,D3D12_CPU_PAGE_PROPERTY_UNKNOWN
		/*MemoryPoolPreference*/    ,D3D12_MEMORY_POOL_UNKNOWN
		/*CreationNodeMask*/        ,0
		/*VisibleNodeMask*/         ,0
	};

	D3D12_RESOURCE_DESC buffersDesc = {};
	buffersDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffersDesc.Alignment = 0;
	buffersDesc.Width = sizeof(unsigned int) * outputBufferSize;
	buffersDesc.Height = 1;
	buffersDesc.DepthOrArraySize = 1;
	buffersDesc.MipLevels = 1;
	buffersDesc.Format = DXGI_FORMAT_UNKNOWN;
	buffersDesc.SampleDesc.Count = 1;
	buffersDesc.SampleDesc.Quality = 0;
	buffersDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	buffersDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	device->CreateCommittedResource(
		&heapPropertiesReadback,
		D3D12_HEAP_FLAG_NONE,
		&buffersDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&bufferReadback));

	bufferReadback->SetName(L"readback buffer");
}

void CreateUAV()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
	srvUavHeapDesc.NumDescriptors = 2;
	srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&srvUavHeap)));

	srvUavHeap->SetName(L"srvUav heap");
	srvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = outputBufferSize;
	uavDesc.Buffer.StructureByteStride = sizeof(unsigned int);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 0, srvUavDescriptorSize);

	device->CreateUnorderedAccessView(uavBuffer.Get(), nullptr, &uavDesc, uavHandle);


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = outputBufferSize;
	srvDesc.Buffer.StructureByteStride = sizeof(unsigned int);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1 /*offsetInDescriptors*/, srvUavDescriptorSize /*descriptorIncrementSize*/);


	device->CreateShaderResourceView(srvBuffer.Get(), &srvDesc, srvHandle);
}

void CreateData()
{
	unsigned int data[outputBufferSize];
	const UINT dataSize = outputBufferSize * sizeof(unsigned int);
	std::cout << "Initial Data:\n";
	for (int i = 0; i < outputBufferSize; i++)
	{
		data[i] = (float)outputBufferSize - i;
		std::cout << data[i] << std::endl;
		//arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
	}
	UINT cbSizeAligned = (sizeof(unsigned int) + 255) & ~255;

	void* dataBegin = nullptr;
	D3D12_RANGE range = { 0, 0 };
	outputBuffer->Map(0, &range, &dataBegin);
	memcpy(dataBegin, &data, sizeof(data));
	outputBuffer->Unmap(0, nullptr);
}

void WaitForGPU(ID3D12CommandQueue* queue)
{
	const UINT64 fence = fenceValue;
	queue->Signal(Fence.Get(), fence);

	if (Fence->GetCompletedValue() < fence)
	{
		Fence->SetEventOnCompletion(fence, EventHandle);
		WaitForSingleObject(EventHandle, INFINITE);
	}

	fenceValue++;
}


