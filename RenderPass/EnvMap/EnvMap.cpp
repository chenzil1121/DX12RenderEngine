#include "EnvMap.h"

EnvMap::EnvMap(RenderDevice* pDevice, SwapChain* pSwapChain, std::string EnvMapPath) :
	m_pDevice(pDevice),
	m_pSwapChain(pSwapChain)
{
	m_EnvMap = LoadTextureFromFile(EnvMapPath, m_pDevice);
	TextureViewerDesc desc;
	desc.TexType = TextureType::TextureCubeMap;
	desc.ViewerType = TextureViewerType::SRV;
	desc.MostDetailedMip = 0;
	desc.NumMipLevels = m_EnvMap->GetDesc().MipLevels;
	m_EnvMapView.reset(new TextureViewer(m_pDevice, m_EnvMap.get(), desc, true));

	Create();
}

void EnvMap::Create()
{
	//Shader
	ComPtr<ID3DBlob> EnvMapPassVsByteCode = nullptr;
	ComPtr<ID3DBlob> EnvMapPassPsByteCode = nullptr;

	EnvMapPassVsByteCode = Utility::CompileShader(L"Shader\\EnvMap.hlsl", nullptr, "VS", "vs_5_1");
	EnvMapPassPsByteCode = Utility::CompileShader(L"Shader\\EnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//EnvMap RootSignature
	m_EnvMapRS.reset(new RootSignature(2, 1));
	m_EnvMapRS->InitStaticSampler(0, Sampler::LinearWrap);
	(*m_EnvMapRS)[0].InitAsDescriptorTable(1);
	(*m_EnvMapRS)[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*m_EnvMapRS)[1].InitAsConstantBuffer(0);

	m_EnvMapRS->Finalize(L"EnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, m_pDevice->g_Device);

	//EnvMap PSO
	m_EnvMapPSO.reset(new GraphicsPSO(L"EnvMapPSO"));
	m_EnvMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_EnvMapPSO->SetRenderTargetFormat(
		m_pSwapChain->GetBackBufferSRGBFormat(),
		m_pSwapChain->GetDepthStencilFormat(),
		m_pSwapChain->GetMSAACount(),
		m_pSwapChain->GetMSAAQuality()
	);
	m_EnvMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	m_EnvMapPSO->SetDepthStencilState(depthStencilDesc);
	m_EnvMapPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_EnvMapPSO->SetSampleMask(UINT_MAX);
	m_EnvMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(EnvMapPassVsByteCode->GetBufferPointer()), EnvMapPassVsByteCode->GetBufferSize());
	m_EnvMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(EnvMapPassPsByteCode->GetBufferPointer()), EnvMapPassPsByteCode->GetBufferSize());
	m_EnvMapPSO->SetRootSignature(*m_EnvMapRS);

	m_EnvMapPSO->Finalize(m_pDevice->g_Device);
}

void EnvMap::Render(GraphicsContext& Context, Buffer* PassConstantBuffer)
{
	Context.SetPipelineState(*m_EnvMapPSO);
	Context.SetRootSignature(*m_EnvMapRS);
	Context.SetConstantBuffer(1, PassConstantBuffer->GetGpuVirtualAddress());
	auto HeapInfo = m_EnvMapView->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetDescriptorTable(0, m_EnvMapView->GetGpuHandle());
	Context.Draw(3);
}
