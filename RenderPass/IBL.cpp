
#include "IBL.h"

IBL::IBL(RenderDevice* pDevice, TextureViewer* EnvMapView) :
	pCore(pDevice),
	envMapView(EnvMapView)
{
	PreComputeBRDF();
	PreComputeCubeMaps();
	CreateTextureView();
}

IBL::IBL(RenderDevice* pDevice,
	int BrdfLookUpTableDim,
	int IrradianceEnvMapDim,
	int PrefilteredEnvMapDim,
	DXGI_FORMAT BrdfLookUpTableFmt,
	DXGI_FORMAT PreIrradianceEnvMapFmt,
	DXGI_FORMAT PreFilterEnvMapFmt,
	TextureViewer* EnvMapView) :
	pCore(pDevice),
	brdfLookUpTableDim(BrdfLookUpTableDim),
	irradianceEnvMapDim(IrradianceEnvMapDim),
	prefilteredEnvMapDim(PrefilteredEnvMapDim),
	brdfLookUpTableFmt(BrdfLookUpTableFmt),
	preIrradianceEnvMapFmt(PreIrradianceEnvMapFmt),
	preFilterEnvMapFmt(PreFilterEnvMapFmt),
	envMapView(EnvMapView)
{
	PreComputeBRDF();
	PreComputeCubeMaps();
	CreateTextureView();
}

void IBL::PreComputeBRDF()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = brdfLookUpTableDim;
	texDesc.Height = brdfLookUpTableDim;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = brdfLookUpTableFmt;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = brdfLookUpTableFmt;
	memcpy(clearColor.Color, Colors::Black, sizeof(float) * 4);
	brdfLookUpTable.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"BrdfLookUpTable"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::RTV;
	texViewDesc.MostDetailedMip = 0;
	std::unique_ptr<TextureViewer>texView;
	texView.reset(new TextureViewer(pCore, brdfLookUpTable.get(), texViewDesc, false));

	//Shader
	ComPtr<ID3DBlob> PreComputeBRDFVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreComputeBRDFPsByteCode = nullptr;

	PreComputeBRDFVsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreBRDF.hlsl", nullptr, "VS", "vs_5_1");
	PreComputeBRDFPsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreBRDF.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	preBRDFRS.reset(new RootSignature());

	preBRDFRS->Finalize(L"PreBRDFRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//PSO
	preBRDFPSO.reset(new GraphicsPSO(L"PreBRDFPSO"));
	preBRDFPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	preBRDFPSO->SetRenderTargetFormat(
		brdfLookUpTableFmt,
		DXGI_FORMAT_UNKNOWN
	);
	preBRDFPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	preBRDFPSO->SetDepthStencilState(depthStencilDesc);
	CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	preBRDFPSO->SetRasterizerState(rasterizerDesc);
	preBRDFPSO->SetSampleMask(UINT_MAX);
	preBRDFPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreComputeBRDFVsByteCode->GetBufferPointer()), PreComputeBRDFVsByteCode->GetBufferSize());
	preBRDFPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreComputeBRDFPsByteCode->GetBufferPointer()), PreComputeBRDFPsByteCode->GetBufferSize());
	preBRDFPSO->SetRootSignature(*(preBRDFRS));

	preBRDFPSO->Finalize(pCore->g_Device);

	//PreComputeBRDF
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(brdfLookUpTableDim);
	viewport.Height = static_cast<float>(brdfLookUpTableDim);
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)brdfLookUpTableDim;
	scissor.bottom = (LONG)brdfLookUpTableDim;

	GraphicsContext& Context = pCore->BeginGraphicsContext(L"PreComputeBRDF");
	Context.SetPipelineState(*preBRDFPSO);
	Context.SetRootSignature(*preBRDFRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetViewportAndScissor(viewport, scissor);
	Context.SetRenderTargets(1, &texView->GetCpuHandle());
	Context.Draw(3);
	Context.TransitionResource(brdfLookUpTable.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.Finish(true);
}

void IBL::PreComputeCubeMaps()
{
	//PreIrradianceEnvMap for Diffuse Light

	//Texture
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = irradianceEnvMapDim;
	texDesc.Height = irradianceEnvMapDim;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = irradianceEnvMapMipNum;
	texDesc.Format = preIrradianceEnvMapFmt;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = preIrradianceEnvMapFmt;
	memcpy(clearColor.Color, Colors::Black, sizeof(float) * 4);
	preIrradianceEnvMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"PreIrradianceEnvMap"));

	//Shader
	ComPtr<ID3DBlob> PreIrradianceEnvMapVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreIrradianceEnvMapPsByteCode = nullptr;

	PreIrradianceEnvMapVsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreIrradianceEnvMap.hlsl", nullptr, "VS", "vs_5_1");
	PreIrradianceEnvMapPsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreIrradianceEnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	preIrradianceEnvMapRS.reset(new RootSignature(2, 1));
	preIrradianceEnvMapRS->InitStaticSampler(0, Sampler::LinearClamp);
	(*preIrradianceEnvMapRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*preIrradianceEnvMapRS)[1].InitAsConstantBuffer(0);

	preIrradianceEnvMapRS->Finalize(L"PreIrradianceEnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//PSO
	preIrradianceEnvMapPSO.reset(new GraphicsPSO(L"PreIrradianceEnvMapPSO"));
	preIrradianceEnvMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	preIrradianceEnvMapPSO->SetRenderTargetFormat(
		preIrradianceEnvMapFmt,
		DXGI_FORMAT_UNKNOWN
	);
	preIrradianceEnvMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	preIrradianceEnvMapPSO->SetDepthStencilState(depthStencilDesc);
	CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	preIrradianceEnvMapPSO->SetRasterizerState(rasterizerDesc);
	preIrradianceEnvMapPSO->SetSampleMask(UINT_MAX);
	preIrradianceEnvMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreIrradianceEnvMapVsByteCode->GetBufferPointer()), PreIrradianceEnvMapVsByteCode->GetBufferSize());
	preIrradianceEnvMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreIrradianceEnvMapPsByteCode->GetBufferPointer()), PreIrradianceEnvMapPsByteCode->GetBufferSize());
	preIrradianceEnvMapPSO->SetRootSignature(*(preIrradianceEnvMapRS));

	preIrradianceEnvMapPSO->Finalize(pCore->g_Device);

	//PreFilterEnvMap for Specular Light
	struct PreFilterEnvMapConstantBuffer
	{
		XMFLOAT4X4 Rotation;
		float Roughness;
		unsigned int NumSamples;
		float Unused1;
		float Unused2;
	}PreFilterEnvMapCB;

	//Texture
	texDesc.Width = prefilteredEnvMapDim;
	texDesc.Height = prefilteredEnvMapDim;
	texDesc.MipLevels = prefilteredEnvMapMipNum;
	texDesc.Format = preFilterEnvMapFmt;

	clearColor.Format = preFilterEnvMapFmt;
	preFilterEnvMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"PreFilterEnvMap"));

	//Shader
	ComPtr<ID3DBlob> PreFilterEnvMapVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreFilterEnvMapPsByteCode = nullptr;

	PreFilterEnvMapVsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreFilterEnvMap.hlsl", nullptr, "VS", "vs_5_1");
	PreFilterEnvMapPsByteCode = Utility::CompileShader(L"../RenderPass/Shader/PreFilterEnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	preFilterEnvMapRS.reset(new RootSignature(2, 1));
	preFilterEnvMapRS->InitStaticSampler(0, Sampler::LinearClamp);
	(*preFilterEnvMapRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*preFilterEnvMapRS)[1].InitAsConstantBuffer(0);

	preFilterEnvMapRS->Finalize(L"PreFilterEnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//PSO
	preFilterEnvMapPSO.reset(new GraphicsPSO(L"PreFilterEnvMapPSO"));
	preFilterEnvMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	preFilterEnvMapPSO->SetRenderTargetFormat(
		preFilterEnvMapFmt,
		DXGI_FORMAT_UNKNOWN
	);
	preFilterEnvMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	preFilterEnvMapPSO->SetDepthStencilState(depthStencilDesc);
	preFilterEnvMapPSO->SetRasterizerState(rasterizerDesc);
	preFilterEnvMapPSO->SetSampleMask(UINT_MAX);
	preFilterEnvMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreFilterEnvMapVsByteCode->GetBufferPointer()), PreFilterEnvMapVsByteCode->GetBufferSize());
	preFilterEnvMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreFilterEnvMapPsByteCode->GetBufferPointer()), PreFilterEnvMapPsByteCode->GetBufferSize());
	preFilterEnvMapPSO->SetRootSignature(*(preIrradianceEnvMapRS));
	preFilterEnvMapPSO->Finalize(pCore->g_Device);

	//PreComputeCubeMaps
	//需要初始化，否则TopLeftX和TopLeftY是-100000000,导致光栅化失败
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(irradianceEnvMapDim);
	viewport.Height = static_cast<float>(irradianceEnvMapDim);
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)irradianceEnvMapDim;
	scissor.bottom = (LONG)irradianceEnvMapDim;

	const std::array<XMMATRIX, 6> rotations =
	{
		/* +X */XMMatrixRotationY(+MathHelper::PI_F / 2.f),
		/* -X */XMMatrixRotationY(-MathHelper::PI_F / 2.f),
		/* +Y */XMMatrixRotationX(-MathHelper::PI_F / 2.f),
		/* -Y */XMMatrixRotationX(+MathHelper::PI_F / 2.f),
		/* +Z */MathHelper::Identity(),
		/* -Z */XMMatrixRotationY(MathHelper::PI_F),
	};

	GraphicsContext& Context = pCore->BeginGraphicsContext(L"PreComputeCubeMaps");
	Context.SetPipelineState(*preIrradianceEnvMapPSO);
	Context.SetRootSignature(*preIrradianceEnvMapRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	auto HeapInfo = envMapView->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetDescriptorTable(0, envMapView->GetGpuHandle());
	int mipLevels = preIrradianceEnvMap->GetDesc().MipLevels;
	for (size_t mip = 0; mip < mipLevels; mip++)
	{
		for (size_t face = 0; face < 6; face++)
		{
			//渲染了不同mipmap等级的纹理，Viewport也应该跟着改变!!!
			TextureViewerDesc RTVDesc;
			RTVDesc.TexType = TextureType::Texture2DArray;
			RTVDesc.ViewerType = TextureViewerType::RTV;
			RTVDesc.MostDetailedMip = mip;
			RTVDesc.FirstArraySlice = face;
			RTVDesc.NumArraySlices = 1;

			std::unique_ptr<TextureViewer>pRTV;
			pRTV.reset(new TextureViewer(pCore, preIrradianceEnvMap.get(), RTVDesc, false));

			XMFLOAT4X4 mat;
			XMStoreFloat4x4(&mat, XMMatrixTranspose(rotations[face]));

			//需要6个不同的DynamicAlloc，因为GPU正式执行绘制在Finish之后
			//这里如果循环外面构建一个Buffer，循环内反复Upload会导致只有最后一个矩阵被着色器使用，其他被覆盖
			std::unique_ptr<Buffer> rotationBuffer;
			rotationBuffer.reset(new Buffer(pCore, nullptr, (UINT)(sizeof(XMFLOAT4X4)), true, true));
			rotationBuffer->Upload(&mat);

			Context.SetConstantBuffer(1, rotationBuffer->GetGpuVirtualAddress());
			viewport.Width = irradianceEnvMapDim >> mip;
			viewport.Height = viewport.Width;
			Context.SetViewportAndScissor(viewport, scissor);
			Context.SetRenderTargets(1, &pRTV->GetCpuHandle());
			Context.Draw(4);
		}
	}

	scissor.right = (LONG)prefilteredEnvMapDim;
	scissor.bottom = (LONG)prefilteredEnvMapDim;
	Context.SetPipelineState(*preFilterEnvMapPSO);
	Context.SetRootSignature(*preFilterEnvMapRS);
	mipLevels = preFilterEnvMap->GetDesc().MipLevels;
	for (size_t mip = 0; mip < mipLevels; mip++)
	{
		for (size_t face = 0; face < 6; face++)
		{
			TextureViewerDesc RTVDesc;
			RTVDesc.TexType = TextureType::Texture2DArray;
			RTVDesc.ViewerType = TextureViewerType::RTV;
			RTVDesc.MostDetailedMip = mip;
			RTVDesc.FirstArraySlice = face;
			RTVDesc.NumArraySlices = 1;

			std::unique_ptr<TextureViewer>pRTV;
			pRTV.reset(new TextureViewer(pCore, preFilterEnvMap.get(), RTVDesc, false));

			XMStoreFloat4x4(&PreFilterEnvMapCB.Rotation, XMMatrixTranspose(rotations[face]));
			PreFilterEnvMapCB.Roughness = static_cast<float>(mip) / static_cast<float>(mipLevels);
			PreFilterEnvMapCB.NumSamples = 256;

			std::unique_ptr<Buffer> cbBuffer;
			cbBuffer.reset(new Buffer(pCore, nullptr, (UINT)(sizeof(PreFilterEnvMapConstantBuffer)), true, true));
			cbBuffer->Upload(&PreFilterEnvMapCB);

			Context.SetConstantBuffer(1, cbBuffer->GetGpuVirtualAddress());
			viewport.Width = prefilteredEnvMapDim >> mip;
			viewport.Height = viewport.Width;
			Context.SetViewportAndScissor(viewport, scissor);
			Context.SetRenderTargets(1, &pRTV->GetCpuHandle());
			Context.Draw(4);
		}
	}
	Context.TransitionResource(preIrradianceEnvMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(preFilterEnvMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.Finish(true);
}

void IBL::CreateTextureView()
{
	std::vector<TextureViewerDesc>IBLTexViewDesc(3);
	IBLTexViewDesc[0].TexType = TextureType::Texture2D;
	IBLTexViewDesc[0].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[0].MostDetailedMip = 0;
	IBLTexViewDesc[0].NumMipLevels = brdfLookUpTable->GetDesc().MipLevels;
	IBLTexViewDesc[1].TexType = TextureType::TextureCubeMap;
	IBLTexViewDesc[1].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[1].MostDetailedMip = 0;
	IBLTexViewDesc[1].NumMipLevels = preIrradianceEnvMap->GetDesc().MipLevels;
	IBLTexViewDesc[2].TexType = TextureType::TextureCubeMap;
	IBLTexViewDesc[2].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[2].MostDetailedMip = 0;
	IBLTexViewDesc[2].NumMipLevels = preFilterEnvMap->GetDesc().MipLevels;
	std::vector<Texture*> IBLTexs = { brdfLookUpTable.get(),preIrradianceEnvMap.get(),preFilterEnvMap.get() };

	IBLTexView.reset(new TextureViewer(pCore, IBLTexs, IBLTexViewDesc.data(), true));
}

