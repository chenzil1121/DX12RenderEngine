#pragma once
#include "RingBuffer.h"

struct DynamicAllocation
{
    DynamicAllocation(Microsoft::WRL::ComPtr<ID3D12Resource> pBuff, size_t ThisOffset, size_t ThisSize) :
        pBuffer(pBuff), Offset(ThisOffset), Size(ThisSize) {}
    DynamicAllocation(){}

    Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer = nullptr;
    size_t Offset = 0;
    size_t Size = 0;
    void* CPUAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
};

class GPURingBuffer : public RingBuffer
{
public:
    GPURingBuffer(size_t MaxSize, ID3D12Device* pd3d12Device);

    GPURingBuffer(GPURingBuffer&& rhs);
    GPURingBuffer& operator =(GPURingBuffer&& rhs);
    GPURingBuffer(const GPURingBuffer&) = delete;
    GPURingBuffer& operator =(GPURingBuffer&) = delete;
    ~GPURingBuffer();

    DynamicAllocation Allocate(size_t SizeInBytes);

private:
    void Destroy();

    void* m_CpuVirtualAddress;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pBuffer;
};

class DynamicUploadHeap
{
public:
    DynamicUploadHeap(ID3D12Device* pDevice, size_t InitialSize);

    DynamicUploadHeap(const DynamicUploadHeap&) = delete;
    DynamicUploadHeap(DynamicUploadHeap&&) = delete;
    DynamicUploadHeap& operator=(const DynamicUploadHeap&) = delete;
    DynamicUploadHeap& operator=(DynamicUploadHeap&&) = delete;

    DynamicAllocation Allocate(size_t SizeInBytes,bool IsConstantBuffer);

    void FinishFrame(UINT64 FenceValue, UINT64 LastCompletedFenceValue);
private:
    std::vector<GPURingBuffer> m_RingBuffers;
    ID3D12Device* m_Device;
};