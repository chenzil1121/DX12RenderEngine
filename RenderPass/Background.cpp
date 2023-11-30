#include "Background.h"

Background::Background(RenderDevice* pCore, SwapChain* pSwapChain, XMVECTORF32 top, XMVECTORF32 buttom):
	pCore(pCore),
	pSwapChain(pSwapChain)
{
	Create();
	SetBackgroudColor(top, buttom);
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void Background::Create()
{
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;
	ComPtr<ID3DBlob> GenXZPsByteCode = nullptr;
	ComPtr<ID3DBlob> GenYPsByteCode = nullptr;

	const D3D_SHADER_MACRO define[] =
	{
		"GenY","1",
		NULL, NULL
	};

	VsByteCode = Utility::CompileShader(L"Shader/Backgroud.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"Shader/Backgroud.hlsl", nullptr, "PS", "ps_5_1");
	GenXZPsByteCode = Utility::CompileShader(L"Shader/BackgroudGenEnvMap.hlsl", nullptr, "PS", "ps_5_1");
	GenYPsByteCode = Utility::CompileShader(L"Shader/BackgroudGenEnvMap.hlsl", define, "PS", "ps_5_1");

	//RootSignature
	backgroudRS.reset(new RootSignature(1));
	(*backgroudRS)[0].InitAsConstantBuffer(0);

	backgroudRS->Finalize(L"BackgroudPass RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//PSO
	backgroudPSO.reset(new GraphicsPSO(L"Backgroud PSO"));
	backgroudPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	backgroudPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		pSwapChain->GetDepthStencilFormat(),
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	backgroudPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	backgroudPSO->SetDepthStencilState(depthStencilDesc);
	backgroudPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	backgroudPSO->SetSampleMask(UINT_MAX);
	backgroudPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	backgroudPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	backgroudPSO->SetRootSignature(*backgroudRS);

	backgroudPSO->Finalize(pCore->g_Device);

	//GenXZPSO
	backgroudGenEnvMapXZPSO.reset(new GraphicsPSO(L"BackgroudGenEnvMapXZPSO"));
	backgroudGenEnvMapXZPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	std::array<DXGI_FORMAT, 4>rtvFormat = { DXGI_FORMAT_R16G16B16A16_FLOAT };
	rtvFormat.fill(DXGI_FORMAT_R16G16B16A16_FLOAT);
	backgroudGenEnvMapXZPSO->SetRenderTargetFormats(
		4,
		rtvFormat.data(),
		DXGI_FORMAT_UNKNOWN,
		1,
		0
	);
	backgroudGenEnvMapXZPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	depthStencilDesc.DepthEnable = FALSE;
	backgroudGenEnvMapXZPSO->SetDepthStencilState(depthStencilDesc);
	backgroudGenEnvMapXZPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	backgroudGenEnvMapXZPSO->SetSampleMask(UINT_MAX);
	backgroudGenEnvMapXZPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	backgroudGenEnvMapXZPSO->SetPixelShader(reinterpret_cast<BYTE*>(GenXZPsByteCode->GetBufferPointer()), GenXZPsByteCode->GetBufferSize());
	backgroudGenEnvMapXZPSO->SetRootSignature(*backgroudRS);

	backgroudGenEnvMapXZPSO->Finalize(pCore->g_Device);

	//GenYPSO
	backgroudGenEnvMapYPSO.reset(new GraphicsPSO(L"BackgroudGenEnvMapYPSO"));
	backgroudGenEnvMapYPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	backgroudGenEnvMapYPSO->SetRenderTargetFormats(
		2,
		rtvFormat.data(),
		DXGI_FORMAT_UNKNOWN,
		1,
		0
	);
	backgroudGenEnvMapYPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	backgroudGenEnvMapYPSO->SetDepthStencilState(depthStencilDesc);
	backgroudGenEnvMapYPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	backgroudGenEnvMapYPSO->SetSampleMask(UINT_MAX);
	backgroudGenEnvMapYPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	backgroudGenEnvMapYPSO->SetPixelShader(reinterpret_cast<BYTE*>(GenYPsByteCode->GetBufferPointer()), GenYPsByteCode->GetBufferSize());
	backgroudGenEnvMapYPSO->SetRootSignature(*backgroudRS);

	backgroudGenEnvMapYPSO->Finalize(pCore->g_Device);
}

void Background::GenEnvMap()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1024;
	texDesc.Height = 1024;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	backgroudEnvMapPositiveX.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapPositiveX"));
	backgroudEnvMapNegativeX.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapNegativeX"));
	backgroudEnvMapPositiveY.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapPositiveY"));
	backgroudEnvMapNegativeY.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapNegativeY"));
	backgroudEnvMapPositiveZ.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapPositiveZ"));
	backgroudEnvMapNegativeZ.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, L"BackgroudEnvMapNegativeZ"));

	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 1;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	backgroudEnvMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"BackgroudEnvMap"));

	//TextureView
	std::vector<Texture*> pRTs = 
	{ 
		backgroudEnvMapPositiveX.get(),
		backgroudEnvMapNegativeX.get(),
		backgroudEnvMapPositiveZ.get(),
		backgroudEnvMapNegativeZ.get()
	};
	std::array<TextureViewerDesc, 4>rtvViewDescs;
	for (size_t i = 0; i < 4; i++)
	{
		rtvViewDescs[i].TexType = TextureType::Texture2D;
		rtvViewDescs[i].ViewerType = TextureViewerType::RTV;
		rtvViewDescs[i].MostDetailedMip = 0;
	}
	backgroudEnvMapXZRTV.reset(new TextureViewer(pCore, pRTs, rtvViewDescs.data(), false));
	
	pRTs.clear();
	pRTs.push_back(backgroudEnvMapPositiveY.get());
	pRTs.push_back(backgroudEnvMapNegativeY.get());
	backgroudEnvMapYRTV.reset(new TextureViewer(pCore, pRTs, rtvViewDescs.data(), false));
	
	TextureViewerDesc srvViewDesc;
	srvViewDesc.TexType = TextureType::TextureCubeMap;
	srvViewDesc.ViewerType = TextureViewerType::SRV;
	srvViewDesc.MostDetailedMip = 0;
	srvViewDesc.NumMipLevels = 1;
	backgroudEnvMapSRV.reset(new TextureViewer(pCore, backgroudEnvMap.get(), srvViewDesc, true));

	//GenEnvMap
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(1024);
	viewport.Height = static_cast<float>(1024);
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)1024;
	scissor.bottom = (LONG)1024;

	//XZ
	GraphicsContext& Context = pCore->BeginGraphicsContext(L"BackgroudGenEnvMapXZ");
	Context.SetPipelineState(*backgroudGenEnvMapXZPSO);
	Context.SetRootSignature(*backgroudRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetViewportAndScissor(viewport, scissor);
	Context.SetConstantBuffer(0, backgroudCBuffer->GetGpuVirtualAddress());
	Context.SetRenderTargets(4, &backgroudEnvMapXZRTV->GetCpuHandle(0), true);
	Context.Draw(6);

	//Y
	Context.SetPipelineState(*backgroudGenEnvMapYPSO);
	Context.SetConstantBuffer(0, backgroudCBuffer->GetGpuVirtualAddress());
	Context.SetRenderTargets(2, &backgroudEnvMapYRTV->GetCpuHandle(0), true);
	Context.Draw(6);

	Context.TransitionResource(backgroudEnvMapPositiveX.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMapNegativeX.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMapPositiveY.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMapNegativeY.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMapPositiveZ.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMapNegativeZ.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(backgroudEnvMap.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	
	D3D12_TEXTURE_COPY_LOCATION dest = {};
	dest.pResource = backgroudEnvMap->GetResource();
	dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dest.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = backgroudEnvMapPositiveX->GetResource();
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);
	dest.SubresourceIndex = 1;
	src.pResource = backgroudEnvMapNegativeX->GetResource();
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);
	dest.SubresourceIndex = 2;
	src.pResource = backgroudEnvMapPositiveY->GetResource();
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);
	dest.SubresourceIndex = 3;
	src.pResource = backgroudEnvMapNegativeY->GetResource();
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);
	dest.SubresourceIndex = 4;
	src.pResource = backgroudEnvMapPositiveZ->GetResource();
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);
	dest.SubresourceIndex = 5;
	src.pResource = backgroudEnvMapNegativeZ->GetResource();
	Context.CopyTextureRegion(&dest, { 0,0,0 }, &src, nullptr);

	Context.TransitionResource(backgroudEnvMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.Finish(true);
}
 
void Background::Render(GraphicsContext& Context)
{
	Context.SetRootSignature(*backgroudRS);
	Context.SetPipelineState(*backgroudPSO);

	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView(), &pSwapChain->GetDepthBufferView());
	Context.SetConstantBuffer(0, backgroudCBuffer->GetGpuVirtualAddress());

	Context.Draw(6);
}

void Background::SetBackgroudColor(XMVECTORF32 top, XMVECTORF32 buttom) {
	XMStoreFloat4(&backgroudConstants.topColor, top);
	XMStoreFloat4(&backgroudConstants.buttomColor, buttom);
	backgroudCBuffer.reset(new Buffer(pCore, &backgroudConstants, sizeof(backgroudConstants), false, true));
}