#pragma once
#include <iomanip>
#include"Utility.h"

class RayTracingPSO
{
public:
	void SetShader(const void* Binary, size_t Size, std::vector<const WCHAR*>entryPoints);
	void SetShader(const D3D12_SHADER_BYTECODE& Binary, std::vector<const WCHAR*>entryPoints);
	void SetHitGroup(const wchar_t* HitGroupName, const wchar_t* AnyHitShaderName, const wchar_t* ClosestHitShaderName, const wchar_t* IntersectionShaderName = L"", D3D12_HIT_GROUP_TYPE HitGroupType = D3D12_HIT_GROUP_TYPE_TRIANGLES);
	void SetShaderConfig(uint32_t MaxPayloadSizeInBytes, uint32_t MaxAttributeSizeInBytes);
	void SetPipelineConfig(uint32_t MaxTraceRecursionDepth);
	void SetGlobalRootSignature(ID3D12RootSignature* pRootSig);
	void SetLocalRootSignature(ID3D12RootSignature* pRootSig);
	void SetLocalRootSignature(ID3D12RootSignature* pRootSig, const WCHAR* ExportNames[], uint32_t ExportCount);
	void SetExportAssociation(const WCHAR* ExportNames[], uint32_t ExportCount, const D3D12_STATE_SUBOBJECT& pSubobjectToAssociate);

	void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc);

	void Finalize(ID3D12Device5* pDevice);

	ID3D12StateObject* GetPipelineStateObject() const { return m_DXRStateObject.Get(); }
private:
	CD3DX12_STATE_OBJECT_DESC m_PSODesc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
	ComPtr<ID3D12StateObject> m_DXRStateObject;
};