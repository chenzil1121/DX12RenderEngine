#include "OutLine.h"

OutLinePass::OutLinePass(RenderDevice* pCore, SwapChain* swapchain):
	pCore(pCore),
	pSwapChain(swapchain)
{
	Create();
}

void OutLinePass::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"Shader/OutLine.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"Shader/OutLine.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	OutLinePassRS.reset(new RootSignature(2));
	(*OutLinePassRS)[0].InitAsConstantBuffer(0);
	(*OutLinePassRS)[1].InitAsConstantBuffer(1);

	OutLinePassRS->Finalize(L"OutLinePass RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//PSO
	OutLinePassPSO.reset(new GraphicsPSO(L"OutLineBackFacePassPSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	OutLinePassPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	OutLinePassPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	OutLinePassPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		pSwapChain->GetDepthStencilFormat(),
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	OutLinePassPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	OutLinePassPSO->SetDepthStencilState(depthStencilDesc);
	D3D12_RASTERIZER_DESC rasterizerStateDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	rasterizerStateDesc.CullMode = D3D12_CULL_MODE_FRONT;
	rasterizerStateDesc.DepthBias = -1;
	rasterizerStateDesc.SlopeScaledDepthBias = -2;
	OutLinePassPSO->SetRasterizerState(rasterizerStateDesc);
	OutLinePassPSO->SetSampleMask(UINT_MAX);
	OutLinePassPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	OutLinePassPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	OutLinePassPSO->SetRootSignature(*OutLinePassRS);

	OutLinePassPSO->Finalize(pCore->g_Device);
}

void OutLinePass::Render(GraphicsContext& Context, Buffer* PassConstantBuffer, std::vector<std::unique_ptr<Geometry>>& geos)
{
	Context.SetPipelineState(*OutLinePassPSO);
	Context.SetRootSignature(*OutLinePassRS);

	Context.SetConstantBuffer(0, PassConstantBuffer->GetGpuVirtualAddress());

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
}
