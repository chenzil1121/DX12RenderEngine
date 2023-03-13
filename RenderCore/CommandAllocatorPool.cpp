#include"CommandAllocatorPool.h"

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE Type):
	m_CommandListType(Type),
	m_Device(nullptr)
{
}

CommandAllocatorPool::~CommandAllocatorPool()
{
	Shutdown();
}

void CommandAllocatorPool::Create(ID3D12Device* pDevice)
{
	m_Device = pDevice;
}

void CommandAllocatorPool::Shutdown()
{
	//Release是COM组件的一个方法，用来减少引用计数，真正的释放由系统自动决定
	for (size_t i = 0; i < m_AllocatorPool.size(); i++)
		m_AllocatorPool[i]->Release();

	m_AllocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::RequestAllocator(uint64_t CompleteFenceValue)
{
	ID3D12CommandAllocator* pAllocator = nullptr;
	if (!m_ReadyAllocators.empty())
	{
		auto AllocatorPair = m_ReadyAllocators.front();
		if (CompleteFenceValue >= AllocatorPair.first)
		{
			pAllocator = AllocatorPair.second;
			//复用CommandAllocator前需要清空记录的命令
			ASSERT_SUCCEEDED(pAllocator->Reset());
			m_ReadyAllocators.pop();
		}
	}
	else
	{
		ASSERT_SUCCEEDED(m_Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&pAllocator)));
		wchar_t AllocatorName[32];
		//%zu对应size_t
		swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_AllocatorPool.size());
		pAllocator->SetName(AllocatorName);
		m_AllocatorPool.push_back(pAllocator);
	}
	return pAllocator;
}

void CommandAllocatorPool::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator)
{
	//该FenceValue表明Allocator所记录全部命令执行完毕
	m_ReadyAllocators.push(std::make_pair(FenceValue, Allocator));
}



