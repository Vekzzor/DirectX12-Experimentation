#include "GPUComputing.h"
#include "pch.h"

GPUComputing::GPUComputing() {}

GPUComputing::~GPUComputing() {}

int GPUComputing::init(ID3D12Device4* device,
					   ID3DBlob* computeBlob,
					   ID3D12RootSignature* rootSignature)
{
	m_CreateCommandInterface(device);
	m_CreatePipeLineState(device, computeBlob, rootSignature);
	return 0;
}

void GPUComputing::PopulateComputeCommandQueue()
{
	m_pComputeAllocator->Reset();
	m_pComputeList->Reset(m_pComputeAllocator.Get(), m_pPipelineState.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_DescHeap};

	m_pComputeList->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

	m_pComputeList->SetComputeRootSignature(m_pSignature);

	m_pComputeList->SetComputeRootDescriptorTable(0,
												  m_DescHeap->GetGPUDescriptorHandleForHeapStart());

	m_pComputeList->Dispatch(5, 1, 1);
	m_pComputeList->Close();
}

void GPUComputing::m_CreateCommandInterface(ID3D12Device4* device)
{
	D3D12_COMMAND_QUEUE_DESC descComputeQueue = {};
	descComputeQueue.Flags					  = D3D12_COMMAND_QUEUE_FLAG_NONE;
	descComputeQueue.Type					  = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ThrowIfFailed(device->CreateCommandQueue(&descComputeQueue, IID_PPV_ARGS(&m_pComputeQueue)));
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
												 IID_PPV_ARGS(&m_pComputeAllocator)));
	ThrowIfFailed(device->CreateCommandList(0,
											D3D12_COMMAND_LIST_TYPE_COMPUTE,
											m_pComputeAllocator.Get(),
											nullptr,
											IID_PPV_ARGS(&m_pComputeList)));
	m_pComputeList->Close();

	m_pComputeAllocator->SetName(L"Compute Allocator");
	m_pComputeQueue->SetName(L"Compute Queue");
	m_pComputeList->SetName(L"Compute list");
}

void GPUComputing::m_CreatePipeLineState(ID3D12Device4* device,
										 ID3DBlob* computeBlob,
										 ID3D12RootSignature* rootSignature)
{
	m_pSignature = rootSignature;

	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsd = {};
	cpsd.pRootSignature					   = m_pSignature;
	cpsd.CS.pShaderBytecode				   = computeBlob->GetBufferPointer();
	cpsd.CS.BytecodeLength				   = computeBlob->GetBufferSize();
	cpsd.NodeMask						   = 0;
	ThrowIfFailed(device->CreateComputePipelineState(&cpsd, IID_PPV_ARGS(&m_pPipelineState)));
}
