#pragma once
#include "GpuResource.h"
#include "RenderDevice.h"
#include "DynamicUploadHeap.h"

class RenderDevice;
struct DynamicAllocation;

class Buffer :public GpuResource
{
public:
	Buffer() :
		GpuResource(), 
		pDevice(nullptr),
		m_DynamicFlag(false)
	{}
	Buffer(
		RenderDevice* Device,
		const void* InitData,
		UINT64 ByteSize,
		bool IsDynamic,
		bool IsConstant,
		const wchar_t* Name = L"",
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATES InitState = D3D12_RESOURCE_STATE_COMMON
	);
	~Buffer()
	{
	}

	void Upload(const void* UploadData)
	{
		if (m_DynamicFlag)
		{
			memcpy(m_DynamicAlloc.CPUAddress, UploadData, m_DynamicAlloc.Size);
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress()
	{
		if (!m_DynamicFlag)
			return GpuResource::GetGpuVirtualAddress();
		else
			return m_DynamicAlloc.GPUAddress;
	}

private:
	RenderDevice* pDevice;

	bool m_DynamicFlag;
	DynamicAllocation m_DynamicAlloc;
};