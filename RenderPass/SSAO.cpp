#include "SSAO.h"

SSAO::SSAO(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void SSAO::Create()
{
	//Shader
	ComPtr<ID3DBlob> ByteCode = nullptr;

	ByteCode = Utility::CompileShader(L"Shader/SSAO.hlsl", nullptr, "main", "cs_5_1");

	//RootSignature
	SSAOPassRS.reset(new RootSignature(3));
	(*SSAOPassRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	(*SSAOPassRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	(*SSAOPassRS)[2].InitAsConstantBuffer(0);

	SSAOPassRS->Finalize(L"SSAO RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//PSO
	SSAOPassPSO.reset(new ComputePSO(L"SSAO PSO"));
	SSAOPassPSO->SetComputeShader(reinterpret_cast<BYTE*>(ByteCode->GetBufferPointer()), ByteCode->GetBufferSize());
	SSAOPassPSO->SetRootSignature(*SSAOPassRS);

	SSAOPassPSO->Finalize(pCore->g_Device);
}

void SSAO::CreateTexture()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();
	threadX = texDesc.Width / 8 + 1;
	threadY = texDesc.Height / 8 + 1;

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;

	ambientOcclusionMap.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"AmbientOcclusionMap"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::UAV;
	texViewDesc.MostDetailedMip = 0;
	texViewDesc.NumMipLevels = 1;
	UAV.reset(new TextureViewer(pCore, ambientOcclusionMap.get(), texViewDesc, true));
	texViewDesc.ViewerType = TextureViewerType::SRV;
	SRV.reset(new TextureViewer(pCore, ambientOcclusionMap.get(), texViewDesc, true));
}

void SSAO::Compute(ComputeContext& Context, Buffer* PassConstantBuffer, TextureViewer* GbufferSRV)
{
	Context.TransitionResource(ambientOcclusionMap.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	Context.SetPipelineState(*SSAOPassPSO);
	Context.SetRootSignature(*SSAOPassRS);

	auto HeapInfo = GbufferSRV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);

	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle());
	Context.SetDescriptorTable(1, UAV->GetGpuHandle());
	Context.SetConstantBuffer(2, PassConstantBuffer->GetGpuVirtualAddress());

	Context.Dispatch(threadX, threadY, threadZ);

	Context.TransitionResource(ambientOcclusionMap.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}
