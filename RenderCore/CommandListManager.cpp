#include"CommandListManager.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE Type):
	m_Type(Type),
	m_CommandQueue(nullptr),
	m_pFence(nullptr),
	m_NextFenceValue((uint64_t)1),
	m_LastCompletedFenceValue((uint64_t)0),
	m_AllocatorPool(Type)
{
}

CommandQueue::~CommandQueue()
{
	Shutdown();
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
	ASSERT(pDevice != nullptr);
	ASSERT(!IsReady());
	
	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Type = m_Type;
	pDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&m_CommandQueue));
	m_CommandQueue->SetName(L"CommandListManager::m_CommandQueue");

	ASSERT_SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_pFence->SetName(L"CommandListManager::m_pFence");

	m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
	ASSERT(m_FenceEventHandle != NULL);

	m_AllocatorPool.Create(pDevice);

	ASSERT(IsReady());
}

void CommandQueue::Shutdown()
{
	if (m_CommandQueue == nullptr)
		return;

	m_AllocatorPool.Shutdown();

	CloseHandle(m_FenceEventHandle);

	m_CommandQueue->Release();
	m_CommandQueue = nullptr;

	m_pFence->Release();
	m_pFence = nullptr;
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
	uint64_t CompletedFence = m_pFence->GetCompletedValue();
	return m_AllocatorPool.RequestAllocator(CompletedFence);
}

void CommandQueue::DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator)
{
	m_AllocatorPool.DiscardAllocator(FenceValueForReset, Allocator);
}

uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* List)
{
	
	ASSERT_SUCCEEDED(((ID3D12GraphicsCommandList*)List)->Close());

	m_CommandQueue->ExecuteCommandLists(1, &List);

	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);

	//后置++，返回的是Signal设定的FenceValue
	return m_NextFenceValue++;
}

void CommandQueue::WaitForFence(uint64_t FenceValue)
{
	if (IsFenceComplete(FenceValue))
		return;

	m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
	WaitForSingleObject(m_FenceEventHandle, INFINITE);
	m_LastCompletedFenceValue = FenceValue;
}

uint64_t CommandQueue::IncrementFence(void)
{
	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);
	return m_NextFenceValue++;
}

bool CommandQueue::IsFenceComplete(uint64_t FenceValue)
{
	
	//更新m_LastCompletedFenceValue，同时也避免了多次查询GetCompletedValue的开销
	if (FenceValue > m_LastCompletedFenceValue)
		m_LastCompletedFenceValue = std::max(m_pFence->GetCompletedValue(), m_LastCompletedFenceValue);

	return FenceValue <= m_LastCompletedFenceValue;
}

CommandListManager::CommandListManager():
	m_Device(nullptr),
	m_GraphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{
}

CommandListManager::~CommandListManager()
{
	Shutdown();
}

void CommandListManager::Create(ID3D12Device* pDevice)
{
	ASSERT(pDevice != nullptr);
	m_Device = pDevice;
	m_GraphicsQueue.Create(pDevice);
	m_ComputeQueue.Create(pDevice);
	m_CopyQueue.Create(pDevice);
}

void CommandListManager::Shutdown()
{
	m_GraphicsQueue.Shutdown();
	m_ComputeQueue.Shutdown();
	m_CopyQueue.Shutdown();
}

void CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator)
{
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		*Allocator = m_GraphicsQueue.RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		*Allocator = m_ComputeQueue.RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		*Allocator = m_CopyQueue.RequestAllocator();
		break;
	}
	ASSERT_SUCCEEDED(m_Device->CreateCommandList(0, Type, *Allocator, nullptr, IID_PPV_ARGS(List)));
	(*List)->SetName(L"CommandList");
}


