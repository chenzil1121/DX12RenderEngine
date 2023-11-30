#include "EnvMap.h"

EnvMap::EnvMap(RenderDevice* pCore, SwapChain* pSwapChain, std::string EnvMapPath) :
	pCore(pCore),
	pSwapChain(pSwapChain)
{
	envMap = TextureLoader::LoadTextureFromFile(EnvMapPath, pCore);
	TextureViewerDesc desc;
	desc.TexType = TextureType::TextureCubeMap;
	desc.ViewerType = TextureViewerType::SRV;
	desc.MostDetailedMip = 0;
	desc.NumMipLevels = envMap->GetDesc().MipLevels;
	envMapView.reset(new TextureViewer(pCore, envMap.get(), desc, true));

	Create();
}

void EnvMap::Create()
{
	//Shader
	ComPtr<ID3DBlob> EnvMapPassVsByteCode = nullptr;
	ComPtr<ID3DBlob> EnvMapPassPsByteCode = nullptr;

	EnvMapPassVsByteCode = Utility::CompileShader(L"../RenderPass/Shader/EnvMap.hlsl", nullptr, "VS", "vs_5_1");
	EnvMapPassPsByteCode = Utility::CompileShader(L"../RenderPass/Shader/EnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//EnvMap RootSignature
	envMapRS.reset(new RootSignature(2, 1));
	envMapRS->InitStaticSampler(0, Sampler::LinearWrap);
	(*envMapRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*envMapRS)[1].InitAsConstantBuffer(0);

	envMapRS->Finalize(L"EnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//EnvMap PSO
	envMapPSO.reset(new GraphicsPSO(L"EnvMapPSO"));
	envMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	envMapPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		pSwapChain->GetDepthStencilFormat(),
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	envMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	envMapPSO->SetDepthStencilState(depthStencilDesc);
	envMapPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	envMapPSO->SetSampleMask(UINT_MAX);
	envMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(EnvMapPassVsByteCode->GetBufferPointer()), EnvMapPassVsByteCode->GetBufferSize());
	envMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(EnvMapPassPsByteCode->GetBufferPointer()), EnvMapPassPsByteCode->GetBufferSize());
	envMapPSO->SetRootSignature(*envMapRS);

	envMapPSO->Finalize(pCore->g_Device);
}

void EnvMap::Render(GraphicsContext& Context, Buffer* PassConstantBuffer)
{
	Context.SetPipelineState(*envMapPSO);
	Context.SetRootSignature(*envMapRS);
	Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView(), &pSwapChain->GetDepthBufferView());
	Context.SetConstantBuffer(1, PassConstantBuffer->GetGpuVirtualAddress());
	auto HeapInfo = envMapView->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetDescriptorTable(0, envMapView->GetGpuHandle());
	Context.Draw(3);
}
