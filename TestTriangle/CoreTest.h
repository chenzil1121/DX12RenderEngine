#pragma once
#include <Windows.h>
#include <wrl.h>
#include <iostream>
#include"AppBase.h"
#include"RenderDevice.h"
#include"SwapChain.h"
#include"PipelineState.h"
#include"RootSignature.h"
#include"Buffer.h"
#include"GameTimer.h"
#include"Model.h"

struct VertexTri
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

class CoreTest :public AppBase
{
public:

    CoreTest(HINSTANCE hInstance) :AppBase(hInstance)
    {
        m_viewport.Width = static_cast<float>(m_Device.g_DisplayWidth);
        m_viewport.Height = static_cast<float>(m_Device.g_DisplayHeight);
        m_viewport.MaxDepth = 1.0f;
        m_viewport.MinDepth = 0.0f;

        m_scissor.left = 0;
        m_scissor.top = 0;
        m_scissor.right = (LONG)m_Device.g_DisplayWidth;
        m_scissor.bottom = (LONG)m_Device.g_DisplayHeight;
    }
    ~CoreTest() 
    {
        m_Device.Free();
        m_SwapChain.Free();
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
        m_RootSignature.Free();
        m_PSO.Free();

//#if _DEBUG
//        ID3D12DebugDevice* d3dDebug;
//        ASSERT_SUCCEEDED(m_Device.g_Device->QueryInterface(IID_PPV_ARGS(&d3dDebug)));
//        d3dDebug->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
//        d3dDebug->Release();
//#endif
        if (m_Device.g_Device)
        {
            m_Device.g_Device->Release();
            m_Device.g_Device = nullptr;
        }
    }

    bool Initialize();
    void CreateResources();
    void Update(const GameTimer& gt);
    void Render(const GameTimer& gt);
    void Present();
    void OnResize();

    SwapChain m_SwapChain;
    RootSignature m_RootSignature;
    GraphicsPSO m_PSO;
    std::unique_ptr<Buffer> m_VertexBuffer;
    std::unique_ptr<Buffer> m_IndexBuffer;
    std::vector<VertexTri> vertices;
    std::vector<std::uint16_t> indices;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor;
};