#pragma once
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "Utility.h"
#include "RenderDevice.h"
class ImGuiWrapper
{
public:
	ImGuiWrapper(HWND hwnd, RenderDevice* pDevice,int RenderBufferNum, DXGI_FORMAT BackBufferFormat) :m_pDevice(pDevice)
	{
		m_FontSRV = m_pDevice->AllocateGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX12_Init(pDevice->g_Device, RenderBufferNum,
			BackBufferFormat, m_FontSRV.GetDescriptorHeap(),
			m_FontSRV.GetCpuHandle(),
			m_FontSRV.GetGpuHandle());
	}
	~ImGuiWrapper()
	{
		m_pDevice->FreeGPUDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_FontSRV));
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
	void NewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}
	void RenderDrawData(GraphicsContext& Context)
	{
		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_FontSRV.GetDescriptorHeap());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Context.GetCommandList());
	}

private:
	RenderDevice* m_pDevice;
	DescriptorHeapAllocation m_FontSRV;
};