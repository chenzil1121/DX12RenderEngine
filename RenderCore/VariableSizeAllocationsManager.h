#pragma once
#include<map>
#include<algorithm>
#include"Utility.h"

using OffsetType = size_t;

static OffsetType InvalidOffset = ~OffsetType(0);

struct FreeBlockInfo;

using FreeBlockByOffsetMap = std::map<OffsetType, FreeBlockInfo>;
using FreeBlockBySizeMap = std::multimap<OffsetType, FreeBlockByOffsetMap::iterator>;

struct FreeBlockInfo
{
	//����ȡֵ
	OffsetType Size;

	FreeBlockBySizeMap::iterator OrderBySizeIt;

	FreeBlockInfo(OffsetType _Size) :Size(_Size) {}
};

struct Allocation
{
	Allocation() 
	{
		Offset = InvalidOffset;
		Size = 0;
	}
	Allocation(OffsetType offset, OffsetType size) :Offset{ offset }, Size{ size }{ }

	OffsetType Offset;
	OffsetType Size;
};

/*
* ֻ�ǹ���һ�������Allocation��Ϣ����û�в���ʵ���ڴ���Դ�ķ���
*/
class VariableSizeAllocationsManager
{
public:

	VariableSizeAllocationsManager() {}

	VariableSizeAllocationsManager(OffsetType MaxSize) :m_MaxSize(MaxSize), m_FreeSize(MaxSize)
	{
		AddNewBlock(0, m_MaxSize);
	}

	~VariableSizeAllocationsManager() {}

	VariableSizeAllocationsManager(VariableSizeAllocationsManager&& rhs)
	{
		m_FreeBlocksByOffset = std::move(rhs.m_FreeBlocksByOffset);
		m_FreeBlocksBySize = std::move(rhs.m_FreeBlocksBySize);

		rhs.m_FreeSize = 0;
		rhs.m_MaxSize = 0;
	}
	//ֻ�����ƶ����壬���������͸�ֵ���壬��Ϊֻ����һ��Manager����һ�����򣬱�������ͬ��
	VariableSizeAllocationsManager& operator = (VariableSizeAllocationsManager&& rhs) = default;
	VariableSizeAllocationsManager(const VariableSizeAllocationsManager&) = delete;
	VariableSizeAllocationsManager& operator = (const VariableSizeAllocationsManager& rhs) = delete;

	void AddNewBlock(OffsetType Offset, OffsetType Size)
	{
		//map��emplace���ص���һ��pair<iterator,bool>
		auto NewBlockIt = m_FreeBlocksByOffset.emplace(Offset, Size);
		//multimap��emplace���ص���iterator����Ϊmultimap�����ڳ�ͻ
		auto OrderIt = m_FreeBlocksBySize.emplace(Size, NewBlockIt.first);
		NewBlockIt.first->second.OrderBySizeIt = OrderIt;
	}

	Allocation Allocate(OffsetType Size)
	{
		ASSERT(Size != 0, "Allocate 0 Size");
		if (m_FreeSize < Size)
			return Allocation();

		//�ҵ�һ�����ڵ���Size��Block
		auto SmallestBlockItIt = m_FreeBlocksBySize.lower_bound(Size);
		if (SmallestBlockItIt == m_FreeBlocksBySize.end())
			return Allocation();

		auto SmallestBlockIt = SmallestBlockItIt->second;

		auto Offset = SmallestBlockIt->first;
		auto NewOffset = Offset + Size;
		auto NewSize = SmallestBlockItIt->first - Size;

		m_FreeBlocksBySize.erase(SmallestBlockItIt);
		m_FreeBlocksByOffset.erase(SmallestBlockIt);
		if (NewSize > 0)
			AddNewBlock(NewOffset, NewSize);

		m_FreeSize -= Size;

		return Allocation(Offset, Size);
	}

	void Free(OffsetType Offset, OffsetType Size)
	{
		ASSERT(Offset + Size <= m_MaxSize,"Exceed Max");
		//�ҵ���һ������Offset��Block
		auto NextBlockIt = m_FreeBlocksByOffset.upper_bound(Offset);

		auto PrevBlockIt = NextBlockIt;
		ASSERT(NextBlockIt == m_FreeBlocksByOffset.end() || Offset + Size <= NextBlockIt->first);
		if (PrevBlockIt != m_FreeBlocksByOffset.begin())
		{
			--PrevBlockIt;
			ASSERT(Offset >= PrevBlockIt->first + PrevBlockIt->second.Size);
		}
		else
			PrevBlockIt = m_FreeBlocksByOffset.end();//map��ֻ��һ��Ԫ��

		OffsetType NewOffset, NewSize;
		/*
		* ��һ���������PrevBlockIt��NextBlockIt�����
		* �ڶ��������ֻ��PrevBlockIt���
		* �����������ֻ��NextBlockIt���
		* �����������������Block
		*/

		if (PrevBlockIt != m_FreeBlocksByOffset.end() && PrevBlockIt->first + PrevBlockIt->second.Size == Offset)
		{
			NewSize = Size + PrevBlockIt->second.Size;
			NewOffset = PrevBlockIt->first;
			if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
			{
				NewSize += NextBlockIt->second.Size;
				m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
				m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
				m_FreeBlocksByOffset.erase(PrevBlockIt);
				m_FreeBlocksByOffset.erase(NextBlockIt);
			}
			else
			{
				m_FreeBlocksBySize.erase(PrevBlockIt->second.OrderBySizeIt);
				m_FreeBlocksByOffset.erase(PrevBlockIt);
			}
		}
		else if (NextBlockIt != m_FreeBlocksByOffset.end() && Offset + Size == NextBlockIt->first)
		{
			NewOffset = Offset;
			NewSize = Size + NextBlockIt->second.Size;
			m_FreeBlocksBySize.erase(NextBlockIt->second.OrderBySizeIt);
			m_FreeBlocksByOffset.erase(NextBlockIt);
		}
		else
		{
			NewOffset = Offset;
			NewSize = Size;
		}
		AddNewBlock(NewOffset, NewSize);
		m_FreeSize += Size;
	}

	bool IsFull() { return m_FreeSize == 0; }
	bool IsEmpty()const { return m_FreeSize == m_MaxSize; }
	OffsetType GetMaxSize() const { return m_MaxSize; }
	OffsetType GetFreeSize()const { return m_FreeSize; }
	OffsetType GetUsedSize()const { return m_MaxSize - m_FreeSize; }

	size_t GetNumFreeBlocks() const
	{
		return m_FreeBlocksByOffset.size();
	}

	OffsetType GetMaxFreeBlockSize() const
	{
		return !m_FreeBlocksBySize.empty() ? m_FreeBlocksBySize.rbegin()->first : 0;
	}
private:
	FreeBlockByOffsetMap m_FreeBlocksByOffset;
	FreeBlockBySizeMap m_FreeBlocksBySize;

	OffsetType m_MaxSize = 0;
	OffsetType m_FreeSize = 0;
};