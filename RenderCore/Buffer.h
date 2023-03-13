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
		bool IsConstant
		); 
	~Buffer()
	{
		if (!m_CBV.IsNull())
			pDevice->FreeCPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_CBV));
	}
	
	void CreateCBV();

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

	D3D12_CPU_DESCRIPTOR_HANDLE GetCBV() { return m_CBV.GetCpuHandle(0); }

private:
	RenderDevice* pDevice;

	bool m_DynamicFlag;
	DynamicAllocation m_DynamicAlloc;

	DescriptorHeapAllocation m_CBV;
};