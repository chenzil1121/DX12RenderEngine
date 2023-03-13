#include "Buffer.h"

Buffer::Buffer(RenderDevice* Device, const void* initData, UINT64 byteSize, bool IsDynamic, bool IsConstant):
	pDevice(Device),
	m_DynamicFlag(IsDynamic)
{

	m_ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

	if (!m_DynamicFlag)
	{
		if (IsConstant)
		{
			UINT64 AlignSize = byteSize % 256 == 0 ? 0 : 256 - byteSize % 256;
			AlignSize = byteSize + AlignSize;
			m_ResourceDesc.Width = AlignSize;
		}

		ASSERT_SUCCEEDED(pDevice->g_Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&m_ResourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(m_pResource.GetAddressOf())));

		if (initData)
		{
			GpuResource UploadBuffer;

			ASSERT_SUCCEEDED(pDevice->g_Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&m_ResourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(UploadBuffer.GetAddressOf())));

			void* DestAddres = nullptr;

			ASSERT_SUCCEEDED(UploadBuffer->Map(0, nullptr, &DestAddres));
			memcpy(DestAddres, initData, static_cast<size_t>(byteSize));
			UploadBuffer->Unmap(0, nullptr);

			GraphicsContext& Context = pDevice->BeginGraphicsContext(L"InitBuffer");
			Context.TransitionResource(this, D3D12_RESOURCE_STATE_COPY_DEST, true);
			Context.CopyResource(this, &UploadBuffer);
			Context.TransitionResource(this, D3D12_RESOURCE_STATE_GENERIC_READ);
			Context.Finish(true);
		}
	}
	else
	{
		m_DynamicAlloc = pDevice->AllocateDynamicResouorce(byteSize, IsConstant);
	}

}

void Buffer::CreateCBV()
{
	m_CBV = pDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	if (!m_DynamicFlag)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.SizeInBytes = m_ResourceDesc.Width;
		cbvDesc.BufferLocation = GetGpuVirtualAddress();
		pDevice->g_Device->CreateConstantBufferView(&cbvDesc, m_CBV.GetCpuHandle(0));
	}
	else
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.SizeInBytes = m_DynamicAlloc.Size;
		cbvDesc.BufferLocation = m_DynamicAlloc.GPUAddress;
		pDevice->g_Device->CreateConstantBufferView(&cbvDesc, m_CBV.GetCpuHandle(0));
	}
}
