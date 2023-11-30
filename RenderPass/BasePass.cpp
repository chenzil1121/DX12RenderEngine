#include "BasePass.h"
#include "Sampler.h"

BasePass::BasePass(RenderDevice* pCore, SwapChain* swapchain) :
	pCore(pCore),
	pSwapChain(swapchain)
{
	Create();
}

void BasePass::Create()
{
	//Shader
	ComPtr<ID3DBlob> VsByteCode = nullptr;
	ComPtr<ID3DBlob> PsByteCode = nullptr;

	VsByteCode = Utility::CompileShader(L"../RenderPass/Shader/BasePass.hlsl", nullptr, "VS", "vs_5_1");
	PsByteCode = Utility::CompileShader(L"../RenderPass/Shader/BasePass.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	BasePassRS.reset(new RootSignature(6, 2));
	BasePassRS->InitStaticSampler(0, Sampler::LinearWrap);
	BasePassRS->InitStaticSampler(1, Sampler::LinearClamp);
	(*BasePassRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
	(*BasePassRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3);
	(*BasePassRS)[2].InitAsConstantBuffer(0);
	(*BasePassRS)[3].InitAsConstantBuffer(1);
	(*BasePassRS)[4].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_ALL, 1);
	(*BasePassRS)[5].InitAsConstantBuffer(2);


	BasePassRS->Finalize(L"BasePass RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, pCore->g_Device);

	//OpaquePSO
	BasePassOpaquePSO.reset(new GraphicsPSO(L"BasePassPSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	BasePassOpaquePSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	BasePassOpaquePSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	BasePassOpaquePSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		pSwapChain->GetDepthStencilFormat(),
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	BasePassOpaquePSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	BasePassOpaquePSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	BasePassOpaquePSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	BasePassOpaquePSO->SetSampleMask(UINT_MAX);
	BasePassOpaquePSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	BasePassOpaquePSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	BasePassOpaquePSO->SetRootSignature(*BasePassRS);

	BasePassOpaquePSO->Finalize(pCore->g_Device);

	//BlendPSO
	BasePassBlendPSO.reset(new GraphicsPSO(L"BasePassPSO"));
	BasePassBlendPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	BasePassBlendPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	BasePassBlendPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		pSwapChain->GetDepthStencilFormat(),
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	CD3DX12_BLEND_DESC blendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendState.RenderTarget[0].BlendEnable = TRUE;
	blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	BasePassBlendPSO->SetBlendState(blendState);
	BasePassBlendPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	BasePassBlendPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	BasePassBlendPSO->SetSampleMask(UINT_MAX);
	BasePassBlendPSO->SetVertexShader(reinterpret_cast<BYTE*>(VsByteCode->GetBufferPointer()), VsByteCode->GetBufferSize());
	BasePassBlendPSO->SetPixelShader(reinterpret_cast<BYTE*>(PsByteCode->GetBufferPointer()), PsByteCode->GetBufferSize());
	BasePassBlendPSO->SetRootSignature(*BasePassRS);

	BasePassBlendPSO->Finalize(pCore->g_Device);
}

void BasePass::Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Scene* scene, IBL* ibl)
{
	Context.SetPipelineState(*BasePassOpaquePSO);
	Context.SetRootSignature(*BasePassRS);

	Context.SetConstantBuffer(3, PassConstantBuffer->GetGpuVirtualAddress());
	Context.SetBufferSRV(4, scene->m_LightBuffers->GetGpuVirtualAddress());
	auto IBLView = ibl->GetIBLView();
	auto IBLViewHeapInfo = IBLView->GetHeapInfo();
	Context.SetDescriptorHeap(IBLViewHeapInfo.first, IBLViewHeapInfo.second);
	Context.SetDescriptorTable(1, IBLView->GetGpuHandle());

	//Opaque Mesh
	for (size_t i = 0; i < scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)].size(); i++)
	{

		Mesh& mesh = scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)][i];
		Context.SetVertexBuffer(0, { mesh.m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Vertices.size() * sizeof(Vertex)),sizeof(Vertex) });
		Context.SetIndexBuffer({ mesh.m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Indices.size() * sizeof(uint32_t)),DXGI_FORMAT_R32_UINT });
		Context.SetConstantBuffer(2, mesh.m_ConstantsBuffer->GetGpuVirtualAddress());
		Context.SetConstantBuffer(5, scene->m_Materials[mesh.m_MatID].m_ConstantsBuffer->GetGpuVirtualAddress());

		auto FirstTexView = scene->m_Materials[mesh.m_MatID].GetTextureView();
		auto HeapInfo = FirstTexView->GetHeapInfo();
		Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
		Context.SetDescriptorTable(0, FirstTexView->GetGpuHandle());

		Context.DrawIndexed(mesh.m_Indices.size());
	}

	//Blend Mesh
	Context.SetPipelineState(*BasePassBlendPSO);
	for (size_t i = 0; i < scene->m_Meshes[static_cast<size_t>(LayerType::Blend)].size(); i++)
	{
		Mesh& mesh = scene->m_Meshes[static_cast<size_t>(LayerType::Blend)][i];
		Context.SetVertexBuffer(0, { mesh.m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Vertices.size() * sizeof(Vertex)),sizeof(Vertex) });
		Context.SetIndexBuffer({ mesh.m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Indices.size() * sizeof(uint32_t)),DXGI_FORMAT_R32_UINT });
		Context.SetConstantBuffer(2, mesh.m_ConstantsBuffer->GetGpuVirtualAddress());
		Context.SetConstantBuffer(5, scene->m_Materials[mesh.m_MatID].m_ConstantsBuffer->GetGpuVirtualAddress());

		auto FirstTexView = scene->m_Materials[mesh.m_MatID].GetTextureView();
		auto HeapInfo = FirstTexView->GetHeapInfo();
		Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
		Context.SetDescriptorTable(0, FirstTexView->GetGpuHandle());

		Context.DrawIndexed(mesh.m_Indices.size());
	}
}


