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
#include"MathHelper.h"
#include"Model.h"

struct VertexCube
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

class CoreTest :public AppBase
{
public:

    CoreTest(HINSTANCE hInstance, int windowsWidth, int windowsHeight) :AppBase(hInstance, windowsWidth, windowsHeight)
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
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    void OnKeyboardInput(const GameTimer& gt);


    std::unique_ptr<SwapChain> m_SwapChain;
    std::unique_ptr<RootSignature> m_RootSignature;
    std::unique_ptr<GraphicsPSO> m_PSO;
    std::unique_ptr<Buffer> m_VertexBuffer;
    std::unique_ptr<Buffer> m_IndexBuffer;
    std::unique_ptr<Buffer> m_ObjConstantBuffer;
    std::vector<VertexCube> vertices;
    std::vector<std::uint16_t> indices;

    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
    float Angle = 0.0f;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor;
};