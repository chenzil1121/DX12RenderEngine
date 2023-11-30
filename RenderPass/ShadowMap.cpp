#include "ShadowMap.h"

ShadowMap::ShadowMap(RenderDevice* pCore, XMFLOAT3 LightPos, XMFLOAT3 LightDirection):
	pCore(pCore)
{
	Create();
	UpdateLightVP(LightPos, LightDirection);
}

void ShadowMap::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"Shader/ShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"Shader/ShadowMap.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	ShadowMapRS.reset(new RootSignature(2));
	(*ShadowMapRS)[0].InitAsConstantBuffer(0);
	(*ShadowMapRS)[1].InitAsConstantBuffer(1);

	ShadowMapRS->Finalize(L"ShadowMap RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//PSO
	ShadowMapPSO.reset(new GraphicsPSO(L"ShadowMapPSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } 
	};
	ShadowMapPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	ShadowMapPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	ShadowMapPSO->SetDepthTargetFormat(
		DXGI_FORMAT_D32_FLOAT
	);
	ShadowMapPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	ShadowMapPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	D3D12_RASTERIZER_DESC rasterizerStateDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerStateDesc.DepthBias = 2;
	rasterizerStateDesc.SlopeScaledDepthBias = 4;
	ShadowMapPSO->SetRasterizerState(rasterizerStateDesc);
	ShadowMapPSO->SetSampleMask(UINT_MAX);
	ShadowMapPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	ShadowMapPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	ShadowMapPSO->SetRootSignature(*ShadowMapRS);

	ShadowMapPSO->Finalize(pCore->g_Device);

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
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = DXGI_FORMAT_D32_FLOAT;
	clearColor.DepthStencil.Depth = 1.0f;
	clearColor.DepthStencil.Stencil = 0;

	shadowMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, clearColor, L"ShadowMap"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::DSV;
	texViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	texViewDesc.MostDetailedMip = 0;
	shadowMapDSV.reset(new TextureViewer(pCore, shadowMap.get(), texViewDesc, false));

	texViewDesc.ViewerType = TextureViewerType::SRV;
	texViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texViewDesc.NumMipLevels = shadowMap->GetDesc().MipLevels;
	shadowMapSRV.reset(new TextureViewer(pCore, shadowMap.get(), texViewDesc, true));
	
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

void ShadowMap::UpdateLightVP(XMFLOAT3 LightPos, XMFLOAT3 LightDirection)
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

void ShadowMap::Render(GraphicsContext& Context, std::vector<std::unique_ptr<Geometry>>& geos)
{
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetViewportAndScissor(viewPort, scissor);
	Context.TransitionResource(shadowMap.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	Context.SetPipelineState(*ShadowMapPSO);
	Context.SetRootSignature(*ShadowMapRS);
	Context.SetRenderTargets(0, NULL, &shadowMapDSV->GetCpuHandle());
	Context.ClearDepthAndStencil(shadowMapDSV->GetCpuHandle(), 1.0f, 0);

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
	Context.TransitionResource(shadowMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
