#include"DynamicUploadHeap.h"

GPURingBuffer::GPURingBuffer(size_t MaxSize, ID3D12Device* pd3d12Device) :
	RingBuffer(MaxSize),
	m_CpuVirtualAddress(nullptr),
	m_GpuVirtualAddress(0)
{
	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 0;
	HeapProps.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC ResourceDesc;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Alignment = 0;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_RESOURCE_STATES DefaultUsage;
	HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;

	ResourceDesc.Width = MaxSize;

	pd3d12Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &ResourceDesc, DefaultUsage, nullptr, IID_PPV_ARGS(m_pBuffer.GetAddressOf()));
	m_pBuffer->SetName(L"Upload Ring Buffer");

	m_GpuVirtualAddress = m_pBuffer->GetGPUVirtualAddress();
	m_pBuffer->Map(0, nullptr, &m_CpuVirtualAddress);
}

GPURingBuffer::GPURingBuffer(GPURingBuffer&& rhs):
	RingBuffer(std::move(rhs)),
	m_CpuVirtualAddress(rhs.m_CpuVirtualAddress),
	m_GpuVirtualAddress(rhs.m_GpuVirtualAddress),
	m_pBuffer(std::move(rhs.m_pBuffer))
{
	rhs.m_CpuVirtualAddress = nullptr;
	rhs.m_GpuVirtualAddress = 0;
}

GPURingBuffer& GPURingBuffer::operator=(GPURingBuffer&& rhs)
{
	RingBuffer::operator=(std::move(rhs));
	m_CpuVirtualAddress = rhs.m_CpuVirtualAddress;
	m_GpuVirtualAddress = rhs.m_GpuVirtualAddress;
	m_pBuffer = std::move(rhs.m_pBuffer);

	rhs.m_CpuVirtualAddress = nullptr;
	rhs.m_GpuVirtualAddress = 0;

	return *this;
}

GPURingBuffer::~GPURingBuffer()
{
	Destroy();
}

DynamicAllocation GPURingBuffer::Allocate(size_t SizeInBytes)
{
	auto Offset = RingBuffer::Allocate(SizeInBytes);
	if (Offset != RingBuffer::InvalidOffset)
	{
		DynamicAllocation DynAlloc(m_pBuffer, Offset, SizeInBytes);
		DynAlloc.GPUAddress = m_GpuVirtualAddress + Offset;
		DynAlloc.CPUAddress = reinterpret_cast<char*>(m_CpuVirtualAddress) + Offset;
		return DynAlloc;
	}
	else
	{
		return DynamicAllocation(nullptr, 0, 0);
	}
}

void GPURingBuffer::Destroy()
{
	m_CpuVirtualAddress = nullptr;
	m_GpuVirtualAddress = 0;
	if (m_pBuffer)
	{
		m_pBuffer->Unmap(0, nullptr);
		m_pBuffer = nullptr;
		//m_pBuffer->Release();
	}
}

DynamicUploadHeap::DynamicUploadHeap(ID3D12Device* pDevice, size_t InitialSize):
	m_Device(pDevice)
{
	m_RingBuffers.emplace_back(InitialSize, m_Device);
}

DynamicAllocation DynamicUploadHeap::Allocate(size_t SizeInBytes, bool IsConstantBuffer)
{
	if (IsConstantBuffer)
	{
		size_t AlignSize = SizeInBytes % 256 == 0 ? 0 : 256 - SizeInBytes % 256;
		SizeInBytes += AlignSize;
	}
	auto DynAlloc = m_RingBuffers.back().Allocate(SizeInBytes);
	if (!DynAlloc.pBuffer)
	{
		auto NewMaxSize = m_RingBuffers.back().GetMaxSize() * 2;
		while (NewMaxSize < SizeInBytes) NewMaxSize *= 2;
		m_RingBuffers.emplace_back(NewMaxSize, m_Device);
		DynAlloc = m_RingBuffers.back().Allocate(SizeInBytes);
	}
	return DynAlloc;
}

void DynamicUploadHeap::FinishFrame(UINT64 FenceValue, UINT64 LastCompletedFenceValue)
{
	size_t NumBuffsToDelete = 0;
	for (size_t Ind = 0; Ind < m_RingBuffers.size(); Ind++)
	{
		auto& RingBuffer = m_RingBuffers[Ind];
		RingBuffer.FinishCurrentFrame(FenceValue);
		RingBuffer.ReleaseCompletedFrames(LastCompletedFenceValue);
		//第二个条件是至少留一个GpuBuffer
		if (NumBuffsToDelete == Ind && Ind < m_RingBuffers.size() - 1 && RingBuffer.IsEmpty())
		{
			++NumBuffsToDelete;
		}
	}

	if (NumBuffsToDelete)
		m_RingBuffers.erase(m_RingBuffers.begin(), m_RingBuffers.begin() + NumBuffsToDelete);
}


