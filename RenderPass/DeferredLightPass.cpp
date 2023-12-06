#include "DeferredLightPass.h"

DeferredLightPass::DeferredLightPass(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
}

void DeferredLightPass::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"../RenderPass/Shader/DeferredLight.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"../RenderPass/Shader/DeferredLight.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	DeferredLightRS.reset(new RootSignature(6, 1));
	DeferredLightRS->InitStaticSampler(0, Sampler::LinearClamp);
	(*DeferredLightRS)[0].InitAsConstantBuffer(0);
	(*DeferredLightRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	(*DeferredLightRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 3);
	(*DeferredLightRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 1);
	(*DeferredLightRS)[4].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_ALL, 1);
	(*DeferredLightRS)[5].InitAsConstantBuffer(1);


	DeferredLightRS->Finalize(L"DeferredLight RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);
	
	//PSO
	DeferredLightPSO.reset(new GraphicsPSO(L"DeferredLightPSO"));
	DeferredLightPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	DeferredLightPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		DXGI_FORMAT_UNKNOWN,
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	DeferredLightPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	DeferredLightPSO->SetDepthStencilState(depthStencilDesc);
	DeferredLightPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	DeferredLightPSO->SetSampleMask(UINT_MAX);
	DeferredLightPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	DeferredLightPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	DeferredLightPSO->SetRootSignature(*DeferredLightRS);

	DeferredLightPSO->Finalize(pCore->g_Device);
}

void DeferredLightPass::Render(GraphicsContext& Context, Buffer* PassConstantBuffer, TextureViewer* GbufferSRV, TextureViewer* IBLView, VarianceShadowMap* vsm, Scene* scene)
{
	Context.SetPipelineState(*DeferredLightPSO);
	Context.SetRootSignature(*DeferredLightRS);
	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView());
	Context.SetConstantBuffer(0, PassConstantBuffer->GetGpuVirtualAddress());
	Context.SetDescriptorTable(1, GbufferSRV->GetGpuHandle());
	Context.SetDescriptorTable(2, IBLView->GetGpuHandle());
	Context.SetBufferSRV(4, scene->m_LightBuffers->GetGpuVirtualAddress());
	Context.SetConstantBuffer(5, vsm->GetShadowMapCB()->GetGpuVirtualAddress());
	Context.SetDescriptorTable(3, vsm->GetShadowMapSRV()->GetGpuHandle());
	Context.Draw(3);
}
