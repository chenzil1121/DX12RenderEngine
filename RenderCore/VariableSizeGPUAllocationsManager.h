#pragma once
#include<deque>
#include"VariableSizeAllocationsManager.h"

class VariableSizeGPUAllocationsManager :public VariableSizeAllocationsManager
{
public:
	VariableSizeGPUAllocationsManager() {}

	VariableSizeGPUAllocationsManager(OffsetType MaxSize) :VariableSizeAllocationsManager(MaxSize) {}

	~VariableSizeGPUAllocationsManager() 
	{
		ASSERT(m_RetireAllocations.empty(), "Not all stale allocations released");
		//ASSERT(m_RetireAllocationsNum == 0, "Not all stale allocations released");
	}

	//�����ƶ����캯����Ҫ�ȵ��ø����ƶ����캯��
	//����std::moveȷ�����ø�����ƶ����캯�������ǿ������캯��
	VariableSizeGPUAllocationsManager(VariableSizeGPUAllocationsManager&& rhs) :VariableSizeAllocationsManager(std::move(rhs))
	{
		m_RetireAllocations = std::move(rhs.m_RetireAllocations);
		//m_RetireAllocationsNum = rhs.m_RetireAllocationsNum;
		//rhs.m_RetireAllocationsNum = 0;
	}

	VariableSizeGPUAllocationsManager& operator = (VariableSizeGPUAllocationsManager&& rhs) = delete;

	void Free(OffsetType Offset, OffsetType Size, uint64_t FenceValue)
	{
		m_RetireAllocations.emplace(FenceValue, Allocation(Offset, Size));
		//m_RetireAllocationsNum
	}

	void ReleaseRetireAllocations(uint64_t FenceValue)
	{
		while (!m_RetireAllocations.empty() && m_RetireAllocations.front().first <= FenceValue)
		{
			auto& OldAllocation = m_RetireAllocations.front().second;
			VariableSizeAllocationsManager::Free(OldAllocation.Offset, OldAllocation.Size);
			m_RetireAllocations.pop();
		}
	}

private:
	std::queue<std::pair<uint64_t, Allocation>>m_RetireAllocations;
	//size_t m_RetireAllocationsNum = 0;
};