#include "Copying.h"
#include "pch.h"

Copying::Copying() {}

Copying::~Copying() {}

void Copying::init(ID3D12Device4* device)
{
	D3D12_COMMAND_QUEUE_DESC descCopyQueue = {};
	descCopyQueue.Flags					   = D3D12_COMMAND_QUEUE_FLAG_NONE;
	descCopyQueue.Type					   = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(device->CreateCommandQueue(&descCopyQueue, IID_PPV_ARGS(&m_pCopyQueue)));
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
												 IID_PPV_ARGS(&m_pCopyAllocator)));
	ThrowIfFailed(device->CreateCommandList(0,
											D3D12_COMMAND_LIST_TYPE_COPY,
											m_pCopyAllocator.Get(),
											nullptr,
											IID_PPV_ARGS(&m_pcopyList)));
	m_pcopyList->Close();

	m_pCopyAllocator->SetName(L"Copy Allocator");
	m_pCopyQueue->SetName(L"Copy Queue");
	m_pcopyList->SetName(L"Copy list");
}

void Copying::PopulateComputeCommandQueue(ID3D12Resource1* src, int size, ID3D12Resource1** dst)
{
	m_pCopyAllocator->Reset();
	m_pcopyList->Reset(m_pCopyAllocator.Get(), nullptr);
	for (int i = 0; i < size; i++)
		m_pcopyList->CopyResource(dst[i], src);
	m_pcopyList->Close();
}

void Copying::PopulateComputeCommandQueue(ID3D12Resource1* src, ID3D12Resource1* dst)
{
	m_pCopyAllocator->Reset();
	m_pcopyList->Reset(m_pCopyAllocator.Get(), nullptr);
	m_pcopyList->CopyResource(dst, src);
	m_pcopyList->Close();
}
