#include "Buffer.h"

Buffer::Buffer(RenderDevice* Device, const void* InitData, UINT64 ByteSize, bool IsDynamic, bool IsConstant, const wchar_t* Name, D3D12_RESOURCE_FLAGS Flags, D3D12_RESOURCE_STATES InitState):
	pDevice(Device),
	m_DynamicFlag(IsDynamic)
{
	m_ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(ByteSize, Flags);

	if (!m_DynamicFlag)
	{
		if (IsConstant)
		{
			UINT64 AlignSize = ByteSize % 256 == 0 ? 0 : 256 - ByteSize % 256;
			AlignSize = ByteSize + AlignSize;
			m_ResourceDesc.Width = AlignSize;
		}

		ASSERT_SUCCEEDED(pDevice->g_Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&m_ResourceDesc,
			InitState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ? D3D12_RESOURCE_STATE_COMMON : InitState,
			nullptr,
			IID_PPV_ARGS(m_pResource.GetAddressOf())));

		if (InitState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		{
			GraphicsContext& Context = pDevice->BeginGraphicsContext(L"UAV_STATE");
			Context.TransitionResource(this, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.Finish(true);
		}

		if (InitData)
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
			memcpy(DestAddres, InitData, static_cast<size_t>(ByteSize));
			UploadBuffer->Unmap(0, nullptr);

			GraphicsContext& Context = pDevice->BeginGraphicsContext(L"InitBuffer");
			Context.TransitionResource(this, D3D12_RESOURCE_STATE_COPY_DEST);
			Context.CopyResource(this, &UploadBuffer);
			Context.TransitionResource(this, D3D12_RESOURCE_STATE_GENERIC_READ);
			Context.Finish(true);
		}
		SetName(Name);
	}
	else
	{
		m_DynamicAlloc = pDevice->AllocateDynamicResouorce(ByteSize, IsConstant);
	}
}
