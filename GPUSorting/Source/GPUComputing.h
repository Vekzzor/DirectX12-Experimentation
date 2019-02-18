#pragma once
class GPUComputing
{
public:
	GPUComputing();
	~GPUComputing();
	int init(ID3D12Device4* device, ID3DBlob* computeBlob, ID3D12RootSignature* rootSignature);

	ID3D12CommandQueue* getCommandQueue()
	{
		return m_pComputeQueue.Get();
	}
	ID3D12CommandAllocator* getCommandAllocator()
	{
		return m_pComputeAllocator.Get();
	}
	ID3D12GraphicsCommandList* getCommandList()
	{
		return m_pComputeList.Get();
	}
	ID3D12PipelineState* getPipelineState()
	{
		return m_pPipelineState.Get();
	}
	
	ID3D12RootSignature* RootSignature()
	{
		return m_pSignature;
	}
	void RootSignature(ID3D12RootSignature* signature)
	{
		m_pSignature = signature;
	}

	ID3D12DescriptorHeap* DescriptorHeap()
	{
		return m_DescHeap;
	}
	void DescriptorHeap(ID3D12DescriptorHeap* descHeap)
	{
		m_DescHeap = descHeap;
	}

	void PopulateComputeCommandQueue();

private:
	void m_CreateCommandInterface(ID3D12Device4* device);
	void m_CreatePipeLineState(ID3D12Device4* device,
							   ID3DBlob* computeBlob,
							   ID3D12RootSignature* rootSignature);

private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pComputeQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pComputeAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pComputeList;
	
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
	ID3D12RootSignature* m_pSignature = nullptr;
	ID3D12DescriptorHeap* m_DescHeap  = nullptr;
};