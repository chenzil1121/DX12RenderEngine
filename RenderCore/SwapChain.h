#pragma once
#include"Utility.h"
#include"RenderDevice.h"
#include"Texture.h"
#include"TextureViewer.h"

#define SWAP_CHAIN_BUFFER_COUNT 2
class SwapChain
{
public:
    SwapChain(HWND hWnd, RenderDevice* Device) { Create(hWnd, Device); }
    ~SwapChain() { Free(); }

    void Free() 
    {
        for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
        {
            m_BackBuffer[i].reset();
        }

        m_DepthStencilBuffer.reset();

        m_SwapChain = nullptr;
    }

    void Create(HWND hWnd, RenderDevice* Device);

    void Resize();

    void InitBuffersAndViews();

    DXGI_SWAP_CHAIN_DESC* GetDesc()
    {
        auto hr = m_SwapChain->GetDesc(&m_Desc);
        return &m_Desc;
    }

    void Present()
    {
        ASSERT_SUCCEEDED(m_SwapChain->Present(0, 0));
        m_CurrentBuffer = (m_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;
    }

    unsigned int GetMSAACount() { return MSAACount; }
    unsigned int GetMSAAQuality() { return MSAAQuality; }

    GpuResource* GetBackBuffer() { return m_BackBuffer[m_CurrentBuffer].get(); }
    GpuResource* GetDepthBuffer() { return m_DepthStencilBuffer.get(); }

    GpuResource* GetmsaaRenderTarget() { return m_msaaRenderTarget.get(); }
    GpuResource* GetmsaaDepthStencil() { return m_msaaDepthStencil.get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferView() { return m_BackBufferViewer[m_CurrentBuffer]->GetCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthBufferView() { return m_DepthStencilBufferViewer->GetCpuHandle(); }

    D3D12_CPU_DESCRIPTOR_HANDLE GetmsaaRenderTargetView() { return m_msaaRenderTargetViewer->GetCpuHandle(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetmsaaDepthStencilView() { return m_msaaDepthStencilViewer->GetCpuHandle(); }

    //DXGI_FORMAT GetBackBufferFormat() { return m_BackBufferFormat; }
    DXGI_FORMAT GetBackBufferSRGBFormat() { return m_BackBufferSRGBFormat; }
    DXGI_FORMAT GetDepthStencilFormat() { return m_DepthStencilFormat; }
private:
    RenderDevice* pDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;

    std::unique_ptr<Texture>m_BackBuffer[SWAP_CHAIN_BUFFER_COUNT];
    std::unique_ptr<Texture>m_DepthStencilBuffer;
    std::unique_ptr<TextureViewer>m_BackBufferViewer[SWAP_CHAIN_BUFFER_COUNT];
    std::unique_ptr<TextureViewer>m_DepthStencilBufferViewer;

    std::unique_ptr<Texture> m_msaaRenderTarget;
    std::unique_ptr<Texture> m_msaaDepthStencil;
    std::unique_ptr<TextureViewer>m_msaaRenderTargetViewer;
    std::unique_ptr<TextureViewer>m_msaaDepthStencilViewer;

    DXGI_SWAP_CHAIN_DESC m_Desc;

    //交换链创建的BackBuffer没有SRGB，但View是SRGB
    DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_BackBufferSRGBFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D32_FLOAT;

    unsigned int MSAACount = 4;
    unsigned int MSAAQuality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;

    //unsigned int MSAACount = 1;
    //unsigned int MSAAQuality = 0;

    UINT m_CurrentBuffer = 0;
};