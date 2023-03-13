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
	//方便取值
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
* 只是管理一块区域的Allocation信息，并没有参与实际内存或显存的分配
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
	//只允许移动语义，不允许拷贝和赋值语义，因为只能有一个Manager负责一块区域，避免数据同步
	VariableSizeAllocationsManager& operator = (VariableSizeAllocationsManager&& rhs) = default;
	VariableSizeAllocationsManager(const VariableSizeAllocationsManager&) = delete;
	VariableSizeAllocationsManager& operator = (const VariableSizeAllocationsManager& rhs) = delete;

	void AddNewBlock(OffsetType Offset, OffsetType Size)
	{
		//map的emplace返回的是一个pair<iterator,bool>
		auto NewBlockIt = m_FreeBlocksByOffset.emplace(Offset, Size);
		//multimap的emplace返回的是iterator，因为multimap不存在冲突
		auto OrderIt = m_FreeBlocksBySize.emplace(Size, NewBlockIt.first);
		NewBlockIt.first->second.OrderBySizeIt = OrderIt;
	}

	Allocation Allocate(OffsetType Size)
	{
		ASSERT(Size != 0, "Allocate 0 Size");
		if (m_FreeSize < Size)
			return Allocation();

		//找到一个大于等于Size的Block
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
		//找到第一个大于Offset的Block
		auto NextBlockIt = m_FreeBlocksByOffset.upper_bound(Offset);

		auto PrevBlockIt = NextBlockIt;
		ASSERT(NextBlockIt == m_FreeBlocksByOffset.end() || Offset + Size <= NextBlockIt->first);
		if (PrevBlockIt != m_FreeBlocksByOffset.begin())
		{
			--PrevBlockIt;
			ASSERT(Offset >= PrevBlockIt->first + PrevBlockIt->second.Size);
		}
		else
			PrevBlockIt = m_FreeBlocksByOffset.end();//map中只有一个元素

		OffsetType NewOffset, NewSize;
		/*
		* 第一种情况，与PrevBlockIt和NextBlockIt都相接
		* 第二种情况，只与PrevBlockIt相接
		* 第三种情况，只与NextBlockIt相接
		* 第四种情况，独立的Block
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