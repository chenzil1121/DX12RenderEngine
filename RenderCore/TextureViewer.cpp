#include"TextureViewer.h"

TextureViewer::TextureViewer(RenderDevice* Device, Texture* tex, TextureViewerDesc ViewerDesc, bool GpuHeapflag) :
	pDevice(Device),
	IsGpuHeap(GpuHeapflag)
{
	if (ViewerDesc.Format == DXGI_FORMAT_UNKNOWN)
		ViewerDesc.Format = tex->GetDesc().Format;
	m_Desc.push_back(ViewerDesc);
	CreateView(std::vector<Texture*>{tex}, &ViewerDesc, GpuHeapflag, 1);
}

TextureViewer::TextureViewer(RenderDevice* Device, std::vector<Texture*>& texs, TextureViewerDesc* ViewerDescs, bool GpuHeapflag) :
	pDevice(Device),
	IsGpuHeap(GpuHeapflag)
{
	for (size_t i = 0; i < texs.size(); i++)
	{
		if (ViewerDescs[i].Format == DXGI_FORMAT_UNKNOWN)
			ViewerDescs[i].Format = texs[i]->GetDesc().Format;
		m_Desc.push_back(ViewerDescs[i]);
	}
	CreateView(texs, ViewerDescs, GpuHeapflag, texs.size());
}

void TextureViewer::CreateView(std::vector<Texture*>& texs, TextureViewerDesc* ViewerDescs, bool flag, UINT Count)
{
	ASSERT(Count > 0, "ViewCount Should greater than 0");
	D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
	switch (ViewerDescs[0].ViewerType)
	{
	case TextureViewerType::RTV:
		HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		break;
	case TextureViewerType::DSV:
		HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		break;
	default:
		HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		break;
	}

	if (IsGpuHeap)
		m_Viewer = pDevice->AllocateGPUDescriptors(HeapType, Count);
	else
		m_Viewer = pDevice->AllocateDescriptors(HeapType, Count);

	for (size_t i = 0; i < Count; i++)
	{
		switch (ViewerDescs[i].ViewerType)
		{
		case TextureViewerType::RTV:
			CreateRTV(texs[i], ViewerDescs[i], i);
			break;
		case TextureViewerType::DSV:
			CreateDSV(texs[i], ViewerDescs[i], i);
			break;
		case TextureViewerType::SRV:
			CreateSRV(texs[i], ViewerDescs[i], i);
			break;
		case TextureViewerType::UAV:
			CreateUAV(texs[i], ViewerDescs[i], i);
			break;
		};
	}
}

void TextureViewer::CreateRTV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = ViewerDesc.Format;

	switch (ViewerDesc.TexType)
	{
	case TextureType::Texture2D:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = ViewerDesc.MostDetailedMip;

		}
		break;
	case TextureType::Texture2DArray:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
			rtvDesc.Texture2DMSArray.ArraySize = ViewerDesc.NumArraySlices;
			rtvDesc.Texture2DMSArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
		}
		else
		{
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = ViewerDesc.NumArraySlices;
			rtvDesc.Texture2DArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
			rtvDesc.Texture2DArray.MipSlice = ViewerDesc.MostDetailedMip;
		}
		break;
	}
	pDevice->g_Device->CreateRenderTargetView(tex->GetResource(), &rtvDesc, m_Viewer.GetCpuHandle(Offset));
}

void TextureViewer::CreateDSV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = ViewerDesc.Format;

	switch (ViewerDesc.TexType)
	{
	case TextureType::Texture2D:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Texture2D.MipSlice = ViewerDesc.MostDetailedMip;
		}
		break;
	case TextureType::Texture2DArray:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
			dsvDesc.Texture2DMSArray.ArraySize = ViewerDesc.NumArraySlices;
			dsvDesc.Texture2DMSArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
		}
		else
		{
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = ViewerDesc.NumArraySlices;
			dsvDesc.Texture2DArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
			dsvDesc.Texture2DArray.MipSlice = ViewerDesc.MostDetailedMip;
		}
		break;
	}
	pDevice->g_Device->CreateDepthStencilView(tex->GetResource(), &dsvDesc, m_Viewer.GetCpuHandle(Offset));
}

void TextureViewer::CreateSRV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = ViewerDesc.Format;

	switch (ViewerDesc.TexType)
	{
	case TextureType::Texture2D:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = ViewerDesc.NumMipLevels;
			srvDesc.Texture2D.MostDetailedMip = ViewerDesc.MostDetailedMip;
		}
		break;
	case TextureType::Texture2DArray:
		if (tex->GetDesc().SampleDesc.Count != 1)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
			srvDesc.Texture2DMSArray.ArraySize = ViewerDesc.NumArraySlices;
			srvDesc.Texture2DMSArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.ArraySize = ViewerDesc.NumArraySlices;
			srvDesc.Texture2DArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
			srvDesc.Texture2DArray.MipLevels = ViewerDesc.NumMipLevels;
			srvDesc.Texture2DArray.MostDetailedMip = ViewerDesc.MostDetailedMip;
		}
		break;
	case TextureType::TextureCubeMap:
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = ViewerDesc.NumMipLevels;
		srvDesc.TextureCube.MostDetailedMip = ViewerDesc.MostDetailedMip;
		break;
	}
	pDevice->g_Device->CreateShaderResourceView(tex->GetResource(), &srvDesc, m_Viewer.GetCpuHandle(Offset));
}

void TextureViewer::CreateUAV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = ViewerDesc.Format;

	switch (ViewerDesc.TexType)
	{
	case TextureType::Texture2D:
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = ViewerDesc.MostDetailedMip;
		break;
	case TextureType::Texture2DArray:
		
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = ViewerDesc.NumArraySlices;
		uavDesc.Texture2DArray.FirstArraySlice = ViewerDesc.FirstArraySlice;
		uavDesc.Texture2DArray.MipSlice = ViewerDesc.MostDetailedMip;
		break;
	}
	pDevice->g_Device->CreateUnorderedAccessView(tex->GetResource(), nullptr, &uavDesc, m_Viewer.GetCpuHandle(Offset));
}

