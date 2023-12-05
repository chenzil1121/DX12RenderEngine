#include"GbufferPass.h"

Gbuffer::Gbuffer(RenderDevice* device,SwapChain* swapchain) :
	pCore(device),
	pSwapChain(swapchain)
{
	Create();
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void Gbuffer::Create() {

	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"../RenderPass/Shader/Gbuffer.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"../RenderPass/Shader/Gbuffer.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	GbufferRS.reset(new RootSignature(4, 1));
	GbufferRS->InitStaticSampler(0, Sampler::LinearWrap);
	(*GbufferRS)[0].InitAsConstantBuffer(0);
	(*GbufferRS)[1].InitAsConstantBuffer(1);
	(*GbufferRS)[2].InitAsConstantBuffer(2);
	(*GbufferRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);

	GbufferRS->Finalize(L"GbufferPass RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//PSO
	GbufferPSO.reset(new GraphicsPSO(L"GbufferPSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	GbufferPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	GbufferPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	std::vector<DXGI_FORMAT>rtvFormats;
	for (int i = 0; i < NUMRT; i++)
	{
		rtvFormats.push_back(GbufferInfo[i].second);
	}
	GbufferPSO->SetRenderTargetFormats(NUMRT, rtvFormats.data(), pSwapChain->GetDepthStencilFormat(), 1, 0);

	GbufferPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	GbufferPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	GbufferPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	GbufferPSO->SetSampleMask(UINT_MAX);
	GbufferPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	GbufferPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	GbufferPSO->SetRootSignature(*GbufferRS);

	GbufferPSO->Finalize(pCore->g_Device);
}

void Gbuffer::CreateTexture()
{
	//GBuffer
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();

	for (int i = 0; i < NUMRT; i++) {
		D3D12_CLEAR_VALUE clearColor;
		clearColor.Format = GbufferInfo[i].second;
		memcpy(clearColor.Color, rtvClearColors[i].data(), sizeof(float) * 4);
		texDesc.Format = GbufferInfo[i].second;
		RTs[i].reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, GbufferInfo[i].first.c_str()));
	}

	std::vector<Texture*> pRTs;
	std::array<TextureViewerDesc, NUMRT>rtvViewDescs;
	for (int i = 0; i < NUMRT; i++)
	{
		rtvViewDescs[i].TexType = TextureType::Texture2D;
		rtvViewDescs[i].ViewerType = TextureViewerType::RTV;
		rtvViewDescs[i].MostDetailedMip = 0;

		pRTs.push_back(RTs[i].get());
	}
	GbufferRTV.reset(new TextureViewer(pCore, pRTs, rtvViewDescs.data(), false));

	std::array<TextureViewerDesc, NUMRT>srvViewDescs;
	for (int i = 0; i < NUMRT; i++)
	{
		srvViewDescs[i].TexType = TextureType::Texture2D;
		srvViewDescs[i].ViewerType = TextureViewerType::SRV;
		srvViewDescs[i].MostDetailedMip = 0;
		srvViewDescs[i].NumMipLevels = 1;
	}
	GbufferSRV.reset(new TextureViewer(pCore, pRTs, srvViewDescs.data(), true));

	//PreBuffer
	texDesc = pSwapChain->GetBackBufferDesc();
	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	memcpy(clearColor.Color, rtvClearColors[0].data(), sizeof(float) * 4);
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	PreNormalMetallic.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"PreNormalMetallic"));
	PreMotionLinearZ.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"PreMotionLinearZ"));
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	PreObjectID.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"PreObjectID"));

	std::array<TextureViewerDesc, 3>preSrvViewDescs;
	for (int i = 0; i < preSrvViewDescs.size(); i++)
	{
		preSrvViewDescs[i].TexType = TextureType::Texture2D;
		preSrvViewDescs[i].ViewerType = TextureViewerType::SRV;
		preSrvViewDescs[i].MostDetailedMip = 0;
		preSrvViewDescs[i].NumMipLevels = 1;
	}
	pRTs.clear();
	pRTs.push_back(PreNormalMetallic.get());
	pRTs.push_back(PreMotionLinearZ.get());
	pRTs.push_back(PreObjectID.get());

	PreSRV.reset(new TextureViewer(pCore, pRTs, preSrvViewDescs.data(), true));
}

void Gbuffer::Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Scene* scene)
{
	//Copy to Pre
	Context.TransitionResource(RTs[2].get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(RTs[3].get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(RTs[4].get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(PreNormalMetallic.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(PreMotionLinearZ.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(PreObjectID.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.CopyResource(PreNormalMetallic.get(), RTs[2].get());
	Context.CopyResource(PreMotionLinearZ.get(), RTs[3].get());
	Context.CopyResource(PreObjectID.get(), RTs[4].get());

	for (int i = 0; i < NUMRT; i++)
		Context.TransitionResource(RTs[i].get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	Context.FlushResourceBarriers();

	for (int i = 0; i < NUMRT; i++)
		Context.ClearRenderTarget(GbufferRTV->GetCpuHandle(i), rtvClearColors[i].data(), nullptr);

	Context.SetRenderTargets(NUMRT, &GbufferRTV->GetCpuHandle(0), true, &pSwapChain->GetDepthBufferView());
	Context.SetPipelineState(*GbufferPSO);
	Context.SetRootSignature(*GbufferRS);

	Context.SetConstantBuffer(0, PassConstantBuffer->GetGpuVirtualAddress());

	for (size_t i = 0; i < scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)].size(); i++)
	{
		Mesh& mesh = scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)][i];

		Context.SetVertexBuffer(0, mesh.GetVertexBufferView());
		Context.SetIndexBuffer(mesh.GetIndexBufferView());

		Context.SetConstantBuffer(1, mesh.m_ConstantsBuffer->GetGpuVirtualAddress());
		Context.SetConstantBuffer(2, scene->m_Materials[mesh.m_MatID].m_ConstantsBuffer->GetGpuVirtualAddress());

		auto FirstTexView = scene->m_Materials[mesh.m_MatID].GetTextureView();
		auto HeapInfo = FirstTexView->GetHeapInfo();
		Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
		Context.SetDescriptorTable(3, FirstTexView->GetGpuHandle());

		Context.DrawIndexed(mesh.m_Indices.size());
	}

	for (int i = 0; i < NUMRT; i++)
		Context.TransitionResource(RTs[i].get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(PreNormalMetallic.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(PreMotionLinearZ.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(PreObjectID.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.FlushResourceBarriers();
}