#pragma once
#include "Buffer.h"

class AccelerationStructure
{
public:
	AccelerationStructure(RenderDevice* pDevice, std::vector<std::pair<D3D12_RAYTRACING_GEOMETRY_DESC, XMFLOAT4X4>>& GeomDesc, UINT InstanceOffsetInShaderTable) :
		m_pRenderDevice(pDevice),
		m_InstanceOffsetInShaderTable(m_InstanceOffsetInShaderTable)
	{
		Build(GeomDesc);
	}

	void Build(std::vector<std::pair<D3D12_RAYTRACING_GEOMETRY_DESC, XMFLOAT4X4>>& GeomDesc);

	D3D12_GPU_VIRTUAL_ADDRESS GetTLASGPUVirtualAddress() { return m_TopLevelAS->GetGpuVirtualAddress(); }
	void UpdateMatrix(std::vector<XMFLOAT4X4>& matrixs);
private:
	RenderDevice* m_pRenderDevice;

	UINT m_InstanceOffsetInShaderTable;
	std::unique_ptr<Buffer>m_TopLevelAS;
	std::vector<std::pair<std::unique_ptr<Buffer>, XMFLOAT4X4>>m_Instances;
};