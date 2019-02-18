#pragma once

class Copying
{
public:
	Copying();
	~Copying();
	void init(ID3D12Device4* device);
	void PopulateComputeCommandQueue(ID3D12Resource1* src, int size, ID3D12Resource1** dst);
	void PopulateComputeCommandQueue(ID3D12Resource1* src, ID3D12Resource1* dst);

	ID3D12CommandQueue* getCommandQueue()
	{
		return m_pCopyQueue.Get();
	}
	ID3D12CommandAllocator* getCommandAllocator()
	{
		return m_pCopyAllocator.Get();
	}
	ID3D12GraphicsCommandList* getCommandList()
	{
		return m_pcopyList.Get();
	}

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCopyQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCopyAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pcopyList;
};
