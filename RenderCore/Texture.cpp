#include"Texture.h"
//GPU直接创建Texture
Texture::Texture(
	RenderDevice* Device,
	D3D12_RESOURCE_DESC& Desc,
	D3D12_RESOURCE_STATES InitialState,
	D3D12_CLEAR_VALUE& Clear,
	const wchar_t* Name) :
	pDevice(Device),
	m_TexName(Name)
{
	ASSERT_SUCCEEDED(pDevice->g_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&Desc,
		InitialState,
		&Clear,
		IID_PPV_ARGS(m_pResource.GetAddressOf())));
	SetName(Name);
	m_ResourceDesc = Desc;
	m_UsageState = InitialState;
}
//绑定了另一个Texture
Texture::Texture(
	RenderDevice* Device,
	D3D12_RESOURCE_DESC& Desc,
	D3D12_RESOURCE_STATES InitialState,
	ID3D12Resource* pResource,
	const wchar_t* Name) :
	pDevice(Device),
	m_TexName(Name)
{
	m_pResource.Attach(pResource);
	m_UsageState = InitialState;
	m_ResourceDesc = Desc;
	m_pResource->SetName(Name);
}
//从内存中上传纹理数据到GPU
Texture::Texture(
	RenderDevice* Device,
	D3D12_RESOURCE_DESC& Desc,
	const D3D12_SUBRESOURCE_DATA* InitData,
	ID3D12Resource* pResource,
	const wchar_t* Name) :
	pDevice(Device),
	m_TexName(Name)
{
	m_pResource.Attach(pResource);
	m_UsageState = D3D12_RESOURCE_STATE_COPY_DEST;
	m_ResourceDesc = Desc;
	m_pResource->SetName(Name);

	ComPtr<ID3D12Resource> textureUploadHeap;

	const UINT num2DSubresources = m_ResourceDesc.DepthOrArraySize * m_ResourceDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pResource.Get(), 0, num2DSubresources);

	ASSERT_SUCCEEDED(pDevice->g_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap)));
	
	GraphicsContext& Context = pDevice->BeginGraphicsContext(L"Texture Upload");
	UpdateSubresources(Context.GetCommandList(), m_pResource.Get(), textureUploadHeap.Get(), 0, 0, num2DSubresources, InitData);
	Context.TransitionResource(this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.Finish(true);
}