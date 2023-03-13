#include "IBL.h"

IBL::IBL(RenderDevice* pDevice, TextureViewer* EnvMapView) :
	m_pDevice(pDevice),
	m_EnvMapView(EnvMapView)
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
	m_pDevice(pDevice),
	m_BrdfLookUpTableDim(BrdfLookUpTableDim),
	m_IrradianceEnvMapDim(IrradianceEnvMapDim),
	m_PrefilteredEnvMapDim(PrefilteredEnvMapDim),
	m_BrdfLookUpTableFmt(BrdfLookUpTableFmt),
	m_PreIrradianceEnvMapFmt(PreIrradianceEnvMapFmt),
	m_PreFilterEnvMapFmt(PreFilterEnvMapFmt),
	m_EnvMapView(EnvMapView)
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
	texDesc.Width = m_BrdfLookUpTableDim;
	texDesc.Height = m_BrdfLookUpTableDim;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = m_BrdfLookUpTableFmt;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = m_BrdfLookUpTableFmt;
	memcpy(clearColor.Color, Colors::Black, sizeof(float) * 4);
	m_BrdfLookUpTable.reset(new Texture(m_pDevice, texDesc, D3D12_RESOURCE_STATE_COMMON, clearColor, L"BrdfLookUpTable"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::RTV;
	texViewDesc.MostDetailedMip = 0;
	std::unique_ptr<TextureViewer>texView;
	texView.reset(new TextureViewer(m_pDevice, m_BrdfLookUpTable.get(), texViewDesc, false));

	//Shader
	ComPtr<ID3DBlob> PreComputeBRDFVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreComputeBRDFPsByteCode = nullptr;

	PreComputeBRDFVsByteCode = Utility::CompileShader(L"Shader\\PreBRDF.hlsl", nullptr, "VS", "vs_5_1");
	PreComputeBRDFPsByteCode = Utility::CompileShader(L"Shader\\PreBRDF.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	m_PreBRDFRS.reset(new RootSignature());

	m_PreBRDFRS->Finalize(L"PreBRDFRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, m_pDevice->g_Device);

	//PSO
	m_PreBRDFPSO.reset(new GraphicsPSO(L"PreBRDFPSO"));
	m_PreBRDFPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PreBRDFPSO->SetRenderTargetFormat(
		m_BrdfLookUpTableFmt,
		DXGI_FORMAT_UNKNOWN
	);
	m_PreBRDFPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	m_PreBRDFPSO->SetDepthStencilState(depthStencilDesc);
	CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	m_PreBRDFPSO->SetRasterizerState(rasterizerDesc);
	m_PreBRDFPSO->SetSampleMask(UINT_MAX);
	m_PreBRDFPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreComputeBRDFVsByteCode->GetBufferPointer()), PreComputeBRDFVsByteCode->GetBufferSize());
	m_PreBRDFPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreComputeBRDFPsByteCode->GetBufferPointer()), PreComputeBRDFPsByteCode->GetBufferSize());
	m_PreBRDFPSO->SetRootSignature(*(m_PreBRDFRS));

	m_PreBRDFPSO->Finalize(m_pDevice->g_Device);

	//PreComputeBRDF
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(m_BrdfLookUpTableDim);
	viewport.Height = static_cast<float>(m_BrdfLookUpTableDim);
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)m_BrdfLookUpTableDim;
	scissor.bottom = (LONG)m_BrdfLookUpTableDim;

	GraphicsContext& Context = m_pDevice->BeginGraphicsContext(L"PreComputeBRDF");
	Context.SetPipelineState(*m_PreBRDFPSO);
	Context.SetRootSignature(*m_PreBRDFRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.TransitionResource(m_BrdfLookUpTable.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	Context.SetViewportAndScissor(viewport, scissor);
	Context.SetRenderTargets(1, &texView->GetCpuHandle());
	Context.Draw(3);
	Context.Finish(true);
}

void IBL::PreComputeCubeMaps()
{
	//PreIrradianceEnvMap for Diffuse Light

	//Texture
	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = m_IrradianceEnvMapDim;
	texDesc.Height = m_IrradianceEnvMapDim;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = m_IrradianceEnvMapMipNum;
	texDesc.Format = m_PreIrradianceEnvMapFmt;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = m_PreIrradianceEnvMapFmt;
	memcpy(clearColor.Color, Colors::Black, sizeof(float) * 4);
	m_PreIrradianceEnvMap.reset(new Texture(m_pDevice, texDesc, D3D12_RESOURCE_STATE_COMMON, clearColor, L"PreIrradianceEnvMap"));

	//Shader
	ComPtr<ID3DBlob> PreIrradianceEnvMapVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreIrradianceEnvMapPsByteCode = nullptr;

	PreIrradianceEnvMapVsByteCode = Utility::CompileShader(L"Shader\\PreIrradianceEnvMap.hlsl", nullptr, "VS", "vs_5_1");
	PreIrradianceEnvMapPsByteCode = Utility::CompileShader(L"Shader\\PreIrradianceEnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	m_PreIrradianceEnvMapRS.reset(new RootSignature(2, 1));
	m_PreIrradianceEnvMapRS->InitStaticSampler(0, Sampler::LinearClamp);
	(*m_PreIrradianceEnvMapRS)[0].InitAsDescriptorTable(1);
	(*m_PreIrradianceEnvMapRS)[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*m_PreIrradianceEnvMapRS)[1].InitAsConstantBuffer(0);

	m_PreIrradianceEnvMapRS->Finalize(L"PreIrradianceEnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, m_pDevice->g_Device);

	//PSO
	m_PreIrradianceEnvMapPSO.reset(new GraphicsPSO(L"PreIrradianceEnvMapPSO"));
	m_PreIrradianceEnvMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PreIrradianceEnvMapPSO->SetRenderTargetFormat(
		m_PreIrradianceEnvMapFmt,
		DXGI_FORMAT_UNKNOWN
	);
	m_PreIrradianceEnvMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	m_PreIrradianceEnvMapPSO->SetDepthStencilState(depthStencilDesc);
	CD3DX12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	m_PreIrradianceEnvMapPSO->SetRasterizerState(rasterizerDesc);
	m_PreIrradianceEnvMapPSO->SetSampleMask(UINT_MAX);
	m_PreIrradianceEnvMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreIrradianceEnvMapVsByteCode->GetBufferPointer()), PreIrradianceEnvMapVsByteCode->GetBufferSize());
	m_PreIrradianceEnvMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreIrradianceEnvMapPsByteCode->GetBufferPointer()), PreIrradianceEnvMapPsByteCode->GetBufferSize());
	m_PreIrradianceEnvMapPSO->SetRootSignature(*(m_PreIrradianceEnvMapRS));

	m_PreIrradianceEnvMapPSO->Finalize(m_pDevice->g_Device);

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
	texDesc.Width = m_PrefilteredEnvMapDim;
	texDesc.Height = m_PrefilteredEnvMapDim;
	texDesc.MipLevels = m_PrefilteredEnvMapMipNum;
	texDesc.Format = m_PreFilterEnvMapFmt;

	clearColor.Format = m_PreFilterEnvMapFmt;
	m_PreFilterEnvMap.reset(new Texture(m_pDevice, texDesc, D3D12_RESOURCE_STATE_COMMON, clearColor, L"PreFilterEnvMap"));

	//Shader
	ComPtr<ID3DBlob> PreFilterEnvMapVsByteCode = nullptr;
	ComPtr<ID3DBlob> PreFilterEnvMapPsByteCode = nullptr;

	PreFilterEnvMapVsByteCode = Utility::CompileShader(L"Shader\\PreFilterEnvMap.hlsl", nullptr, "VS", "vs_5_1");
	PreFilterEnvMapPsByteCode = Utility::CompileShader(L"Shader\\PreFilterEnvMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	m_PreFilterEnvMapRS.reset(new RootSignature(2, 1));
	m_PreFilterEnvMapRS->InitStaticSampler(0, Sampler::LinearClamp);
	(*m_PreFilterEnvMapRS)[0].InitAsDescriptorTable(1);
	(*m_PreFilterEnvMapRS)[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*m_PreFilterEnvMapRS)[1].InitAsConstantBuffer(0);

	m_PreFilterEnvMapRS->Finalize(L"PreFilterEnvMapRootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, m_pDevice->g_Device);

	//PSO
	m_PreFilterEnvMapPSO.reset(new GraphicsPSO(L"PreFilterEnvMapPSO"));
	m_PreFilterEnvMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PreFilterEnvMapPSO->SetRenderTargetFormat(
		m_PreFilterEnvMapFmt,
		DXGI_FORMAT_UNKNOWN
	);
	m_PreFilterEnvMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_PreFilterEnvMapPSO->SetDepthStencilState(depthStencilDesc);
	m_PreFilterEnvMapPSO->SetRasterizerState(rasterizerDesc);
	m_PreFilterEnvMapPSO->SetSampleMask(UINT_MAX);
	m_PreFilterEnvMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(PreFilterEnvMapVsByteCode->GetBufferPointer()), PreFilterEnvMapVsByteCode->GetBufferSize());
	m_PreFilterEnvMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PreFilterEnvMapPsByteCode->GetBufferPointer()), PreFilterEnvMapPsByteCode->GetBufferSize());
	m_PreFilterEnvMapPSO->SetRootSignature(*(m_PreIrradianceEnvMapRS));
	*(m_PreIrradianceEnvMapRS);
	m_PreFilterEnvMapPSO->Finalize(m_pDevice->g_Device);

	//PreComputeCubeMaps
	//需要初始化，否则TopLeftX和TopLeftY是-100000000,导致光栅化失败
	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<float>(m_IrradianceEnvMapDim);
	viewport.Height = static_cast<float>(m_IrradianceEnvMapDim);
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)m_IrradianceEnvMapDim;
	scissor.bottom = (LONG)m_IrradianceEnvMapDim;

	const std::array<XMMATRIX, 6> rotations =
	{
		/* +X */XMMatrixRotationY(+MathHelper::PI_F / 2.f),
		/* -X */XMMatrixRotationY(-MathHelper::PI_F / 2.f),
		/* +Y */XMMatrixRotationX(-MathHelper::PI_F / 2.f),
		/* -Y */XMMatrixRotationX(+MathHelper::PI_F / 2.f),
		/* +Z */MathHelper::Identity(),
		/* -Z */XMMatrixRotationY(MathHelper::PI_F),
	};

	GraphicsContext& Context = m_pDevice->BeginGraphicsContext(L"PreComputeCubeMaps");
	Context.SetPipelineState(*m_PreIrradianceEnvMapPSO);
	Context.SetRootSignature(*m_PreIrradianceEnvMapRS);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	
	auto HeapInfo = m_EnvMapView->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetDescriptorTable(0, m_EnvMapView->GetGpuHandle());
	Context.TransitionResource(m_PreIrradianceEnvMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	int mipLevels = m_PreIrradianceEnvMap->GetDesc().MipLevels;
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
			pRTV.reset(new TextureViewer(m_pDevice, m_PreIrradianceEnvMap.get(), RTVDesc, false));

			XMFLOAT4X4 mat;
			XMStoreFloat4x4(&mat, XMMatrixTranspose(rotations[face]));

			//需要6个不同的DynamicAlloc，因为GPU正式执行绘制在Finish之后
			//这里如果循环外面构建一个Buffer，循环内反复Upload会导致只有最后一个矩阵被着色器使用，其他被覆盖
			std::unique_ptr<Buffer> rotationBuffer;
			rotationBuffer.reset(new Buffer(m_pDevice, nullptr, (UINT)(sizeof(XMFLOAT4X4)), true, true));
			rotationBuffer->Upload(&mat);

			Context.SetConstantBuffer(1, rotationBuffer->GetGpuVirtualAddress());
			viewport.Width = m_IrradianceEnvMapDim >> mip;
			viewport.Height = viewport.Width;
			Context.SetViewportAndScissor(viewport, scissor);
			Context.SetRenderTargets(1, &pRTV->GetCpuHandle());
			Context.Draw(4);
		}
	}
	Context.TransitionResource(m_PreIrradianceEnvMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	scissor.right = (LONG)m_PrefilteredEnvMapDim;
	scissor.bottom = (LONG)m_PrefilteredEnvMapDim;
	Context.SetPipelineState(*m_PreFilterEnvMapPSO);
	Context.SetRootSignature(*m_PreFilterEnvMapRS);
	Context.TransitionResource(m_PreFilterEnvMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	mipLevels = m_PreFilterEnvMap->GetDesc().MipLevels;
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
			pRTV.reset(new TextureViewer(m_pDevice, m_PreFilterEnvMap.get(), RTVDesc, false));

			XMStoreFloat4x4(&PreFilterEnvMapCB.Rotation, XMMatrixTranspose(rotations[face]));
			PreFilterEnvMapCB.Roughness = static_cast<float>(mip) / static_cast<float>(mipLevels);
			PreFilterEnvMapCB.NumSamples = 256;

			std::unique_ptr<Buffer> cbBuffer;
			cbBuffer.reset(new Buffer(m_pDevice, nullptr, (UINT)(sizeof(PreFilterEnvMapConstantBuffer)), true, true));
			cbBuffer->Upload(&PreFilterEnvMapCB);

			Context.SetConstantBuffer(1, cbBuffer->GetGpuVirtualAddress());
			viewport.Width = m_PrefilteredEnvMapDim >> mip;
			viewport.Height = viewport.Width;
			Context.SetViewportAndScissor(viewport, scissor);
			Context.SetRenderTargets(1, &pRTV->GetCpuHandle());
			Context.Draw(4);
		}
	}

	Context.Finish(true);
}

void IBL::CreateTextureView()
{
	std::vector<TextureViewerDesc>IBLTexViewDesc(3);
	IBLTexViewDesc[0].TexType = TextureType::Texture2D;
	IBLTexViewDesc[0].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[0].MostDetailedMip = 0;
	IBLTexViewDesc[0].NumMipLevels = m_BrdfLookUpTable->GetDesc().MipLevels;
	IBLTexViewDesc[1].TexType = TextureType::TextureCubeMap;
	IBLTexViewDesc[1].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[1].MostDetailedMip = 0;
	IBLTexViewDesc[1].NumMipLevels = m_PreIrradianceEnvMap->GetDesc().MipLevels;
	IBLTexViewDesc[2].TexType = TextureType::TextureCubeMap;
	IBLTexViewDesc[2].ViewerType = TextureViewerType::SRV;
	IBLTexViewDesc[2].MostDetailedMip = 0;
	IBLTexViewDesc[2].NumMipLevels = m_PreFilterEnvMap->GetDesc().MipLevels;
	std::vector<Texture*> IBLTexs = { m_BrdfLookUpTable.get(),m_PreIrradianceEnvMap.get(),m_PreFilterEnvMap.get() };

	m_IBLTexView.reset(new TextureViewer(m_pDevice, IBLTexs, IBLTexViewDesc.data(), true, IBLTexViewDesc.size()));
}

