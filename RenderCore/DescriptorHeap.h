#pragma once
#include"Utility.h"
#include"VariableSizeGPUAllocationsManager.h"

class DescriptorHeapAllocation;

//作为CupDescriptorHeap和GupDescriptorHeap和DynamicSuballocationsManager的父类
class DescriptorAllocator
{
public:
	virtual DescriptorHeapAllocation Allocate(uint32_t Count) = 0;
	virtual void Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue) = 0;
	virtual void ReleaseRetireAllocations(uint64_t NumCompletedFrames) = 0;
	virtual uint32_t GetDescriptorSize()const = 0;

};

static uint32_t InvalidAllocationMgrId = UINT32_MAX;
//类似于DescriptorTable，DescriptorHeap中的一段连续空间
class DescriptorHeapAllocation
{
public:
	DescriptorHeapAllocation():
		m_pAllocator(nullptr),
		m_pDescriptorHeap(nullptr),
		m_NumHandles(0),
		m_AllocationManagerId(InvalidAllocationMgrId),
		m_DescriptorSize(0)
	{
		m_FirstCpuHandle.ptr = 0;
		m_FirstGpuHandle.ptr = 0;
		m_FirstGpuHandleOffsetInHeap = -1;
	}

	DescriptorHeapAllocation(
		DescriptorAllocator* Allocator,
		ID3D12DescriptorHeap* pHeap,
		D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle,
		UINT64 GpuHandleOffsetInHeap,
		uint32_t NHandles,
		uint32_t AllocationManagerId):
		m_FirstCpuHandle(CpuHandle),
		m_FirstGpuHandle(GpuHandle),
		m_FirstGpuHandleOffsetInHeap(GpuHandleOffsetInHeap),
		m_pAllocator(Allocator),
		m_pDescriptorHeap(pHeap),
		m_NumHandles(NHandles),
		m_AllocationManagerId(AllocationManagerId)
	{
		m_DescriptorSize = m_pAllocator->GetDescriptorSize();
	}

	DescriptorHeapAllocation(DescriptorHeapAllocation&& rhs):
		m_FirstCpuHandle(std::move(rhs.m_FirstCpuHandle)),
		m_FirstGpuHandle(std::move(rhs.m_FirstGpuHandle)),
		m_FirstGpuHandleOffsetInHeap(std::move(rhs.m_FirstGpuHandleOffsetInHeap)),
		m_pAllocator(std::move(rhs.m_pAllocator)),
		m_pDescriptorHeap(std::move(rhs.m_pDescriptorHeap))
	{
		m_NumHandles = rhs.m_NumHandles;
		m_AllocationManagerId = rhs.m_AllocationManagerId;
		m_DescriptorSize = rhs.m_DescriptorSize;
		rhs.Reset();
	}

	~DescriptorHeapAllocation()
	{
		if (!IsNull() && m_pAllocator)
			m_pAllocator->Free(std::move(*this), ~uint64_t(0));
		// Allocation must have been disposed by the allocator
		ASSERT(IsNull(), "Non-null descriptor is being destroyed");
	}

	DescriptorHeapAllocation& operator=(DescriptorHeapAllocation&& Allocation)
	{
		m_FirstCpuHandle = std::move(Allocation.m_FirstCpuHandle);
		m_FirstGpuHandle = std::move(Allocation.m_FirstGpuHandle);
		m_FirstGpuHandleOffsetInHeap = std::move(Allocation.m_FirstGpuHandleOffsetInHeap);
		m_NumHandles = std::move(Allocation.m_NumHandles);
		m_pAllocator = std::move(Allocation.m_pAllocator);
		m_AllocationManagerId = std::move(Allocation.m_AllocationManagerId);
		m_pDescriptorHeap = std::move(Allocation.m_pDescriptorHeap);
		m_DescriptorSize = std::move(Allocation.m_DescriptorSize);

		Allocation.Reset();

		return *this;
	}

	DescriptorHeapAllocation(const DescriptorHeapAllocation&) = delete;
	DescriptorHeapAllocation& operator=(const DescriptorHeapAllocation&) = delete;

	void Reset()
	{
		m_FirstCpuHandle.ptr = 0;
		m_FirstGpuHandle.ptr = 0;
		m_FirstGpuHandleOffsetInHeap = -1;
		m_pAllocator = nullptr;
		m_pDescriptorHeap = nullptr;
		m_NumHandles = 0;
		m_AllocationManagerId = InvalidAllocationMgrId;
		m_DescriptorSize = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t Offset = 0) const
	{
		ASSERT(Offset >= 0 && Offset < m_NumHandles);

		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = m_FirstCpuHandle;
		CPUHandle.ptr += m_DescriptorSize * Offset;

		return CPUHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t Offset = 0) const
	{
		ASSERT(Offset >= 0 && Offset < m_NumHandles);
		ASSERT(IsShaderVisible(), "Descriptor Not Shader Visible");

		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = m_FirstGpuHandle;
		GPUHandle.ptr += UINT64(m_DescriptorSize) * UINT64(Offset);

		return GPUHandle;
	}
	UINT64 GetFirstGpuHandleOffsetInHeap()
	{
		ASSERT(IsShaderVisible(), "Descriptor Not Shader Visible");
		return m_FirstGpuHandleOffsetInHeap;
	}


	ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_pDescriptorHeap; }

	uint32_t GetNumHandles() const { return m_NumHandles; }
	bool IsNull() const { return m_FirstCpuHandle.ptr == 0; }
	bool IsShaderVisible() const { return m_FirstGpuHandle.ptr != 0; }
	uint32_t GetAllocationManagerId() const { return m_AllocationManagerId; }
	uint32_t GetDescriptorSize() const { return m_DescriptorSize; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGpuHandle;
	UINT64 m_FirstGpuHandleOffsetInHeap;

	DescriptorAllocator* m_pAllocator;
	ID3D12DescriptorHeap* m_pDescriptorHeap;

	uint32_t m_NumHandles;
	uint32_t m_AllocationManagerId;
	uint32_t m_DescriptorSize;
};

//单个DescriptorHeap的封装，适用于CPU可见堆和GPU可见堆
//其中GPU可见堆中的static部分是由该类掌握，而dynamic部分是由该类分配一个大型shared区域，实际分配交给DynamicSuballocationsManager
class DescriptorHeapAllocationManager
{
public:
	DescriptorHeapAllocationManager(
		DescriptorAllocator& ParentAllocator,
		ID3D12Device* Device,
		uint32_t ThisManagerId,
		const D3D12_DESCRIPTOR_HEAP_DESC HeapDesc
	);

	DescriptorHeapAllocationManager(
		DescriptorAllocator& ParentAllocator,
		ID3D12Device* Device,
		uint32_t ThisManagerId,
		ID3D12DescriptorHeap* pd3d12DescriptorHeap,
		uint32_t FirstDescriptor,
		uint32_t NumDescriptors
	);

	DescriptorHeapAllocationManager(DescriptorHeapAllocationManager&& rhs):
		m_ParentAllocator(rhs.m_ParentAllocator),
		m_Device(rhs.m_Device),
		m_ManagerId(rhs.m_ManagerId),
		m_HeapDesc(rhs.m_HeapDesc),
		m_DescriptorSize(rhs.m_DescriptorSize),
		m_FreeBlockManager(std::move(rhs.m_FreeBlockManager)),
		m_FirstCPUHandle(rhs.m_FirstCPUHandle),
		m_FirstGPUHandle(rhs.m_FirstGPUHandle),
		m_pDescriptorHeap(std::move(rhs.m_pDescriptorHeap))
	{
		rhs.m_Device = nullptr;
		rhs.m_ManagerId = UINT32_MAX;
		rhs.m_FirstCPUHandle.ptr = 0;
		rhs.m_FirstGPUHandle.ptr = 0;
	}

	DescriptorHeapAllocationManager& operator = (DescriptorHeapAllocationManager&&) = delete;
	DescriptorHeapAllocationManager(const DescriptorHeapAllocationManager&) = delete;
	DescriptorHeapAllocationManager& operator = (const DescriptorHeapAllocationManager&) = delete;

	~DescriptorHeapAllocationManager() 
	{
		ASSERT(m_FreeBlockManager.IsEmpty(), "Not all descriptors were released");
	};

	DescriptorHeapAllocation Allocate(uint32_t Count);
	void FreeAllocation(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue);
	void ReleaseRetireAllocations(uint64_t NumCompletedFrames);

	size_t GetNumAvailableDescriptors() const { return m_FreeBlockManager.GetFreeSize(); }
private:
	DescriptorAllocator& m_ParentAllocator;
	ID3D12Device* m_Device;

	uint32_t m_ManagerId;
	const D3D12_DESCRIPTOR_HEAP_DESC m_HeapDesc;
	const uint32_t m_DescriptorSize;

	VariableSizeGPUAllocationsManager m_FreeBlockManager;

	D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCPUHandle = { 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGPUHandle = { 0 };
	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
};

//管理一系列CPU可见堆，整个程序只需要四种对应堆的实例
class CPUDescriptorHeap :public DescriptorAllocator
{
public:
	CPUDescriptorHeap(ID3D12Device* Device, uint32_t InitDescriptorNum, D3D12_DESCRIPTOR_HEAP_TYPE Type):
		m_Device(Device),
		m_Type(Type),
		m_DescriptorSize(Device->GetDescriptorHandleIncrementSize(Type)),
		m_InitDescriptorNum(InitDescriptorNum)
	{
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc;
		HeapDesc.NodeMask = 0;
		HeapDesc.NumDescriptors = m_InitDescriptorNum;
		HeapDesc.Type = m_Type;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		m_HeapPool.emplace_back(*this, m_Device, 0, HeapDesc);
		m_AvailableHeaps.insert(0);
	};

	CPUDescriptorHeap(const CPUDescriptorHeap&) = delete;
	CPUDescriptorHeap(CPUDescriptorHeap&&) = delete;
	CPUDescriptorHeap& operator = (const CPUDescriptorHeap&) = delete;
	CPUDescriptorHeap& operator = (CPUDescriptorHeap&&) = delete;

	~CPUDescriptorHeap() {};

	DescriptorHeapAllocation Allocate(uint32_t Count) override;
	void Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue) override;
	uint32_t GetDescriptorSize() const override { return m_DescriptorSize; }

	void ReleaseRetireAllocations(uint64_t NumCompletedFrames) override;
private:
	ID3D12Device* m_Device;

	std::vector<DescriptorHeapAllocationManager> m_HeapPool;
	std::unordered_set<uint32_t> m_AvailableHeaps;
	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	const uint32_t m_DescriptorSize;
	const uint32_t m_InitDescriptorNum;
};

//管理GPU可见堆，整个程序只需要二种对应堆的实例,因为对Pipeline切换GPUDescriptorHeap的开销大，所以只Create两个堆，分别对应两种类型
class GPUDescriptorHeap :public DescriptorAllocator
{
public:
	GPUDescriptorHeap(
		ID3D12Device* Device,
		uint32_t NumDescriptorsInHeap,
		uint32_t NumDynamicDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE Type);

	GPUDescriptorHeap(const GPUDescriptorHeap&) = delete;
	GPUDescriptorHeap(GPUDescriptorHeap&&) = delete;
	GPUDescriptorHeap& operator = (const GPUDescriptorHeap&) = delete;
	GPUDescriptorHeap& operator = (GPUDescriptorHeap&&) = delete;

	~GPUDescriptorHeap() {}

	DescriptorHeapAllocation Allocate(uint32_t Count) override
	{
		return m_HeapAllocationManager.Allocate(Count);
	}

	void Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue) override;
	uint32_t GetDescriptorSize() const override { return m_DescriptorSize; }

	DescriptorHeapAllocation AllocateDynamic(uint32_t Count)
	{
		return m_DynamicAllocationsManager.Allocate(Count);
	}

	void ReleaseRetireAllocations(uint64_t NumCompletedFrames) override;

private:
	ID3D12Device* m_Device;

	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	const uint32_t m_DescriptorSize;

	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;

	// 静态部分
	DescriptorHeapAllocationManager m_HeapAllocationManager;

	// 动态部分
	DescriptorHeapAllocationManager m_DynamicAllocationsManager;
};

//在GPUDescriptorHeap中的m_DynamicAllocationsManager分配的空间中进行二次线性分配，线性分配就是只增长不回收
class DynamicSuballocationsManager :public DescriptorAllocator
{
public:
	DynamicSuballocationsManager(GPUDescriptorHeap& ParentGPUHeap, uint32_t DynamicChunkSize) :
		m_ParentGPUHeap(ParentGPUHeap),
		m_DynamicChunkSize(DynamicChunkSize),
		m_CurrentSuballocationOffset(0)
	{}

	DynamicSuballocationsManager(const DynamicSuballocationsManager&) = delete;
	DynamicSuballocationsManager(DynamicSuballocationsManager&&) = delete;
	DynamicSuballocationsManager& operator = (const DynamicSuballocationsManager&) = delete;
	DynamicSuballocationsManager& operator = (DynamicSuballocationsManager&&) = delete;

	~DynamicSuballocationsManager() {}


	DescriptorHeapAllocation Allocate(uint32_t Count) override;
	void Free(DescriptorHeapAllocation&& Allocation, uint64_t FenceValue) override
	{
		//线性分配不考虑回收，同意由m_ParentGPUHeap回收chunk
		Allocation.Reset();
	}
	void ReleaseRetireAllocations(uint64_t NumCompletedFrames) override;

	uint32_t GetDescriptorSize() const override { return m_ParentGPUHeap.GetDescriptorSize(); }

private:
	GPUDescriptorHeap& m_ParentGPUHeap;

	std::vector<DescriptorHeapAllocation>m_Suballocations;

	uint32_t m_CurrentSuballocationOffset;
	uint32_t m_DynamicChunkSize;
};