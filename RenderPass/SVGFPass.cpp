#include "SVGFPass.h"

SVGFPass::SVGFPass(RenderDevice* pCore, SwapChain* swapChain, bool isDemodulate, int iterations):
	pCore(pCore),
	pSwapChain(swapChain),
	filterIterations(iterations)
{
	Create(isDemodulate);
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void SVGFPass::Create(bool isDemodulate)
{
	//Shader
	ComPtr<ID3DBlob> ReprojectByteCode = nullptr;
	ComPtr<ID3DBlob> EstimateVarByteCode = nullptr;
	ComPtr<ID3DBlob> AtrousByteCode = nullptr;
	ComPtr<ID3DBlob> ShadowTestVSByteCode = nullptr;
	ComPtr<ID3DBlob> ShadowTestPSByteCode = nullptr;

	if (isDemodulate)
	{
		const D3D_SHADER_MACRO define[] =
		{
			"REFLECT","1",
			NULL, NULL
		};

		ReprojectByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFReproject.hlsl", define, "main", "cs_5_1");
		EstimateVarByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFEstimateVar.hlsl", define, "main", "cs_5_1");
		AtrousByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFAtrous.hlsl", define, "main", "cs_5_1");
	}
	else
	{
		ReprojectByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFReproject.hlsl", nullptr, "main", "cs_5_1");
		EstimateVarByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFEstimateVar.hlsl", nullptr, "main", "cs_5_1");
		AtrousByteCode = Utility::CompileShader(L"../RenderPass/Shader/SVGFAtrous.hlsl", nullptr, "main", "cs_5_1");
	}

	//Reproject
	ReprojectRS.reset(new RootSignature(6));
	(*ReprojectRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*ReprojectRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 2);
	(*ReprojectRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 3);
	(*ReprojectRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 9, 1);
	(*ReprojectRS)[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 3);
	(*ReprojectRS)[5].InitAsConstants(0, 2);

	ReprojectRS->Finalize(L"Reproject RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);
	
	ReprojectPSO.reset(new ComputePSO(L"ReprojectPSO"));
	ReprojectPSO->SetComputeShader(reinterpret_cast<BYTE*>(ReprojectByteCode->GetBufferPointer()), ReprojectByteCode->GetBufferSize());
	ReprojectPSO->SetRootSignature(*ReprojectRS);

	ReprojectPSO->Finalize(pCore->g_Device);

	//EstimateVar
	EstimateVarRS.reset(new RootSignature(4));
	(*EstimateVarRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*EstimateVarRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 3);
	(*EstimateVarRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	(*EstimateVarRS)[3].InitAsConstants(0, 2);

	EstimateVarRS->Finalize(L"EstimateVar RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	EstimateVarPSO.reset(new ComputePSO(L"EstimateVarPSO"));
	EstimateVarPSO->SetComputeShader(reinterpret_cast<BYTE*>(EstimateVarByteCode->GetBufferPointer()), EstimateVarByteCode->GetBufferSize());
	EstimateVarPSO->SetRootSignature(*EstimateVarRS);

	EstimateVarPSO->Finalize(pCore->g_Device);

	//Atrous
	AtrousRS.reset(new RootSignature(4));
	(*AtrousRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*AtrousRS)[1].InitAsConstants(0, 3);
	(*AtrousRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);
	(*AtrousRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);

	AtrousRS->Finalize(L"Atrous RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	AtrousPSO.reset(new ComputePSO(L"AtrousPSO"));
	AtrousPSO->SetComputeShader(reinterpret_cast<BYTE*>(AtrousByteCode->GetBufferPointer()), AtrousByteCode->GetBufferSize());
	AtrousPSO->SetRootSignature(*AtrousRS);

	AtrousPSO->Finalize(pCore->g_Device);

}

void SVGFPass::CreateTexture()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();
	threadX = texDesc.Width / 8 + 1;
	threadY = texDesc.Height / 8 + 1;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	accIllumination.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"AccIllumination"));
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	accHistoryLength.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"AccHistoryLength"));
	texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	accMoments.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"AccMoments"));

	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	illuminationPing.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"IlluminationPing"));
	illuminationPong.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"IlluminationPong"));
	texDesc.Format = DXGI_FORMAT_R16_FLOAT;
	historyLength.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"HistoryLength"));
	texDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	moments.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"Moments"));
	
	//TextureViewer
	std::array<TextureViewerDesc, 3>texViewDescs;
	std::vector<Texture*> pRTs = { accIllumination.get(),accHistoryLength.get(),accMoments.get() };
	for (size_t i = 0; i < 3; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::SRV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	accSRV.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), true));

	pRTs.clear();
	pRTs.push_back(illuminationPing.get());
	pRTs.push_back(historyLength.get());
	pRTs.push_back(moments.get());
	for (size_t i = 0; i < 3; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::UAV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	curUAV.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), true));

	for (size_t i = 0; i < 3; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::SRV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	curSRV.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), true));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::UAV;
	texViewDesc.MostDetailedMip = 0;
	texViewDesc.NumMipLevels = 1;
	illuminationPingUAV.reset(new TextureViewer(pCore, illuminationPing.get(), texViewDesc, true));
	illuminationPongUAV.reset(new TextureViewer(pCore, illuminationPong.get(), texViewDesc, true));
	texViewDesc.ViewerType = TextureViewerType::SRV;
	illuminationPingSRV.reset(new TextureViewer(pCore, illuminationPing.get(), texViewDesc, true));
	illuminationPongSRV.reset(new TextureViewer(pCore, illuminationPong.get(), texViewDesc, true));
	
	ClearTexture();
}

void SVGFPass::ClearTexture()
{
	//Clear UAV need CPU Descriptor
	std::unique_ptr<TextureViewer> uavCPU;
	std::vector<Texture*> pRTs = { 
		accIllumination.get(),
		accHistoryLength.get(),
		accMoments.get(),
		illuminationPing.get(),
		historyLength.get(),
		moments.get(),
		illuminationPong.get()
	};
	std::array<TextureViewerDesc, 7>texViewDescs;
	for (size_t i = 0; i < 7; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::UAV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	uavCPU.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), false));

	std::unique_ptr<TextureViewer> accUAV;
	pRTs.clear();
	pRTs.push_back(accIllumination.get());
	pRTs.push_back(accHistoryLength.get());
	pRTs.push_back(accMoments.get());
	std::array<TextureViewerDesc, 3>accUAVViewDescs;
	for (size_t i = 0; i < 3; i++)
	{
		accUAVViewDescs[i].TexType = TextureType::Texture2D;
		accUAVViewDescs[i].ViewerType = TextureViewerType::UAV;
		accUAVViewDescs[i].MostDetailedMip = 0;
		accUAVViewDescs[i].NumMipLevels = 1;
	}
	accUAV.reset(new TextureViewer(pCore, pRTs, accUAVViewDescs.data(), true));

	std::array<float, 4>clearColorfloat4 = { 0.0f,0.0f,0.0f,0.0f };
	std::array<float, 2>clearColorfloat2 = { 0.0f,0.0f };
	std::array<float, 1>clearColorfloat = { 0.0f };
	//Clear UAV
	ComputeContext& Context = pCore->BeginComputeContext(L"Clear SVGF UAV");

	auto HeapInfo = curUAV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.ClearUAV(accUAV->GetGpuHandle(0), uavCPU->GetCpuHandle(0), accIllumination.get(), clearColorfloat4.data());
	Context.ClearUAV(accUAV->GetGpuHandle(1), uavCPU->GetCpuHandle(1), accHistoryLength.get(), clearColorfloat.data());
	Context.ClearUAV(accUAV->GetGpuHandle(2), uavCPU->GetCpuHandle(2), accMoments.get(), clearColorfloat2.data());
	Context.ClearUAV(curUAV->GetGpuHandle(0), uavCPU->GetCpuHandle(3), illuminationPing.get(), clearColorfloat4.data());
	Context.ClearUAV(curUAV->GetGpuHandle(1), uavCPU->GetCpuHandle(4), historyLength.get(), clearColorfloat.data());
	Context.ClearUAV(curUAV->GetGpuHandle(2), uavCPU->GetCpuHandle(5), moments.get(), clearColorfloat2.data());
	Context.ClearUAV(illuminationPongUAV->GetGpuHandle(0), uavCPU->GetCpuHandle(6), illuminationPong.get(), clearColorfloat4.data());
	
	Context.Finish(true);
}

void SVGFPass::Compute(ComputeContext& Context, TextureViewer* GbufferSRV, TextureViewer* PreNormalAndLinearZSRV, TextureViewer* RayTracingOutputSRV)
{
	//Reproject
	Context.TransitionResource(accIllumination.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(accHistoryLength.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(accMoments.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(illuminationPing.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(illuminationPong.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(historyLength.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(moments.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	Context.SetPipelineState(*ReprojectPSO);
	Context.SetRootSignature(*ReprojectRS);

	auto HeapInfo = GbufferSRV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, PreNormalAndLinearZSRV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetDescriptorTable(2, accSRV->GetGpuHandle(0));
	//RootParameter 3
	Context.SetDescriptorTable(3, RayTracingOutputSRV->GetGpuHandle(0));
	//RootParameter 4
	Context.SetDescriptorTable(4, curUAV->GetGpuHandle(0));
	//RootParameter 5
	std::array<float, 2>reprojectConstants = { alpha , momentsAlpha };
	Context.SetConstantArray(5, 2, reprojectConstants.data());

	Context.Dispatch(threadX, threadY, threadZ);

	Context.TransitionResource(accHistoryLength.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(accMoments.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(historyLength.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.TransitionResource(moments.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.CopyResource(accHistoryLength.get(), historyLength.get());
	Context.CopyResource(accMoments.get(), moments.get());

	Context.TransitionResource(illuminationPing.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(historyLength.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(moments.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

	//EstimateVar
	Context.SetPipelineState(*EstimateVarPSO);
	Context.SetRootSignature(*EstimateVarRS);

	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, curSRV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetDescriptorTable(2, illuminationPongUAV->GetGpuHandle(0));
	//RootParameter 3
	std::array<float, 2>EstimateVarConstants = { phiColor , phiNormal };
	Context.SetConstantArray(3, 2, EstimateVarConstants.data());

	Context.Dispatch(threadX, threadY, threadZ);

	//Atrous
	Context.SetPipelineState(*AtrousPSO);
	Context.SetRootSignature(*AtrousRS);

	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));

	std::array<Texture*, 2>pingPongTex = { illuminationPong.get() ,illuminationPing.get() };
	for (size_t i = 0; i < filterIterations; i++)
	{
		step = 1 << i;

		//RootParameter 1
		std::array<float, 3>AtrousConstants = { step , phiColor , phiNormal };
		Context.SetConstantArray(1, 3, AtrousConstants.data());

		Context.TransitionResource(pingPongTex[i % 2], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(pingPongTex[(i + 1) % 2], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		//RootParameter 2
		if (i % 2 == 0)
			Context.SetDescriptorTable(2, illuminationPongSRV->GetGpuHandle(0));
		else
			Context.SetDescriptorTable(2, illuminationPingSRV->GetGpuHandle(0));
		//RootParameter 3
		if ((i + 1) % 2 == 0)
			Context.SetDescriptorTable(3, illuminationPongUAV->GetGpuHandle(0));
		else
			Context.SetDescriptorTable(3, illuminationPingUAV->GetGpuHandle(0));

		Context.Dispatch(threadX, threadY, threadZ);
	}
	step = 0;
	if ((filterIterations) % 2 == 0)
	{
		Context.TransitionResource(illuminationPong.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Context.TransitionResource(accIllumination.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.CopyResource(accIllumination.get(), illuminationPong.get());

		Context.TransitionResource(illuminationPong.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}
	else
	{
		Context.TransitionResource(illuminationPing.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		Context.TransitionResource(accIllumination.get(), D3D12_RESOURCE_STATE_COPY_DEST);
		Context.CopyResource(accIllumination.get(), illuminationPing.get());

		Context.TransitionResource(illuminationPing.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}
}

