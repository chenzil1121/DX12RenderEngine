#include "DebugView.h"

DebugView::DebugView(RenderDevice* pCore, SwapChain* pSwapChain):
	pCore(pCore),
	pSwapChain(pSwapChain)
{
	Create();
}

void DebugView::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"Shader\\DebugView.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"Shader\\DebugView.hlsl", nullptr, "PS", "ps_5_1");

	//DebugView RootSignature
	DebugViewRS.reset(new RootSignature(1));
	(*DebugViewRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);

	DebugViewRS->Finalize(L"DebugView RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);
	
	//DebugView PSO
	DebugViewPSO.reset(new GraphicsPSO(L"DebugView PSO"));
	DebugViewPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	DebugViewPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		DXGI_FORMAT_UNKNOWN,
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	DebugViewPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	DebugViewPSO->SetDepthStencilState(depthStencilDesc);
	DebugViewPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	DebugViewPSO->SetSampleMask(UINT_MAX);
	DebugViewPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	DebugViewPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	DebugViewPSO->SetRootSignature(*DebugViewRS);

	DebugViewPSO->Finalize(pCore->g_Device);
}

void DebugView::Render(GraphicsContext& Context, TextureViewer* debugSRV)
{
	Context.SetPipelineState(*DebugViewPSO);
	Context.SetRootSignature(*DebugViewRS);
	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView());
	auto HeapInfo = debugSRV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetDescriptorTable(0, debugSRV->GetGpuHandle());
	Context.Draw(3);
}



