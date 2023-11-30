#pragma once
#include "Buffer.h"

inline void* GetShaderIdentifier(const wchar_t* name, RayTracingPSO& PSO)
{
    ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    ASSERT_SUCCEEDED(PSO.GetPipelineStateObject()->QueryInterface(IID_PPV_ARGS(&stateObjectProperties)));
    
    return stateObjectProperties->GetShaderIdentifier(name);
}

// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
    ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
        shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
    {
    }

    ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
        shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
        localRootArguments(pLocalRootArguments, localRootArgumentsSize)
    {
    }

    void CopyTo(void* dest) const
    {
        uint8_t* byteDest = static_cast<uint8_t*>(dest);
        memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
        if (localRootArguments.ptr)
        {
            memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
        }
    }

    struct PointerWithSize {
        void* ptr;
        UINT size;

        PointerWithSize() : ptr(nullptr), size(0) {}
        PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
    };
    PointerWithSize shaderIdentifier;
    PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable
{
public:
    ShaderTable(UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
        : m_Name(resourceName)
    {
        m_ShaderRecordSize = Utility::Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        m_ShaderRecords.reserve(numShaderRecords);
        UINT bufferSize = numShaderRecords * m_ShaderRecordSize;
        m_MappedShaderRecords.reset(new uint8_t[bufferSize]());
        m_pMappedShaderRecords = m_MappedShaderRecords.get();
    }

    void push_back(const ShaderRecord& shaderRecord)
    {
        ASSERT(m_ShaderRecords.size() < m_ShaderRecords.capacity(), "Exceed Max Capacity");
        m_ShaderRecords.push_back(shaderRecord);
        shaderRecord.CopyTo(m_pMappedShaderRecords);
        m_pMappedShaderRecords += m_ShaderRecordSize;
    }

    void Finalize(RenderDevice* Device)
    {
        ASSERT(m_ShaderRecords.size() == m_ShaderRecords.capacity(), "Miss ShaderRecords");
        m_ShaderTableBuffer.reset(new Buffer(Device, m_MappedShaderRecords.get(), m_ShaderRecords.size() * m_ShaderRecordSize, false, false, m_Name.c_str()));
    }

    UINT GetShaderRecordSize() { return m_ShaderRecordSize; }
    UINT GetNumShaderRecords() { return static_cast<UINT>(m_ShaderRecords.size()); }
    UINT64 GetSizeInBytes() { return (UINT64)GetShaderRecordSize() * GetNumShaderRecords(); }
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() { return m_ShaderTableBuffer->GetGpuVirtualAddress(); }

    // Pretty-print the shader records.
    void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
    {
        std::wstringstream wstr;
        wstr << L"| --------------------------------------------------------------------\n";
        wstr << L"|Shader table - " << m_Name.c_str() << L": "
            << m_ShaderRecordSize << L" | "
            << m_ShaderRecords.size() * m_ShaderRecordSize << L" bytes\n";

        for (UINT i = 0; i < m_ShaderRecords.size(); i++)
        {
            wstr << L"| [" << i << L"]: ";
            wstr << shaderIdToStringMap[m_ShaderRecords[i].shaderIdentifier.ptr] << L", ";
            wstr << m_ShaderRecords[i].shaderIdentifier.size << L" + " << m_ShaderRecords[i].localRootArguments.size << L" bytes \n";
        }
        wstr << L"| --------------------------------------------------------------------\n";
        wstr << L"\n";

        OutputDebugStringW(wstr.str().c_str());
    }
private:
    std::unique_ptr<Buffer> m_ShaderTableBuffer;
    std::unique_ptr<uint8_t[]> m_MappedShaderRecords;
    uint8_t* m_pMappedShaderRecords;
    UINT m_ShaderRecordSize;

    std::wstring m_Name;
    std::vector<ShaderRecord> m_ShaderRecords;
};
