#include "VarianceShadowMap.h"

VarianceShadowMap::VarianceShadowMap(RenderDevice* pCore, XMFLOAT3 LightPos, XMFLOAT3 LightDirection):
	pCore(pCore)
{
	Create();
	UpdateLightVP(LightPos, LightDirection);
}

void VarianceShadowMap::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"Shader/VarianceShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"Shader/VarianceShadowMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	VarianceShadowMapRS.reset(new RootSignature(2));
	(*VarianceShadowMapRS)[0].InitAsConstantBuffer(0);
	(*VarianceShadowMapRS)[1].InitAsConstantBuffer(1);

	VarianceShadowMapRS->Finalize(L"VarianceShadowMap RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//PSO
	VarianceShadowMapPSO.reset(new GraphicsPSO(L"VarianceShadowMapPSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	VarianceShadowMapPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	VarianceShadowMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	VarianceShadowMapPSO->SetRenderTargetFormat(
		shadowMapFormat,
		DXGI_FORMAT_D32_FLOAT
	);
	VarianceShadowMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	VarianceShadowMapPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	D3D12_RASTERIZER_DESC rasterizerStateDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerStateDesc.DepthBias = 2;
	rasterizerStateDesc.SlopeScaledDepthBias = 4;
	VarianceShadowMapPSO->SetRasterizerState(rasterizerStateDesc);
	VarianceShadowMapPSO->SetSampleMask(UINT_MAX);
	VarianceShadowMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	VarianceShadowMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	VarianceShadowMapPSO->SetRootSignature(*VarianceShadowMapRS);

	VarianceShadowMapPSO->Finalize(pCore->g_Device);

	//Texture
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = shadowMapDim;
	texDesc.Height = shadowMapDim;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = shadowMapFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = shadowMapFormat;
	clearColor.Color[0] = 1.0f;
	clearColor.Color[1] = 1.0f;
	clearColor.Color[2] = 1.0f;
	clearColor.Color[3] = 1.0f;

	varianceShadowMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"VarianceShadowMap"));

	texDesc.Format = DXGI_FORMAT_D32_FLOAT;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	clearColor.Format = DXGI_FORMAT_D32_FLOAT;
	clearColor.DepthStencil.Depth = 1.0f;
	clearColor.DepthStencil.Stencil = 0;

	depthBuffer.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, clearColor, L"VarianceShadowMap DepthBuffer"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::RTV;
	texViewDesc.MostDetailedMip = 0;
	varianceShadowMapRTV.reset(new TextureViewer(pCore, varianceShadowMap.get(), texViewDesc, false));

	texViewDesc.ViewerType = TextureViewerType::SRV;
	texViewDesc.NumMipLevels = varianceShadowMap->GetDesc().MipLevels;
	varianceShadowMapSRV.reset(new TextureViewer(pCore, varianceShadowMap.get(), texViewDesc, true));

	texViewDesc.ViewerType = TextureViewerType::DSV;
	texViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthBufferView.reset(new TextureViewer(pCore, depthBuffer.get(), texViewDesc, false));

	//创建视口和裁剪窗口，一般跟渲染窗口参数保持一致
	viewPort.TopLeftX = 0.0;
	viewPort.TopLeftY = 0.0;
	viewPort.Width = static_cast<float>(shadowMapDim);
	viewPort.Height = static_cast<float>(shadowMapDim);
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;

	scissor.left = 0;
	scissor.top = 0;
	scissor.right = (LONG)shadowMapDim;
	scissor.bottom = (LONG)shadowMapDim;
}

void VarianceShadowMap::UpdateLightVP(XMFLOAT3 LightPos, XMFLOAT3 LightDirection)
{
	XMVECTOR Dir = XMVector3Normalize(XMLoadFloat3(&LightDirection));
	XMVECTOR Pos = XMLoadFloat3(&LightPos);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookToLH(Pos, Dir, Up);

	float distance = 0.0f;
	XMStoreFloat(&distance, XMVector3Length(Pos - XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f)));

	shadowMapCnstants.NearZ = 0.1f;
	shadowMapCnstants.FarZ = 1000.0f;

	shadowMapCnstants.isOrthographic = TRUE;
	XMMATRIX P = XMMatrixOrthographicLH(distance, distance, 0.1f, 1000.0f);
	//shadowMapCnstants.isOrthographic = FALSE;
	//XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1.0f, 0.1f, 1000.0f);

	XMStoreFloat4x4(&shadowMapCnstants.LightMVP, XMMatrixTranspose(V * P));

	ShadowMapCB.reset(new Buffer(pCore, &shadowMapCnstants, sizeof(ShadowMapConstants), false, true, L"ShadowMapConstants"));
}

void VarianceShadowMap::Render(GraphicsContext& Context, std::vector<std::unique_ptr<Geometry>>& geos)
{
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetViewportAndScissor(viewPort, scissor);
	Context.TransitionResource(varianceShadowMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	Context.SetPipelineState(*VarianceShadowMapPSO);
	Context.SetRootSignature(*VarianceShadowMapRS);
	Context.SetRenderTargets(1, &varianceShadowMapRTV->GetCpuHandle(), &depthBufferView->GetCpuHandle());
	Context.ClearRenderTarget(varianceShadowMapRTV->GetCpuHandle(), DirectX::Colors::White, nullptr);
	Context.ClearDepthAndStencil(depthBufferView->GetCpuHandle(), 1.0f, 0);

	Context.SetConstantBuffer(0, ShadowMapCB->GetGpuVirtualAddress());
	for (size_t i = 0; i < geos.size(); i++)
	{
		for (size_t j = 0; j < geos[i]->size(); j++)
		{
			Mesh& mesh = geos[i]->Get(j);

			Context.SetVertexBuffer(0, mesh.GetVertexBufferView());
			Context.SetIndexBuffer(mesh.GetIndexBufferView());

			Context.SetConstantBuffer(1, mesh.GetConstantsBufferAddress());

			Context.DrawIndexedInstanced(mesh.GetIndexsInfo().first, 1, 0, 0, 0);
		}
	}
	Context.TransitionResource(varianceShadowMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
