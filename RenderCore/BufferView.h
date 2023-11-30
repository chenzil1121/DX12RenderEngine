#pragma once
#include"Buffer.h"

enum class BufferViewerType
{
	SRV,
	UAV,
};

struct BufferViewerDesc
{
	BufferViewerType ViewerType;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;

	UINT64 FirstElement = 0;
	UINT NumElements = 0;
	UINT ElementSize = 0;
};

class BufferViewer
{
public:
	BufferViewer(RenderDevice* Device, Buffer* buf, BufferViewerDesc ViewerDesc, bool GpuHeapflag);
	BufferViewer(RenderDevice* Device, std::vector<Buffer*>& bufs, BufferViewerDesc* ViewerDescs, bool GpuHeapflag);

	~BufferViewer()
	{
		if (IsGpuHeap)
			pDevice->FreeGPUDescriptors(m_Viewer.GetDescriptorHeap()->GetDesc().Type, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_Viewer));
		else
			pDevice->FreeCPUDescriptors(m_Viewer.GetDescriptorHeap()->GetDesc().Type, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_Viewer));
	};

	void CreateView(std::vector<Buffer*>& bufs, BufferViewerDesc* ViewerDescs, bool flag, UINT Count);

	void CreateSRV(Buffer* buf, BufferViewerDesc ViewerDesc, UINT Offset);

	void CreateUAV(Buffer* buf, BufferViewerDesc ViewerDesc, UINT Offset);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t Offset = 0)
	{
		return m_Viewer.GetCpuHandle(Offset);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t Offset = 0)
	{
		return m_Viewer.GetGpuHandle(Offset);
	}

	UINT64 GetFirstGpuHandleOffsetInHeap()
	{
		return m_Viewer.GetFirstGpuHandleOffsetInHeap();
	}

	std::pair<D3D12_DESCRIPTOR_HEAP_TYPE, ID3D12DescriptorHeap*> GetHeapInfo()
	{
		auto pHeap = m_Viewer.GetDescriptorHeap();
		return std::make_pair(pHeap->GetDesc().Type, pHeap);
	}

	BufferViewerDesc GetDesc(UINT Offset = 0)
	{
		return m_Desc[Offset];
	}

private:
	RenderDevice* pDevice;
	bool IsGpuHeap = false;
	DescriptorHeapAllocation m_Viewer;
	std::vector<BufferViewerDesc> m_Desc;
	UINT Count;
};