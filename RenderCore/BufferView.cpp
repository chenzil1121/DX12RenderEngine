#include "BufferView.h"

BufferViewer::BufferViewer(RenderDevice* Device, Buffer* buf, BufferViewerDesc ViewerDesc, bool GpuHeapflag):
	pDevice(Device),
	IsGpuHeap(GpuHeapflag)
{
	m_Desc.push_back(ViewerDesc);
	CreateView(std::vector<Buffer*>{buf}, & ViewerDesc, GpuHeapflag, 1);
}

BufferViewer::BufferViewer(RenderDevice* Device, std::vector<Buffer*>& bufs, BufferViewerDesc* ViewerDescs, bool GpuHeapflag) :
	pDevice(Device),
	IsGpuHeap(GpuHeapflag)
{
	for (size_t i = 0; i < bufs.size(); i++)
	{
		m_Desc.push_back(ViewerDescs[i]);
	}
	CreateView(bufs, ViewerDescs, GpuHeapflag, bufs.size());
}

void BufferViewer::CreateView(std::vector<Buffer*>& bufs, BufferViewerDesc* ViewerDescs, bool flag, UINT Count)
{
	ASSERT(Count > 0, "ViewCount Should greater than 0");

	D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	
	if (IsGpuHeap)
		m_Viewer = pDevice->AllocateGPUDescriptors(HeapType, Count);
	else
		m_Viewer = pDevice->AllocateDescriptors(HeapType, Count);

	for (size_t i = 0; i < Count; i++)
	{
		switch (ViewerDescs[i].ViewerType)
		{
		case BufferViewerType::SRV:
			CreateSRV(bufs[i], ViewerDescs[i], i);
			break;
		case BufferViewerType::UAV:
			CreateUAV(bufs[i], ViewerDescs[i], i);
			break;
		};
	}
}

void BufferViewer::CreateSRV(Buffer* buf, BufferViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	if (ViewerDesc.ElementSize == 0)
	{
		ASSERT(ViewerDesc.Format != DXGI_FORMAT_UNKNOWN, "Not Structured Buffer must specified format");
		srvDesc.Format = ViewerDesc.Format;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
		srvDesc.Buffer.NumElements = ViewerDesc.NumElements;
		//ASSERT(false, "To do!!!!");
	}
	else
	{
		ASSERT(ViewerDesc.Format == DXGI_FORMAT_UNKNOWN, "Structured Buffer must be DXGI_FORMAT_UNKNOWN");
		srvDesc.Buffer.FirstElement = ViewerDesc.FirstElement;
		srvDesc.Buffer.NumElements = ViewerDesc.NumElements;
		srvDesc.Buffer.StructureByteStride = ViewerDesc.ElementSize;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	}
	pDevice->g_Device->CreateShaderResourceView(buf->GetResource(), &srvDesc, m_Viewer.GetCpuHandle(Offset));
}

void BufferViewer::CreateUAV(Buffer* buf, BufferViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

	if (ViewerDesc.ElementSize == 0)
	{
		ASSERT(ViewerDesc.Format != DXGI_FORMAT_UNKNOWN, "Not Structured Buffer must specified format");
		uavDesc.Format = ViewerDesc.Format;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		ASSERT(false, "To do!!!!");
	}
	else
	{
		ASSERT(ViewerDesc.Format == DXGI_FORMAT_UNKNOWN, "Structured Buffer must be DXGI_FORMAT_UNKNOWN");
		uavDesc.Buffer.FirstElement = ViewerDesc.FirstElement;
		uavDesc.Buffer.NumElements = ViewerDesc.NumElements;
		uavDesc.Buffer.StructureByteStride = ViewerDesc.ElementSize;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	}
	pDevice->g_Device->CreateUnorderedAccessView(buf->GetResource(), nullptr, &uavDesc, m_Viewer.GetCpuHandle(Offset));
}



