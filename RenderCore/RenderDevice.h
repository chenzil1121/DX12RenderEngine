#pragma once
//#include <dxgi1_6.h>
//#ifdef _DEBUG
//#include <dxgidebug.h>
//#endif
//
//#pragma comment(lib, "d3d12.lib") 
//#pragma comment(lib, "dxgi.lib") 
//#pragma comment(lib,"d3dcompiler.lib")

#include"Utility.h"
#include"CommandContext.h"
#include"CommandListManager.h"
#include"DescriptorHeap.h"
#include"DynamicUploadHeap.h"

class CommandListManager;
class CommandContext;
class GraphicsContext;
class ContextManager;
class CPUDescriptorHeap;
class GPUDescriptorHeap;
class DynamicUploadHeap;
class RenderDevice;

class RenderDevice
{
public:
	RenderDevice(int width, int height);
	~RenderDevice();
	void Free();

	RenderDevice(const RenderDevice&) = delete;
	RenderDevice(RenderDevice&&) = delete;
	RenderDevice& operator = (const RenderDevice&) = delete;
	RenderDevice& operator = (RenderDevice&&) = delete;

	GraphicsContext& BeginGraphicsContext(const std::wstring ID = L"");
	ComputeContext& BeginComputeContext(const std::wstring ID = L"");

	DescriptorHeapAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1);
	DescriptorHeapAllocation AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1);
	
	DynamicAllocation AllocateDynamicResouorce(size_t SizeInBytes, bool IsConstantBuffer);

	void FreeCPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, D3D12_COMMAND_LIST_TYPE QueueType, DescriptorHeapAllocation&& RetiredDescriptors);
	void FreeGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, D3D12_COMMAND_LIST_TYPE QueueType, DescriptorHeapAllocation&& RetiredDescriptors);



public:
	ID3D12Device5* g_Device;
	CommandListManager g_CommandManager;
	ContextManager g_ContextManager;

	CPUDescriptorHeap g_CPUDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	GPUDescriptorHeap g_GPUDescriptorHeap[2];
	DynamicUploadHeap g_DynamicUploadHeap;
	int g_DisplayWidth;
	int g_DisplayHeight;
};

