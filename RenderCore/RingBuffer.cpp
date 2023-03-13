#include"RingBuffer.h"

RingBuffer::RingBuffer(OffsetType MaxSize):
	m_MaxSize(MaxSize)
{

}

RingBuffer::RingBuffer(RingBuffer&& rhs):
	m_CompletedFrameTails(std::move(rhs.m_CompletedFrameTails)),
	m_Head(rhs.m_Head),
	m_Tail(rhs.m_Tail),
	m_MaxSize(rhs.m_MaxSize),
	m_UsedSize(rhs.m_UsedSize),
	m_CurrFrameSize(rhs.m_CurrFrameSize)
{
	rhs.m_Tail = 0;
	rhs.m_Head = 0;
	rhs.m_MaxSize = 0;
	rhs.m_UsedSize = 0;
	rhs.m_CurrFrameSize = 0;
}

RingBuffer& RingBuffer::operator=(RingBuffer&& rhs)
{
	m_CompletedFrameTails = std::move(rhs.m_CompletedFrameTails);
	m_Tail = rhs.m_Tail;
	m_Head = rhs.m_Head;
	m_MaxSize = rhs.m_MaxSize;
	m_UsedSize = rhs.m_UsedSize;
	m_CurrFrameSize = rhs.m_CurrFrameSize;

	rhs.m_MaxSize = 0;
	rhs.m_Tail = 0;
	rhs.m_Head = 0;
	rhs.m_UsedSize = 0;
	rhs.m_CurrFrameSize = 0;

	return *this;
}

RingBuffer::~RingBuffer()
{
	ASSERT(m_UsedSize == 0, "All space in the ring buffer must be released");
}

RingBuffer::OffsetType RingBuffer::Allocate(OffsetType Size)
{
	if (m_UsedSize + Size > m_MaxSize)
	{
		return InvalidOffset;
	}

	if (m_Tail >= m_Head)
	{
		//                     Head             Tail     MaxSize
		//                     |                |        |
		//  [                  xxxxxxxxxxxxxxxxx         ]
		//                                         
		//
		if (m_Tail + Size <= m_MaxSize)
		{
			OffsetType Offset = m_Tail;
			m_Tail += Size;
			m_UsedSize += Size;
			m_CurrFrameSize += Size;
			return Offset;
		}
		else if (Size <= m_Head)
		{
			//逻辑上应该是一个RingBuffer，但是实际GPU内的Buffer是连续分配的，因此不能分配出Ring的空间，只能是把尾部空间填满，然后从Buffer开头使用GPU空间！！！
			OffsetType AddSize = (m_MaxSize - m_Tail) + Size;
			m_UsedSize += AddSize;
			m_CurrFrameSize += AddSize;
			m_Tail = Size;
			return 0;
		}
	}
	else if (m_Tail + Size <= m_Head)
	{
		//
		//       Tail          Head             
		//       |             |             
		//  [xxxx              xxxxxxxxxxxxxxxxxxxxxxxxxx]
		//

		OffsetType Offset = m_Tail;
		m_Tail += Size;
		m_UsedSize += Size;
		m_CurrFrameSize += Size;
		return Offset;
	}

	return InvalidOffset;
}

void RingBuffer::FinishCurrentFrame(uint64_t FenceValue)
{
	if (m_CurrFrameSize != 0)
	{
		m_CompletedFrameTails.emplace_back(FenceValue, m_Tail, m_CurrFrameSize);
		m_CurrFrameSize = 0;
	}
}

void RingBuffer::ReleaseCompletedFrames(uint64_t CompletedFenceValue)
{
	while (!m_CompletedFrameTails.empty() && m_CompletedFrameTails.front().FenceValue <= CompletedFenceValue)
	{
		const auto& OldestFrameTail = m_CompletedFrameTails.front();
		m_UsedSize -= OldestFrameTail.Size;
		m_Head = OldestFrameTail.Offset;
		m_CompletedFrameTails.pop_front();
	}
}




