#include "SwapChain.h"

void SwapChain::Create(HWND hWnd, RenderDevice* Device)
{
    m_SwapChain.Reset();
    pDevice = Device;

    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    ASSERT_SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = pDevice->g_DisplayWidth;
    swapChainDesc.BufferDesc.Height = pDevice->g_DisplayHeight;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;//60֡
    swapChainDesc.BufferDesc.Format = m_BackBufferFormat;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ASSERT_SUCCEEDED(mdxgiFactory->CreateSwapChain(
        pDevice->g_CommandManager.GetGraphicsQueue().GetCommandQueue(),
        &swapChainDesc,
        m_SwapChain.GetAddressOf()));

    InitBuffersAndViews();
}

void SwapChain::Resize()
{
    pDevice->g_CommandManager.IdleGPU();

    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
    {
        m_BackBuffer[i].reset();
    }

    m_DepthStencilBuffer.reset();

    ASSERT_SUCCEEDED(m_SwapChain->ResizeBuffers(
        SWAP_CHAIN_BUFFER_COUNT,
        pDevice->g_DisplayWidth, pDevice->g_DisplayHeight,
        m_BackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    InitBuffersAndViews();

    m_CurrentBuffer = 0;
}

void SwapChain::InitBuffersAndViews()
{
    D3D12_RESOURCE_DESC BackBufferDesc;
    for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
    {
        ComPtr<ID3D12Resource> pBackBuffer;
        ASSERT_SUCCEEDED(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer)));
        BackBufferDesc = pBackBuffer->GetDesc();
        m_BackBuffer[i].reset(new Texture(pDevice, BackBufferDesc, D3D12_RESOURCE_STATE_COMMON, pBackBuffer.Detach(), L"RenderBuffer"));

        TextureViewerDesc desc;
        desc.TexType = TextureType::Texture2D;
        desc.ViewerType = TextureViewerType::RTV;
        desc.Format = m_BackBufferSRGBFormat;
        desc.MostDetailedMip = 0;
        m_BackBufferViewer[i].reset(new TextureViewer(pDevice, m_BackBuffer[i].get(), desc, false));
    }

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = pDevice->g_DisplayWidth;
    depthStencilDesc.Height = pDevice->g_DisplayHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = m_DepthStencilFormat;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = m_DepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    TextureViewerDesc desc;
    m_DepthStencilBuffer.reset(new Texture(pDevice, depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, optClear, L"DepthBuffer"));
    desc.TexType = TextureType::Texture2D;
    desc.ViewerType = TextureViewerType::DSV;
    desc.MostDetailedMip = 0;
    m_DepthStencilBufferViewer.reset(new TextureViewer(pDevice, m_DepthStencilBuffer.get(), desc, false));

    //MSAA Resource
    BackBufferDesc.SampleDesc.Count = MSAACount;
    BackBufferDesc.SampleDesc.Quality = MSAAQuality;
    D3D12_CLEAR_VALUE msaaOptimizedClearValue = {};	
    msaaOptimizedClearValue.Format = m_BackBufferSRGBFormat;
    memcpy(msaaOptimizedClearValue.Color, Colors::Gray, sizeof(float) * 4);
    m_msaaRenderTarget.reset(new Texture(pDevice, BackBufferDesc, D3D12_RESOURCE_STATE_COMMON, msaaOptimizedClearValue, L"MSAA RenderTarget"));
    desc.ViewerType = TextureViewerType::RTV;
    desc.Format = m_BackBufferSRGBFormat;
    m_msaaRenderTargetViewer.reset(new TextureViewer(pDevice, m_msaaRenderTarget.get(), desc, false));

    depthStencilDesc.SampleDesc.Count = MSAACount;
    depthStencilDesc.SampleDesc.Quality = MSAAQuality;
    m_msaaDepthStencil.reset(new Texture(pDevice, depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, optClear, L"MSAA DepthStencil"));
    desc.ViewerType = TextureViewerType::DSV;
    desc.Format = m_DepthStencilFormat;
    m_msaaDepthStencilViewer.reset(new TextureViewer(pDevice, m_msaaDepthStencil.get(), desc, false));
}