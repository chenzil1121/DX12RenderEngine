#include "RenderDevice.h"
#include "CommandListManager.h"
#include "CommandContext.h"


ID3D12Device5* InitializeDevice()
{
    ID3D12Device5* pDevice = nullptr;

	uint32_t useDebugLayers = 0;
//VS自带的宏变量，调试模式下才执行
#if _DEBUG
	// Default to true for debug builds
	useDebugLayers = 1;
#endif

    DWORD dxgiFactoryFlags = 0;

    if (useDebugLayers)
    {
        ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
        {
            debugInterface->EnableDebugLayer();

            uint32_t useGPUBasedValidation = 0;
            if (useGPUBasedValidation)
            {
                ComPtr<ID3D12Debug1> debugInterface1;
                if (SUCCEEDED((debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1)))))
                {
                    debugInterface1->SetEnableGPUBasedValidation(true);
                }
            }
        }
        else
        {
            Utility::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
        }
    }

    // Obtain the DXGI factory
    ComPtr<IDXGIFactory6> dxgiFactory;
    ASSERT_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    // Create the D3D graphics device
    ComPtr<IDXGIAdapter1> pAdapter;

    uint32_t bUseWarpDriver = false;

    D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);

    if (!bUseWarpDriver)
    {
        SIZE_T MaxSize = 0;

        for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);

            // Is a software adapter?
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            // By default, search for the adapter with the most memory because that's usually the dGPU.
            if (desc.DedicatedVideoMemory < MaxSize)
                continue;

            // Can create a D3D12 device?
            if (FAILED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice))))
                continue;

            MaxSize = desc.DedicatedVideoMemory;

            Utility::Printf(L"Selected GPU:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
        }
    }
#ifndef RELEASE
    {
        bool DeveloperModeEnabled = false;

        // Look in the Windows Registry to determine if Developer Mode is enabled
        HKEY hKey;
        LSTATUS result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS)
        {
            DWORD keyValue, keySize = sizeof(DWORD);
            result = RegQueryValueEx(hKey, L"AllowDevelopmentWithoutDevLicense", 0, NULL, (byte*)&keyValue, &keySize);
            if (result == ERROR_SUCCESS && keyValue == 1)
                DeveloperModeEnabled = true;
            RegCloseKey(hKey);
        }

        WARN_ONCE_IF_NOT(DeveloperModeEnabled, "Enable Developer Mode on Windows 11 to get consistent profiling results");

        // Prevent the GPU from overclocking or underclocking to get consistent timings
        if (DeveloperModeEnabled)
            pDevice->SetStablePowerState(TRUE);
    }
#endif	

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData5 = {};
    HRESULT hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData5, sizeof(featureSupportData5));
    if (SUCCEEDED(hr) && featureSupportData5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        // DXR is supported
        Utility::Printf(L"DXR is supported\n");
    }
    else
    {
        // DXR is not supported
        Utility::Printf(L"DXR is not supported\n");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS featureSupportData = {};
    hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &featureSupportData, sizeof(featureSupportData));
    if (SUCCEEDED(hr) && featureSupportData.StandardSwizzle64KBSupported)
    {
        Utility::Printf(L"StandardSwizzle64KBSupported is supported\n");
    }
    else
    {
        Utility::Printf(L"StandardSwizzle64KBSupported is not supported\n");
    }


    return pDevice;
}

RenderDevice::RenderDevice(int width, int height):
    g_Device(InitializeDevice()),
    g_ContextManager(this),
    g_CPUDescriptorHeap
    {
        {g_Device,Utility::CPUDescriptorHeapSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV},
        {g_Device,Utility::CPUDescriptorHeapSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER},
        {g_Device,Utility::CPUDescriptorHeapSize[2], D3D12_DESCRIPTOR_HEAP_TYPE_RTV},
        {g_Device,Utility::CPUDescriptorHeapSize[3], D3D12_DESCRIPTOR_HEAP_TYPE_DSV}
    },
    g_GPUDescriptorHeap
    {
        {g_Device,Utility::GPUDescriptorHeapSize[0],Utility::GPUDescriptorHeapDynamicSize[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV},
        {g_Device,Utility::GPUDescriptorHeapSize[1],Utility::GPUDescriptorHeapDynamicSize[1], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER}
    },
    g_DynamicUploadHeap(g_Device, Utility::DynamicUploadHeapSize),
    g_DisplayWidth(width),
    g_DisplayHeight(height)
{
    g_CommandManager.Create(g_Device);
}

RenderDevice::~RenderDevice()
{

}

void RenderDevice::Free()
{
    g_CommandManager.IdleGPU();
    g_ContextManager.DestroyAllContexts();
    auto FenceValue = g_CommandManager.GetGraphicsQueue().GetLastCompletedFenceValue();
    g_DynamicUploadHeap.FinishFrame(FenceValue, FenceValue);
    g_CommandManager.Shutdown();
}

GraphicsContext& RenderDevice::BeginGraphicsContext(const std::wstring ID)
{
    CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
    //NewContext->SetID(ID);

    return NewContext->GetGraphicsContext();
}

ComputeContext& RenderDevice::BeginComputeContext(const std::wstring ID)
{
    CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE);
    //NewContext->SetID(ID);

    return NewContext->GetComputeContext();
}

DescriptorHeapAllocation RenderDevice::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count)
{
    return g_CPUDescriptorHeap[Type].Allocate(Count);
}

DescriptorHeapAllocation RenderDevice::AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count)
{
    return g_GPUDescriptorHeap[Type].Allocate(Count);
}

DynamicAllocation RenderDevice::AllocateDynamicResouorce(size_t SizeInBytes, bool IsConstantBuffer)
{
    return g_DynamicUploadHeap.Allocate(SizeInBytes, IsConstantBuffer);
}

void RenderDevice::FreeCPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, D3D12_COMMAND_LIST_TYPE QueueType, DescriptorHeapAllocation&& RetiredDescriptors)
{
    g_CPUDescriptorHeap[HeapType].Free(std::move(RetiredDescriptors), g_CommandManager.GetQueue(QueueType).GetLastCompletedFenceValue());
    g_CPUDescriptorHeap[HeapType].ReleaseRetireAllocations(g_CommandManager.GetQueue(QueueType).GetLastCompletedFenceValue());
}

void RenderDevice::FreeGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, D3D12_COMMAND_LIST_TYPE QueueType, DescriptorHeapAllocation&& RetiredDescriptors)
{
    g_GPUDescriptorHeap[HeapType].Free(std::move(RetiredDescriptors), g_CommandManager.GetQueue(QueueType).GetLastCompletedFenceValue());
    g_GPUDescriptorHeap[HeapType].ReleaseRetireAllocations(g_CommandManager.GetQueue(QueueType).GetLastCompletedFenceValue());
}
