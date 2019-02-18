#include "pch.h"
#include <chrono>
#include <iostream>
using namespace Microsoft::WRL;

ComPtr<ID3D12Device4> device;

const UINT bufferSize = 512;

ComPtr<ID3D12Resource1> bufferReadback;
ComPtr<ID3D12Resource1> inputBuffer;
ComPtr<ID3D12Resource1> uavBuffer;
ComPtr<ID3D12Resource1> srvBuffer;

ComPtr<ID3D12Fence1> Fence = nullptr;
int fenceValue			   = 0;
HANDLE EventHandle		   = nullptr;

UINT srvUavDescriptorSize;
ComPtr<ID3D12DescriptorHeap> srvUavHeapDescriptor;
ComPtr<ID3D12RootSignature> RootSignature;

ID3DBlob* computeBlob;

GPUComputing gpuComputing;
Copying copying;

void CreateDevice();
void CreateFenceAndEvent();
void CreateRootSignature(); //For Compute
void CreateShader(); //For Compute
void CreateBuffers();
void CreateSrvUavHeap(); //For Compute
void CreateResourceViews(); //For Shaders
void CreateData();
void WaitForGPU(ID3D12CommandQueue* queue);
void readBackData();

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//system("pause");
	CreateDevice();

	CreateFenceAndEvent();
	CreateRootSignature();

	CreateBuffers();
	CreateSrvUavHeap();
	CreateResourceViews();
	CreateData();
	CreateShader();

	copying.init(device.Get());
	gpuComputing.init(device.Get(), computeBlob, RootSignature.Get());
	gpuComputing.DescriptorHeap(srvUavHeapDescriptor.Get());

	//Copy upload data to uav
	ID3D12CommandList* copylistsToExecute[] = {copying.getCommandList()};
	{
		ID3D12Resource1* destBuffers[] = {uavBuffer.Get(), srvBuffer.Get()};
		copying.PopulateComputeCommandQueue(inputBuffer.Get(), ARRAYSIZE(destBuffers), destBuffers);
		copying.getCommandQueue()->ExecuteCommandLists(ARRAYSIZE(copylistsToExecute),
													   copylistsToExecute);
		WaitForGPU(copying.getCommandQueue());
	}

	//run computeShader
	{
		gpuComputing.PopulateComputeCommandQueue();
		ID3D12CommandList* listsToExecute[] = {gpuComputing.getCommandList()};
		gpuComputing.getCommandQueue()->ExecuteCommandLists(ARRAYSIZE(listsToExecute),
															listsToExecute);
		WaitForGPU(gpuComputing.getCommandQueue());
	}

	//Copy uav data to readback
	{
		copying.PopulateComputeCommandQueue(uavBuffer.Get(), bufferReadback.Get());
		copying.getCommandQueue()->ExecuteCommandLists(ARRAYSIZE(copylistsToExecute),
													   copylistsToExecute);
		WaitForGPU(copying.getCommandQueue());
	}

	readBackData();
	return 0;
}
//Dependencies: None
void CreateDevice()
{
#if _DEBUG
	ComPtr<ID3D12Debug> debugController;

	if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
#endif

	IDXGIFactory6* factory = nullptr;
	IDXGIAdapter1* adapter = nullptr;

	CreateDXGIFactory(IID_PPV_ARGS(&factory));
	//Loop through and find adapter
	for(UINT adapterIndex = 0;; ++adapterIndex)
	{
		adapter = nullptr;
		if(DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
		{
			break;
		}

		if(SUCCEEDED(
			   D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
		{
			break;
		}

		SafeRelease(&adapter);
	}

	if(adapter)
	{
		HRESULT hr = S_OK;
		ThrowIfFailed(
			hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device)));

		SafeRelease(&adapter);
	}

	SafeRelease(&factory);
}

//Dependencies: None
void CreateFenceAndEvent()
{
	device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
	fenceValue = 1;

	EventHandle = CreateEvent(0, false, false, 0);
}

//Dependencies: None
void CreateRootSignature()
{
	D3D12_DESCRIPTOR_RANGE dtRanges[2];
	dtRanges[0].RangeType						  = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	dtRanges[0].NumDescriptors					  = 1;
	dtRanges[0].BaseShaderRegister				  = 0;
	dtRanges[0].RegisterSpace					  = 0;
	dtRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	dtRanges[1].RangeType						  = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	dtRanges[1].NumDescriptors					  = 1;
	dtRanges[1].BaseShaderRegister				  = 0;
	dtRanges[1].RegisterSpace					  = 0;
	dtRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_DESCRIPTOR_TABLE dt;
	dt.NumDescriptorRanges = ARRAYSIZE(dtRanges);
	dt.pDescriptorRanges   = dtRanges;

	D3D12_ROOT_PARAMETER rootParam[1];
	rootParam[0].ParameterType	= D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[0].DescriptorTable  = dt;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	rsDesc.Flags			 = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	rsDesc.NumParameters	 = ARRAYSIZE(rootParam);
	rsDesc.pParameters		 = rootParam;
	rsDesc.NumStaticSamplers = 0;
	rsDesc.pStaticSamplers   = nullptr;

	ID3DBlob* sBlob;
	D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sBlob, nullptr);

	device->CreateRootSignature(
		0, sBlob->GetBufferPointer(), sBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature));
}

//Dependencies: None
void CreateShader()
{
	ThrowIfFailed(D3DCompileFromFile(L"Shaders/ComputeShader3.hlsl",
									 nullptr,
									 nullptr,
									 "CS_main",
									 "cs_5_1",
									 0,
									 0,
									 &computeBlob,
									 nullptr));
}

//Dependencies: dataSize
void CreateBuffers()
{
	D3D12_HEAP_PROPERTIES srvHeapProperties{CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)};
	D3D12_RESOURCE_DESC srvBufferDesc{CD3DX12_RESOURCE_DESC::Buffer(bufferSize)};
	srvBufferDesc.Width = sizeof(float) * bufferSize;
	srvBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommittedResource(&srvHeapProperties,
												  D3D12_HEAP_FLAG_NONE,
												  &srvBufferDesc,
												  D3D12_RESOURCE_STATE_COPY_DEST,
												  nullptr,
												  IID_PPV_ARGS(&srvBuffer)));

	srvBuffer->SetName(L"srv buffer");

	D3D12_HEAP_PROPERTIES uavHeapProperties{CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)};
	D3D12_RESOURCE_DESC uavBufferDesc{CD3DX12_RESOURCE_DESC::Buffer(bufferSize)};
	uavBufferDesc.Width = sizeof(float) * bufferSize;
	uavBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ThrowIfFailed(device->CreateCommittedResource(&uavHeapProperties,
												  D3D12_HEAP_FLAG_NONE,
												  &uavBufferDesc,
												  D3D12_RESOURCE_STATE_COPY_DEST,
												  nullptr,
												  IID_PPV_ARGS(&uavBuffer)));

	uavBuffer->SetName(L"uav buffer");

	D3D12_HEAP_PROPERTIES defaultHeapProperties{CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)};
	D3D12_RESOURCE_DESC outputBufferDesc{CD3DX12_RESOURCE_DESC::Buffer(bufferSize)};
	outputBufferDesc.Width = sizeof(float) * bufferSize;
	ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties,
												  D3D12_HEAP_FLAG_NONE,
												  &outputBufferDesc,
												  D3D12_RESOURCE_STATE_GENERIC_READ,
												  nullptr,
												  IID_PPV_ARGS(&inputBuffer)));

	inputBuffer->SetName(L"upload buffer");

	D3D12_HEAP_PROPERTIES const heapPropertiesReadback = {/*Type*/ D3D12_HEAP_TYPE_READBACK
														  /*CPUPageProperty*/,
														  D3D12_CPU_PAGE_PROPERTY_UNKNOWN
														  /*MemoryPoolPreference*/,
														  D3D12_MEMORY_POOL_UNKNOWN
														  /*CreationNodeMask*/,
														  0
														  /*VisibleNodeMask*/,
														  0};

	D3D12_RESOURCE_DESC buffersDesc = {};
	buffersDesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
	buffersDesc.Alignment			= 0;
	buffersDesc.Width				= sizeof(float) * bufferSize;
	buffersDesc.Height				= 1;
	buffersDesc.DepthOrArraySize	= 1;
	buffersDesc.MipLevels			= 1;
	buffersDesc.Format				= DXGI_FORMAT_UNKNOWN;
	buffersDesc.SampleDesc.Count	= 1;
	buffersDesc.SampleDesc.Quality  = 0;
	buffersDesc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	buffersDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;

	device->CreateCommittedResource(&heapPropertiesReadback,
									D3D12_HEAP_FLAG_NONE,
									&buffersDesc,
									D3D12_RESOURCE_STATE_COPY_DEST,
									nullptr,
									IID_PPV_ARGS(&bufferReadback));

	bufferReadback->SetName(L"readback buffer");
}

//Dependencies: None* (based on root signatures descriptor type ammount maybe?)
void CreateSrvUavHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
	srvUavHeapDesc.NumDescriptors			  = 2;
	srvUavHeapDesc.Type						  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavHeapDesc.Flags					  = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(
		device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&srvUavHeapDescriptor)));

	srvUavHeapDescriptor->SetName(L"srvUav heap");
	srvUavDescriptorSize =
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

//Dependencies: HeapDescriptor, Resources(Buffers)
void CreateResourceViews()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format							 = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension					 = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement				 = 0;
	uavDesc.Buffer.NumElements				 = bufferSize;
	uavDesc.Buffer.StructureByteStride		 = sizeof(float);
	uavDesc.Buffer.CounterOffsetInBytes		 = 0;
	uavDesc.Buffer.Flags					 = D3D12_BUFFER_UAV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(
		srvUavHeapDescriptor->GetCPUDescriptorHandleForHeapStart(), 0, srvUavDescriptorSize);

	device->CreateUnorderedAccessView(uavBuffer.Get(), nullptr, &uavDesc, uavHandle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping			= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format							= DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement				= 0;
	srvDesc.Buffer.NumElements				= bufferSize;
	srvDesc.Buffer.StructureByteStride		= sizeof(float);
	srvDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
		srvUavHeapDescriptor->GetCPUDescriptorHandleForHeapStart(),
		1 /*offsetInDescriptors*/,
		srvUavDescriptorSize /*descriptorIncrementSize*/);

	device->CreateShaderResourceView(srvBuffer.Get(), &srvDesc, srvHandle);
}

//Dependencies: dataSize, CPU-Readable Resources(buffers)
void CreateData()
{
	float data[bufferSize];
	const UINT dataSize = ARRAYSIZE(data);
	std::cout << "Initial Data:\n";
	for(int i = 0; i < bufferSize; i++)
	{
		//data[i] = (float)outputBufferSize - i;
		data[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
		std::cout << data[i] << std::endl;
		//arr2[i] = ((float)rand()) / ((float)RAND_MAX) * 99 + 1;
	}
	UINT cbSizeAligned = (sizeof(float) + 255) & ~255;

	void* dataBegin   = nullptr;
	D3D12_RANGE range = {0, 0};
	inputBuffer->Map(0, &range, &dataBegin);
	memcpy(dataBegin, &data, sizeof(data));
	inputBuffer->Unmap(0, nullptr);
}

//Dependencies: Fence, CommandQueue
void WaitForGPU(ID3D12CommandQueue* queue)
{
	const UINT64 fence = fenceValue;
	queue->Signal(Fence.Get(), fence);

	if(Fence->GetCompletedValue() < fence)
	{
		Fence->SetEventOnCompletion(fence, EventHandle);
		WaitForSingleObject(EventHandle, INFINITE);
	}

	fenceValue++;
}

//Dependencies: dataSize, CPU-Readable Resources(buffers)
void readBackData()
{
	float temp;
	bool sorted = true;
	float* bufferdata;
	D3D12_RANGE readRange = {0, bufferSize * sizeof(float)};
	bufferReadback->Map(0, &readRange, reinterpret_cast<void**>(&bufferdata));
	std::cout << "\nParsed Data:\n";
	temp = bufferdata[0];
	for(int i = 0; i < bufferSize; i++)
	{
		std::cout << bufferdata[i] << std::endl;
		if(temp > bufferdata[i])
			sorted = false;


		if (!sorted)
		{
			std::cout << "FAKE" << std::endl;
		}
		temp = bufferdata[i];
	}
	if(!sorted)
	{
		std::cout << "SORT FAILED" << std::endl;
	}
	bufferReadback->Unmap(0, nullptr);
}