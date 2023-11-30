#include"DescriptorHeap.h"

DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(
	DescriptorAllocator& ParentAllocator,
	ID3D12Device* Device,
	uint32_t ThisManagerId,
	const D3D12_DESCRIPTOR_HEAP_DESC HeapDesc):
	m_ParentAllocator(ParentAllocator),
	m_Device(Device),
	m_ManagerId(ThisManagerId),
	m_HeapDesc(HeapDesc),
	m_DescriptorSize(m_Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type)),
	m_FreeBlockManager(HeapDesc.NumDescriptors)
{
	ASSERT_SUCCEEDED(m_Device->CreateDescriptorHeap(&m_HeapDesc, IID_PPV_ARGS(m_pDescriptorHeap.GetAddressOf())));
	m_FirstCPUHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
		m_FirstGPUHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
}

DescriptorHeapAllocationManager::DescriptorHeapAllocationManager(
	DescriptorAllocator& ParentAllocator,
	ID3D12Device* Device,
	uint32_t ThisManagerId,
	ID3D12DescriptorHeap* pd3d12DescriptorHeap,
	uint32_t FirstDescriptor,
	uint32_t NumDescriptors):
	m_ParentAllocator(ParentAllocator),
	m_Device(Device),
	m_ManagerId(ThisManagerId),
	m_HeapDesc(pd3d12DescriptorHeap->GetDesc()),
	m_DescriptorSize(Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type)),
	m_FreeBlockManager(NumDescriptors),
	m_pDescriptorHeap(pd3d12DescriptorHeap)
{
	m_FirstCPUHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_FirstCPUHandle.ptr +=  FirstDescriptor * m_DescriptorSize;
	if (m_HeapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		m_FirstGPUHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_FirstGPUHandle.ptr += UINT64(FirstDescriptor) * UINT64(m_DescriptorSize);
	}
}

DescriptorHeapAllocation DescriptorHeapAllocationManager::Allocate(uint32_t Count)
{
	auto Allocation = m_FreeBlockManager.Allocate(Count);
	if (Allocation.Offset == InvalidOffset)
		return DescriptorHeapAllocation();

	auto CpuHandle = m_FirstCPUHandle;
	CpuHandle.ptr += Allocation.Offset * m_DescriptorSize;

	auto GpuHandle = m_FirstGPUHandle;
	UINT64 GpuHandleOffsetInHeap = -1;
	if (m_FirstGPUHandle.ptr != 0)
	{
		GpuHandleOffsetInHeap = UINT64(Allocation.Offset);
		GpuHandle.ptr += UINT64(Allocation.Offset) * UINT64(m_DescriptorSize);
	}

	return DescriptorHeapAllocation(&m_ParentAllocator, m_pDescriptorHeap.Get(), CpuHandle, GpuHandle, GpuHandleOffsetInHeap, Allocation.Size, m_ManagerId);
}

void DescriptorHeapAllocationManager::FreeAllocation(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue)
{
	auto DescriptorOffset = (Allocation.GetCpuHandle().ptr - m_FirstCPUHandle.ptr) / m_DescriptorSize;
	m_FreeBlockManager.Free(DescriptorOffset, Allocation.GetNumHandles(), FenceValue);
	Allocation.Reset();
}

void DescriptorHeapAllocationManager::ReleaseRetireAllocations(uint64_t NumCompletedFrames)
{
	m_FreeBlockManager.ReleaseRetireAllocations(NumCompletedFrames);
}

DescriptorHeapAllocation CPUDescriptorHeap::Allocate(uint32_t Count)
{
	DescriptorHeapAllocation Allocation;

	auto AvailableHeapIt = m_AvailableHeaps.begin();
	while (AvailableHeapIt != m_AvailableHeaps.end())
	{
		auto NextIt = AvailableHeapIt;
		NextIt++;
		Allocation = m_HeapPool[*AvailableHeapIt].Allocate(Count);
		if (m_HeapPool[*AvailableHeapIt].GetNumAvailableDescriptors() == 0)
			m_AvailableHeaps.erase(*AvailableHeapIt);
		if (!Allocation.IsNull())
			break;
		AvailableHeapIt = NextIt;
	}
	if (Allocation.IsNull())
	{
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
		HeapDesc.NodeMask = 0;
		HeapDesc.NumDescriptors = std::max(m_InitDescriptorNum, static_cast<UINT>(Count));
		HeapDesc.Type = m_Type;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		m_HeapPool.emplace_back(*this, m_Device, m_HeapPool.size(), HeapDesc);
		m_AvailableHeaps.insert(m_HeapPool.size() - 1);

		Allocation = m_HeapPool.back().Allocate(Count);
	}
	return Allocation;
}

void CPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue)
{
	auto ManagerId = Allocation.GetAllocationManagerId();
	m_HeapPool[ManagerId].FreeAllocation(std::move(Allocation), FenceValue);
}

void CPUDescriptorHeap::ReleaseRetireAllocations(uint64_t NumCompletedFrames)
{
	for (size_t HeapManagerInd = 0; HeapManagerInd < m_HeapPool.size(); HeapManagerInd++)
	{
		m_HeapPool[HeapManagerInd].ReleaseRetireAllocations(NumCompletedFrames);
		if (m_HeapPool[HeapManagerInd].GetNumAvailableDescriptors() > 0)
		{
			//真正等待GPU使用完毕描述符才能重用
			m_AvailableHeaps.insert(HeapManagerInd);
		}
	}
}

GPUDescriptorHeap::GPUDescriptorHeap(
	ID3D12Device* Device,
	uint32_t NumDescriptorsInHeap,
	uint32_t NumDynamicDescriptors,
	D3D12_DESCRIPTOR_HEAP_TYPE Type):
	m_Device(Device),
	m_Type(Type),
	m_DescriptorSize(Device->GetDescriptorHandleIncrementSize(Type)),
	m_pDescriptorHeap
	(
		[&]
		{
			ComPtr<ID3D12DescriptorHeap> pHeap;
			D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
			HeapDesc.Type = m_Type;
			HeapDesc.NodeMask = 0;
			HeapDesc.NumDescriptors = NumDescriptorsInHeap + NumDynamicDescriptors;
			HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ASSERT_SUCCEEDED(m_Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(pHeap.GetAddressOf())));
			return pHeap;
		}()
	),
	m_HeapAllocationManager(*this, Device, 0, m_pDescriptorHeap.Get(), 0, NumDescriptorsInHeap),
	m_DynamicAllocationsManager(*this, Device, 1, m_pDescriptorHeap.Get(), NumDescriptorsInHeap, NumDynamicDescriptors)
{
}

void GPUDescriptorHeap::Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue)
{
	auto ManagerID = Allocation.GetAllocationManagerId();
	if (ManagerID == 0)
		m_HeapAllocationManager.FreeAllocation(std::move(Allocation), FenceValue);
	else
		m_DynamicAllocationsManager.FreeAllocation(std::move(Allocation), FenceValue);
}

void GPUDescriptorHeap::ReleaseRetireAllocations(uint64_t NumCompletedFrames)
{
	m_HeapAllocationManager.ReleaseRetireAllocations(NumCompletedFrames);
	m_DynamicAllocationsManager.ReleaseRetireAllocations(NumCompletedFrames);
}

DescriptorHeapAllocation DynamicSuballocationsManager::Allocate(uint32_t Count)
{
	if (m_Suballocations.empty() || m_CurrentSuballocationOffset + Count > m_Suballocations.back().GetNumHandles())
	{
		uint32_t ChunkSize = std::max(m_DynamicChunkSize, Count);
		auto Chunk = m_ParentGPUHeap.AllocateDynamic(ChunkSize);
		ASSERT(!Chunk.IsNull(), "GPU descriptor heap Dynamic space is exhausted.");
		m_Suballocations.emplace_back(std::move(Chunk));
		m_CurrentSuballocationOffset = 0;
	}
	ASSERT(false,"Not test this Dynamic Allocate, maybe Conflict with bindless!!!")
	auto& CurrentSuballocation = m_Suballocations.back();
	DescriptorHeapAllocation Allocation(this,
		CurrentSuballocation.GetDescriptorHeap(),
		CurrentSuballocation.GetCpuHandle(m_CurrentSuballocationOffset),
		CurrentSuballocation.GetGpuHandle(m_CurrentSuballocationOffset),
		-1,
		Count,
		CurrentSuballocation.GetAllocationManagerId());

	m_CurrentSuballocationOffset += Count;

	return Allocation;
}

void DynamicSuballocationsManager::ReleaseRetireAllocations(uint64_t NumCompletedFrames)
{
	for (auto& Allocation : m_Suballocations)
		m_ParentGPUHeap.Free(std::move(Allocation), NumCompletedFrames);

	m_Suballocations.clear();
	m_CurrentSuballocationOffset = 0;
}