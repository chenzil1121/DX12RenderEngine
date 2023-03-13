#pragma once
#include"Utility.h"
/*
* CommandAllocator可以被多个CommandList复用，因此可以创建一个复用的Pool
* 每个CommandAllocator申请后存放到m_AllocatorPool，通过DiscardAllocator将待重用的CommandAllocator和相应FenceValue加入m_ReadyAllocators
* 通过FenceValue确认执行完CommandAllocator记录的命令后，可以在RequestAllocator中复用CommandAllocator
*/
class CommandAllocatorPool
{
public:
	CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE Type);
	~CommandAllocatorPool();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	ID3D12CommandAllocator* RequestAllocator(uint64_t CompleteFenceValue);
	void DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator);

	inline size_t Size() { return 1; }

private:
	const D3D12_COMMAND_LIST_TYPE m_CommandListType;

	ID3D12Device* m_Device;
	std::vector<ID3D12CommandAllocator*>m_AllocatorPool;
	std::queue<std::pair<uint64_t, ID3D12CommandAllocator*>> m_ReadyAllocators;
};