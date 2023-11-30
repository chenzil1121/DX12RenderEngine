#pragma once
#include"Utility.h"
#include"CommandAllocatorPool.h"
/*
* 利用CommandListManager来管理程序中所有的CommandQueue、CommandAllocator。CommandList则由外部模块输入指针进行操作
* 由于CommandListManager对象先于m_Device被定义，所以需要一系列Create方法延后m_Device的正式初始化
* 单独写Shutdown的好处在于，可以手动重置相关类，而不是只能在析构时候释放占有资源
*/
class CommandQueue
{
public:
	CommandQueue(D3D12_COMMAND_LIST_TYPE Type);
	~CommandQueue();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	inline bool IsReady()
	{
		return m_CommandQueue != nullptr;
	}
	ID3D12CommandQueue* GetCommandQueue() 
	{
		return m_CommandQueue;
	}
	uint64_t GetLastCompletedFenceValue()
	{
		return m_LastCompletedFenceValue;
	}
	ID3D12CommandAllocator* RequestAllocator();
	void DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator);

	uint64_t ExecuteCommandList(ID3D12CommandList* List);
	void WaitForFence(uint64_t FenceValue);
	uint64_t IncrementFence(void);
	void WaitForIdle(void) { WaitForFence(IncrementFence()); }
	bool IsFenceComplete(uint64_t FenceValue);

private:
	ID3D12CommandQueue* m_CommandQueue;
	const D3D12_COMMAND_LIST_TYPE m_Type;

	CommandAllocatorPool m_AllocatorPool;

	ID3D12Fence* m_pFence;
	uint64_t m_NextFenceValue;
	uint64_t m_LastCompletedFenceValue;
	HANDLE m_FenceEventHandle;
};

class CommandListManager
{
public:
	CommandListManager();
	~CommandListManager();

	void Create(ID3D12Device* pDevice);
	void Shutdown();

	CommandQueue& GetGraphicsQueue(void) { return m_GraphicsQueue; }
	CommandQueue& GetComputeQueue(void) { return m_ComputeQueue; }
	CommandQueue& GetCopyQueue(void) { return m_CopyQueue; }

	CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE Type)
	{
		switch (Type)
		{
		case D3D12_COMMAND_LIST_TYPE_COMPUTE:
			return m_ComputeQueue;
		case D3D12_COMMAND_LIST_TYPE_COPY:
			return m_CopyQueue;
		default:
			return m_GraphicsQueue;
		}
	}

	void CreateNewCommandList(
		D3D12_COMMAND_LIST_TYPE Type,
		ID3D12GraphicsCommandList4** List,
		ID3D12CommandAllocator** Allocator);

	void IdleGPU()
	{
		m_GraphicsQueue.WaitForIdle();
		m_ComputeQueue.WaitForIdle();
		m_CopyQueue.WaitForIdle();
	}

private:
	ID3D12Device* m_Device;

	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;
};