#include "FXAA.h"

FXAA::FXAA(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void FXAA::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"../RenderPass/Shader/FXAA.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"../RenderPass/Shader/FXAA.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	fxaaRS.reset(new RootSignature(2, 1));
	fxaaRS->InitStaticSampler(0, Sampler::LinearWrap);
	(*fxaaRS)[0].InitAsConstantBuffer(0);
	(*fxaaRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);

	fxaaRS->Finalize(L"FXAA RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);
	
	//PSO
	fxaaPSO.reset(new GraphicsPSO(L"fxaaPSO"));
	fxaaPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	fxaaPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		DXGI_FORMAT_UNKNOWN
	);
	fxaaPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	fxaaPSO->SetDepthStencilState(depthStencilDesc);
	fxaaPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	fxaaPSO->SetSampleMask(UINT_MAX);
	fxaaPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	fxaaPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	fxaaPSO->SetRootSignature(*fxaaRS);

	fxaaPSO->Finalize(pCore->g_Device);
}

void FXAA::CreateTexture()
{
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();
	inputColor.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"FXAAInput"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::SRV;
	texViewDesc.MostDetailedMip = 0;
	texViewDesc.NumMipLevels = 1;

	inputColorSRV.reset(new TextureViewer(pCore, inputColor.get(), texViewDesc, true));

	//ConstantBuffer
	FXAAConstants constants;
	constants.mrRcpTexDim[0] = 1 / (FLOAT)texDesc.Width;
	constants.mrRcpTexDim[1] = 1 / (FLOAT)texDesc.Height;

	constantsBuffer.reset(new Buffer(pCore, &constants, sizeof(FXAAConstants), false, true, L"FXAAConstants"));
}

void FXAA::Render(GraphicsContext& Context)
{
	//Copy Color
	Context.TransitionResource(inputColor.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.CopyResource(inputColor.get(), pSwapChain->GetBackBuffer());

	Context.TransitionResource(inputColor.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);


	Context.SetPipelineState(*fxaaPSO);
	Context.SetRootSignature(*fxaaRS);

	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView());

	Context.SetConstantBuffer(0, constantsBuffer->GetGpuVirtualAddress());
	Context.SetDescriptorTable(1, inputColorSRV->GetGpuHandle());

	Context.Draw(3);
}

