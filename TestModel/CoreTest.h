#pragma once
#include <Windows.h>
#include <wrl.h>
#include <iostream>
#include"AppBase.h"
#include"SwapChain.h"
#include"Scene.h"
#include"imGuIZMOquat.h"
#include"ImGui.h"
#include"Sampler.h"
#include"../RenderPass/IBL.h"
#include"../RenderPass/EnvMap.h"
#include"../RenderPass/BasePass.h"
#include"../RenderPass/GbufferPass.h"
#include"../RenderPass/FXAA.h"
#include"../RenderPass/VarianceShadowMap.h"
#include"../RenderPass/DeferredLightPass.h"
#include"../RenderPass/RayTracingPass.h"
#include"../RenderPass/SVGFPass.h"
#include"../RenderPass/ModulateIllumination.h"
//#define FOWARD
#define DEFFER
//#define DXR

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
    void UpdateUI();
    void Render(const GameTimer& gt);
    void Present();
    void OnResize();
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    void OnKeyboardInput(const GameTimer& gt);

    std::unique_ptr<SwapChain> m_SwapChain;
    std::unique_ptr<EnvMap> m_EnvMapPass;
    std::unique_ptr<IBL> m_IBL;
    std::unique_ptr<BasePass> m_BasePass;
    std::unique_ptr<Gbuffer> m_GbufferPass;
    std::unique_ptr<DeferredLightPass> m_DeferredLightPass;
    std::unique_ptr<FXAA> m_FXAA;
    std::unique_ptr<VarianceShadowMap> m_VSM;
    std::unique_ptr<RayTracingPass> m_RayTracingPass;
    std::unique_ptr<SVGFPass> m_svgfShadowPass;
    std::unique_ptr<SVGFPass> m_svgfReflectionPass;
    std::unique_ptr<ModulateIllumination> m_ModulateIlluminationPass;

    std::unique_ptr<Scene> m_Scene;

    std::unique_ptr<ImGuiWrapper> m_ImGui;
    std::unique_ptr<Buffer> m_PassConstantBuffer;

    quat m_SceneRotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
    vec3 m_LightDirection = vec3(-0.5f, -0.5f, 1.0f);

    PassConstants m_MainPassCB;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissor;
};