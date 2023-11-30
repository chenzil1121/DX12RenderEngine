#include "RayTracingPipelineState.h"

void RayTracingPSO::SetShader(const void* Binary, size_t Size, std::vector<const WCHAR*>entryPoints)
{
	SetShader(CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size), entryPoints);
}

void RayTracingPSO::SetShader(const D3D12_SHADER_BYTECODE& Binary, std::vector<const WCHAR*>entryPoints)
{
	auto lib = m_PSODesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	lib->SetDXILLibrary(&Binary);
	lib->DefineExports(entryPoints.data(), entryPoints.size());
}

void RayTracingPSO::SetHitGroup(const wchar_t* HitGroupName, const wchar_t* AnyHitShaderName, const wchar_t* ClosestHitShaderName, const wchar_t* IntersectionShaderName, D3D12_HIT_GROUP_TYPE HitGroupType)
{
	auto hitGroup = m_PSODesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();

	hitGroup->SetHitGroupExport(HitGroupName);
    if(AnyHitShaderName != L"")
	    hitGroup->SetAnyHitShaderImport(AnyHitShaderName);
    if (ClosestHitShaderName != L"")
	    hitGroup->SetClosestHitShaderImport(ClosestHitShaderName);
	if (IntersectionShaderName != L"")
		hitGroup->SetIntersectionShaderImport(IntersectionShaderName);
	hitGroup->SetHitGroupType(HitGroupType);
}

void RayTracingPSO::SetShaderConfig(uint32_t MaxPayloadSizeInBytes, uint32_t MaxAttributeSizeInBytes)
{
	auto shaderConfig = m_PSODesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	shaderConfig->Config(MaxPayloadSizeInBytes, MaxAttributeSizeInBytes);
}

void RayTracingPSO::SetPipelineConfig(uint32_t MaxTraceRecursionDepth)
{
	auto pipelineConfig = m_PSODesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	pipelineConfig->Config(MaxTraceRecursionDepth);
}

void RayTracingPSO::SetGlobalRootSignature(ID3D12RootSignature* pRootSig)
{
	auto globalRootSignature = m_PSODesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(pRootSig);
}

void RayTracingPSO::SetLocalRootSignature(ID3D12RootSignature* pRootSig)
{
	auto localRootSignature = m_PSODesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(pRootSig);
}

void RayTracingPSO::SetLocalRootSignature(ID3D12RootSignature* pRootSig, const WCHAR* ExportNames[], uint32_t ExportCount)
{
	auto localRootSignature = m_PSODesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(pRootSig);

	SetExportAssociation(ExportNames, ExportCount, *localRootSignature);
}

void RayTracingPSO::SetExportAssociation(const WCHAR* ExportNames[], uint32_t ExportCount, const D3D12_STATE_SUBOBJECT& pSubobjectToAssociate)
{
	auto association = m_PSODesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	association->SetSubobjectToAssociate(pSubobjectToAssociate);
	association->AddExports(ExportNames, ExportCount);
}

void RayTracingPSO::PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
    std::wstringstream wstr;
    wstr << L"\n";
    wstr << L"--------------------------------------------------------------------\n";
    wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
    if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

    auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
    {
        std::wostringstream woss;
        for (UINT i = 0; i < numExports; i++)
        {
            woss << L"|";
            if (depth > 0)
            {
                for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
            }
            woss << L" [" << i << L"]: ";
            if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
            woss << exports[i].Name << L"\n";
        }
        return woss.str();
    };

    for (UINT i = 0; i < desc->NumSubobjects; i++)
    {
        wstr << L"| [" << i << L"]: ";
        switch (desc->pSubobjects[i].Type)
        {
        case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
            wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
            wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
            wstr << L"Node Mask: 0x" << std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
            break;
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
        {
            wstr << L"DXIL Library 0x";
            auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
            wstr << ExportTree(1, lib->NumExports, lib->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
        {
            wstr << L"Existing Library 0x";
            auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << collection->pExistingCollection << L"\n";
            wstr << ExportTree(1, collection->NumExports, collection->pExports);
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"Subobject to Exports Association (Subobject [";
            auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
            wstr << index << L"])\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
        {
            wstr << L"DXIL Subobjects to Exports Association (";
            auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
            wstr << association->SubobjectToAssociate << L")\n";
            for (UINT j = 0; j < association->NumExports; j++)
            {
                wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
            }
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
        {
            wstr << L"Raytracing Shader Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
            wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
        {
            wstr << L"Raytracing Pipeline Config\n";
            auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
            wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
            break;
        }
        case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
        {
            wstr << L"Hit Group (";
            auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
            wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
            wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
            wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
            break;
        }
        }
        wstr << L"|--------------------------------------------------------------------\n";
    }
    wstr << L"\n";
    OutputDebugStringW(wstr.str().c_str());
}

void RayTracingPSO::Finalize(ID3D12Device5* pDevice)
{
#if _DEBUG
	PrintStateObjectDesc(m_PSODesc);
#endif
    ASSERT_SUCCEEDED(pDevice->CreateStateObject(m_PSODesc, IID_PPV_ARGS(&m_DXRStateObject)), "Couldn't create DirectX Raytracing state object.\n");
}
