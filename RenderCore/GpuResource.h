#pragma once
#include"Utility.h"
/*
* (D3D12_RESOURCE_STATES)-1，枚举强行转换到未定义枚举值也是可以的
*/
class GpuResource
{
	friend class CommandContext;
public:
	GpuResource() :
		m_UsageState(D3D12_RESOURCE_STATE_COMMON),
		m_TransitioningState((D3D12_RESOURCE_STATES)-1)
	{
	}

	GpuResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES CurrentState, D3D12_RESOURCE_DESC Desc, const wchar_t* Name) :
		m_pResource(pResource),
		m_ResourceDesc(Desc),
		m_UsageState(CurrentState),
		m_TransitioningState((D3D12_RESOURCE_STATES)-1)
	{
		m_pResource->SetName(Name);
	}

	~GpuResource() { Destroy(); }

	virtual void Destroy()
	{
		m_pResource = nullptr;
		++m_VersionID;
	}

	ID3D12Resource* operator->() { return m_pResource.Get(); }
	const ID3D12Resource* operator->() const { return m_pResource.Get(); }

	ID3D12Resource* GetResource() { return m_pResource.Get(); }
	const ID3D12Resource* GetResource() const { return m_pResource.Get(); }

	D3D12_RESOURCE_DESC GetDesc() { return m_ResourceDesc; }

	ID3D12Resource** GetAddressOf() { return m_pResource.GetAddressOf(); }

	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() { return GetResource()->GetGPUVirtualAddress(); }

	uint32_t GetVersionID() const { return m_VersionID; }

	void SetName(const wchar_t* Name) { m_pResource->SetName(Name); }

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	D3D12_RESOURCE_DESC m_ResourceDesc;
	D3D12_RESOURCE_STATES m_UsageState;
	//用于Split Barriers，让GPU可以在Barriers期间安插其他异步任务
	D3D12_RESOURCE_STATES m_TransitioningState;

	uint32_t m_VersionID = 0;
};