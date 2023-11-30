#include "TAA.h"

TAA::TAA(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void TAA::Create()
{
	//Shader
	ComPtr<ID3DBlob> ByteCode = nullptr;

	ByteCode = Utility::CompileShader(L"Shader/TAA.hlsl", nullptr, "main", "cs_5_1");

	//RootSignature
	taaRS.reset(new RootSignature(4, 1));
	taaRS->InitStaticSampler(0, Sampler::LinearWrap);
	(*taaRS)[0].InitAsConstants(0, 2);
	(*taaRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*taaRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 2);
	(*taaRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);

	taaRS->Finalize(L"TAA RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);
	
	//PSO
	taaPSO.reset(new ComputePSO(L"TAA PSO"));
	taaPSO->SetComputeShader(reinterpret_cast<BYTE*>(ByteCode->GetBufferPointer()), ByteCode->GetBufferSize());
	taaPSO->SetRootSignature(*taaRS);

	taaPSO->Finalize(pCore->g_Device);
}

void TAA::CreateTexture()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();
	threadX = texDesc.Width / 8 + 1;
	threadY = texDesc.Height / 8 + 1;

	curColor.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"CurColor"));
	prevColor.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_COMMON, L"PrevColor"));

	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	output.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"TAA output"));

	std::array<TextureViewerDesc, 2>texViewDescs;
	std::vector<Texture*> pRTs = { curColor.get(),prevColor.get() };
	for (size_t i = 0; i < 2; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::SRV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	colorSRV.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), true));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::UAV;
	texViewDesc.MostDetailedMip = 0;
	texViewDesc.NumMipLevels = 1;
	outputUAV.reset(new TextureViewer(pCore, output.get(), texViewDesc, true));
}

void TAA::Compute(ComputeContext& Context, TextureViewer* GbufferSRV)
{
	if (frameCount)
	{
		Context.TransitionResource(curColor.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Context.CopyResource(curColor.get(), pSwapChain->GetBackBuffer());

		Context.TransitionResource(curColor.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(prevColor.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(output.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		Context.SetPipelineState(*taaPSO);
		Context.SetRootSignature(*taaRS);

		std::array<float, 2>Constants = { alpha , colorBoxSigma };
		Context.SetConstantArray(0, 2, Constants.data());
		Context.SetDescriptorTable(1, GbufferSRV->GetGpuHandle());
		Context.SetDescriptorTable(2, colorSRV->GetGpuHandle());
		Context.SetDescriptorTable(3, outputUAV->GetGpuHandle());

		Context.Dispatch(threadX, threadY, threadZ);

		Context.TransitionResource(output.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Context.TransitionResource(prevColor.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.CopyResource(prevColor.get(), output.get());
		Context.CopyResource(pSwapChain->GetBackBuffer(), output.get());
	}
	else
	{
		Context.TransitionResource(prevColor.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.TransitionResource(pSwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Context.CopyResource(prevColor.get(), pSwapChain->GetBackBuffer());
	}
	frameCount++;
}
