#pragma once
#include"RenderDevice.h"
#include"GpuResource.h"

class Texture :public GpuResource
{
friend class SwapChain;
public:
	Texture() :
		GpuResource(),
		pDevice(nullptr) 
	{}
	Texture(
		RenderDevice* Device,
		D3D12_RESOURCE_DESC& Desc,
		D3D12_RESOURCE_STATES InitialState,
		D3D12_CLEAR_VALUE& Clear,
		const wchar_t* Name
	);
	Texture(
		RenderDevice* Device,
		D3D12_RESOURCE_DESC& Desc,
		D3D12_RESOURCE_STATES InitialState,
		const wchar_t* Name
	);
	Texture(
		RenderDevice* Device,
		D3D12_RESOURCE_DESC & Desc,
		D3D12_RESOURCE_STATES InitialState,
		ID3D12Resource* pResource,
		const wchar_t* Name
	);

	Texture(
		RenderDevice* Device,
		D3D12_RESOURCE_DESC& Desc,
		const D3D12_SUBRESOURCE_DATA* InitData,
		ID3D12Resource* pResource,
		const wchar_t* Name
	);

	~Texture() {}

	std::wstring GetName() { return m_TexName; }

private:
	RenderDevice* pDevice;
	std::wstring m_TexName;
};