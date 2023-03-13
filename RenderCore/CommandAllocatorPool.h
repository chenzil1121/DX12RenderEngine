#pragma once
#include"Utility.h"
/*
* CommandAllocator���Ա����CommandList���ã���˿��Դ���һ�����õ�Pool
* ÿ��CommandAllocator������ŵ�m_AllocatorPool��ͨ��DiscardAllocator�������õ�CommandAllocator����ӦFenceValue����m_ReadyAllocators
* ͨ��FenceValueȷ��ִ����CommandAllocator��¼������󣬿�����RequestAllocator�и���CommandAllocator
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