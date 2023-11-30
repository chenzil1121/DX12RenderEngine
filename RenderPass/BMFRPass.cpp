#include "BMFRPass.h"

BMFRPass::BMFRPass(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void BMFRPass::Create()
{
	//Shader
	ComPtr<ID3DBlob> PreprocessByteCode = nullptr;
	ComPtr<ID3DBlob> RegressionByteCode = nullptr;
	ComPtr<ID3DBlob> PostprocessByteCode = nullptr;
	ComPtr<ID3DBlob> ReflectionTestVSByteCode = nullptr;
	ComPtr<ID3DBlob> ReflectionTestPSByteCode = nullptr;

	PreprocessByteCode = Utility::CompileShader(L"Shader/BMFRPreprocess.hlsl", nullptr, "main", "cs_5_1");
	RegressionByteCode = Utility::CompileShader(L"Shader/BMFRRegression.hlsl", nullptr, "main", "cs_5_1");
	PostprocessByteCode = Utility::CompileShader(L"Shader/BMFRPostprocess.hlsl", nullptr, "main", "cs_5_1");
	ReflectionTestVSByteCode = Utility::CompileShader(L"Shader/BMFRFinalReflection.hlsl", nullptr, "VS", "vs_5_1");
	ReflectionTestPSByteCode = Utility::CompileShader(L"Shader/BMFRFinalReflection.hlsl", nullptr, "PS", "ps_5_1");

	//Preprocess
	PreprocessRS.reset(new RootSignature(5));
	(*PreprocessRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5);
	(*PreprocessRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 3);
	(*PreprocessRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 1);
	(*PreprocessRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4);
	(*PreprocessRS)[4].InitAsConstants(0, 1);

	PreprocessRS->Finalize(L"Preprocess RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	PreprocessPSO.reset(new ComputePSO(L"PreprocessPSO"));
	PreprocessPSO->SetComputeShader(reinterpret_cast<BYTE*>(PreprocessByteCode->GetBufferPointer()), PreprocessByteCode->GetBufferSize());
	PreprocessPSO->SetRootSignature(*PreprocessRS);

	PreprocessPSO->Finalize(pCore->g_Device);

	//Regression
	RegressionRS.reset(new RootSignature(5));
	(*RegressionRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*RegressionRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 8);
	(*RegressionRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 8, 8);
	(*RegressionRS)[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 16, 1);
	(*RegressionRS)[4].InitAsConstants(0, 1);

	RegressionRS->Finalize(L"Regression RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	RegressionPSO.reset(new ComputePSO(L"RegressionPSO"));
	RegressionPSO->SetComputeShader(reinterpret_cast<BYTE*>(RegressionByteCode->GetBufferPointer()), RegressionByteCode->GetBufferSize());
	RegressionPSO->SetRootSignature(*RegressionRS);

	RegressionPSO->Finalize(pCore->g_Device);

	//Postprocess
	PostprocessRS.reset(new RootSignature(3));
	(*PostprocessRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3, D3D12_SHADER_VISIBILITY_ALL, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	(*PostprocessRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	(*PostprocessRS)[2].InitAsConstants(0, 1);

	PostprocessRS->Finalize(L"Postprocess RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	PostprocessPSO.reset(new ComputePSO(L"PostprocessPSO"));
	PostprocessPSO->SetComputeShader(reinterpret_cast<BYTE*>(PostprocessByteCode->GetBufferPointer()), PostprocessByteCode->GetBufferSize());
	PostprocessPSO->SetRootSignature(*PostprocessRS);

	PostprocessPSO->Finalize(pCore->g_Device);

	//ReflectionTest
	ReflectionTestRS.reset(new RootSignature(2));
	(*ReflectionTestRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*ReflectionTestRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);
	
	ReflectionTestRS->Finalize(L"ReflectionTest RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	ReflectionTestPSO.reset(new GraphicsPSO(L"ReflectionTestPSO"));
	ReflectionTestPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	ReflectionTestPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		DXGI_FORMAT_UNKNOWN,
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	ReflectionTestPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	ReflectionTestPSO->SetDepthStencilState(depthStencilDesc);
	ReflectionTestPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	ReflectionTestPSO->SetSampleMask(UINT_MAX);
	ReflectionTestPSO->SetVertexShader(reinterpret_cast<BYTE*>(ReflectionTestVSByteCode->GetBufferPointer()), ReflectionTestVSByteCode->GetBufferSize());
	ReflectionTestPSO->SetPixelShader(reinterpret_cast<BYTE*>(ReflectionTestPSByteCode->GetBufferPointer()), ReflectionTestPSByteCode->GetBufferSize());
	ReflectionTestPSO->SetRootSignature(*ReflectionTestRS);

	ReflectionTestPSO->Finalize(pCore->g_Device);
}

void BMFRPass::CreateTexture()
{
	//Texture
	D3D12_RESOURCE_DESC texDesc = pSwapChain->GetBackBufferDesc();
	threadX = texDesc.Width / 8 + 1;
	threadY = texDesc.Height / 8 + 1;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	prevNoisy.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"prevNoisy"));
	curNoisy.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"curNoisy"));
	texDesc.Format = DXGI_FORMAT_R32_UINT;
	acceptBools.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"acceptBools"));
	texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
	prevFramePixel.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"prevFramePixel"));
	
	texDesc.Width = 1024;
	texDesc.Height = 16384;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	for (size_t i = 0; i < 8; i++)
	{
		tmpData[i].reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, std::wstring(L"tmpData " + std::to_wstring(i)).c_str()));
		outData[i].reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, std::wstring(L"outData " + std::to_wstring(i)).c_str()));
	}

	//TextureViewer
	std::array<TextureViewerDesc, 4>preProcessTexUAVDescs;
	std::vector<Texture*> pRTs = { prevNoisy.get(),curNoisy.get(), acceptBools.get(),prevFramePixel.get() };
	for (size_t i = 0; i < 4; i++)
	{
		preProcessTexUAVDescs[i].TexType = TextureType::Texture2D;
		preProcessTexUAVDescs[i].ViewerType = TextureViewerType::UAV;
		preProcessTexUAVDescs[i].MostDetailedMip = 0;
		preProcessTexUAVDescs[i].NumMipLevels = 1;
	}
	preProcessTexUAV.reset(new TextureViewer(pCore, pRTs, preProcessTexUAVDescs.data(), true));

	std::array<TextureViewerDesc, 8>UAVDescs;
	pRTs.clear();
	for (size_t i = 0; i < 8; i++)
	{
		UAVDescs[i].TexType = TextureType::Texture2D;
		UAVDescs[i].ViewerType = TextureViewerType::UAV;
		UAVDescs[i].MostDetailedMip = 0;
		UAVDescs[i].NumMipLevels = 1;
		pRTs.push_back(tmpData[i].get());
	}
	tmpDataUAV.reset(new TextureViewer(pCore, pRTs, UAVDescs.data(), true));
	pRTs.clear();
	for (size_t i = 0; i < 8; i++)
	{
		pRTs.push_back(outData[i].get());
	}
	outDataUAV.reset(new TextureViewer(pCore, pRTs, UAVDescs.data(), true));

	TextureViewerDesc curNoisyUAVDesc;
	curNoisyUAVDesc.TexType = TextureType::Texture2D;
	curNoisyUAVDesc.ViewerType = TextureViewerType::UAV;
	curNoisyUAVDesc.MostDetailedMip = 0;
	curNoisyUAVDesc.NumMipLevels = 1;
	curNoisyUAV.reset(new TextureViewer(pCore, curNoisy.get(), curNoisyUAVDesc, true));
	TextureViewerDesc curNoisySRVDesc;
	curNoisySRVDesc.TexType = TextureType::Texture2D;
	curNoisySRVDesc.ViewerType = TextureViewerType::SRV;
	curNoisySRVDesc.MostDetailedMip = 0;
	curNoisySRVDesc.NumMipLevels = 1;
	curNoisySRV.reset(new TextureViewer(pCore, curNoisy.get(), curNoisySRVDesc, true));

	std::array<TextureViewerDesc, 3>postProcessTexSRVDescs;
	pRTs.clear();
	pRTs.push_back(prevNoisy.get());
	pRTs.push_back(acceptBools.get());
	pRTs.push_back(prevFramePixel.get());
	for (size_t i = 0; i < 3; i++)
	{
		postProcessTexSRVDescs[i].TexType = TextureType::Texture2D;
		postProcessTexSRVDescs[i].ViewerType = TextureViewerType::SRV;
		postProcessTexSRVDescs[i].MostDetailedMip = 0;
		postProcessTexSRVDescs[i].NumMipLevels = 1;
	}
	postProcessTexSRV.reset(new TextureViewer(pCore, pRTs, postProcessTexSRVDescs.data(), true));
	
	ClearTexture();
}

void BMFRPass::ClearTexture()
{
	//Clear UAV need CPU Descriptor
	std::unique_ptr<TextureViewer> uavCPU;
	std::vector<Texture*> pRTs = {
		prevNoisy.get(),
		curNoisy.get(),
		acceptBools.get(),
		prevFramePixel.get(),
		tmpData[0].get(),
		tmpData[1].get(),
		tmpData[2].get(),
		tmpData[3].get(),
		tmpData[4].get(),
		tmpData[5].get(),
		tmpData[6].get(),
		tmpData[7].get(),
		outData[0].get(),
		outData[1].get(),
		outData[2].get(),
		outData[3].get(),
		outData[4].get(),
		outData[5].get(),
		outData[6].get(),
		outData[7].get()
	};
	std::array<TextureViewerDesc, 20>texViewDescs;
	for (size_t i = 0; i < 20; i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::UAV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = 1;
	}
	uavCPU.reset(new TextureViewer(pCore, pRTs, texViewDescs.data(), false));

	std::array<float, 4>clearColorfloat4 = { 0.0f,0.0f,0.0f,0.0f };
	std::array<float, 2>clearColorfloat2 = { 0.0f,0.0f };
	std::array<float, 1>clearColorfloat = { 0.0f };
	std::array<UINT, 1>clearColorint = { 0 };
	//Clear UAV
	ComputeContext& Context = pCore->BeginComputeContext(L"Clear BMFR UAV");

	auto HeapInfo = preProcessTexUAV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.ClearUAV(preProcessTexUAV->GetGpuHandle(0), uavCPU->GetCpuHandle(0), prevNoisy.get(), clearColorfloat4.data());
	Context.ClearUAV(preProcessTexUAV->GetGpuHandle(1), uavCPU->GetCpuHandle(1), curNoisy.get(), clearColorfloat4.data());
	Context.ClearUAV(preProcessTexUAV->GetGpuHandle(2), uavCPU->GetCpuHandle(2), acceptBools.get(), clearColorint.data());
	Context.ClearUAV(preProcessTexUAV->GetGpuHandle(3), uavCPU->GetCpuHandle(3), prevFramePixel.get(), clearColorfloat2.data());
	for (size_t i = 0; i < 8; i++)
	{
		Context.ClearUAV(tmpDataUAV->GetGpuHandle(i), uavCPU->GetCpuHandle(4 + i), tmpData[i].get(), clearColorfloat.data());
		Context.ClearUAV(outDataUAV->GetGpuHandle(i), uavCPU->GetCpuHandle(12 + i), outData[i].get(), clearColorfloat.data());
	}

	Context.Finish(true);
}

void BMFRPass::Compute(ComputeContext& Context, TextureViewer* GbufferSRV, TextureViewer* PreNormalAndLinearZSRV, TextureViewer* RayTracingOutputSRV)
{
	//Preprocess
	Context.TransitionResource(prevNoisy.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(curNoisy.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(acceptBools.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(prevFramePixel.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
	for (size_t i = 0; i < 8; i++)
	{
		Context.TransitionResource(tmpData[i].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(outData[i].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	Context.FlushResourceBarriers();

	Context.SetPipelineState(*PreprocessPSO);
	Context.SetRootSignature(*PreprocessRS);
	auto HeapInfo = GbufferSRV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, PreNormalAndLinearZSRV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetDescriptorTable(2, RayTracingOutputSRV->GetGpuHandle(0));
	//RootParameter 3
	Context.SetDescriptorTable(3, preProcessTexUAV->GetGpuHandle(0));
	//RootParameter 4
	std::array<UINT, 1>FrameConstants = { frameCount++ };
	Context.SetConstantArray(4, 1, FrameConstants.data());
	Context.Dispatch(threadX, threadY, threadZ);
	
	//Regression
	Context.SetPipelineState(*RegressionPSO);
	Context.SetRootSignature(*RegressionRS);
	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, tmpDataUAV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetDescriptorTable(2, outDataUAV->GetGpuHandle(0));
	//RootParameter 3
	Context.SetDescriptorTable(3, curNoisyUAV->GetGpuHandle(0));
	//RootParameter 4
	Context.SetConstantArray(4, 1, FrameConstants.data());
	int width = pSwapChain->GetBackBufferDesc().Width;
	int height = pSwapChain->GetBackBufferDesc().Height;
	width = (width + 31) / 32 + 1; // 32 is block edge length
	height = (height + 31) / 32 + 1;
	Context.Dispatch(width * height, 1, 1);

	Context.TransitionResource(prevNoisy.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(acceptBools.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(prevFramePixel.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

	//Postprocess
	Context.SetPipelineState(*PostprocessPSO);
	Context.SetRootSignature(*PostprocessRS);
	//RootParameter 0
	Context.SetDescriptorTable(0, postProcessTexSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, curNoisyUAV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetConstantArray(2, 1, FrameConstants.data());
	Context.Dispatch(threadX, threadY, threadZ);

	Context.TransitionResource(prevNoisy.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(curNoisy.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.CopyResource(prevNoisy.get(), curNoisy.get());
	Context.TransitionResource(curNoisy.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	
	//ReflectionTest
	//Context.GetGraphicsContext().SetPipelineState(*ReflectionTestPSO);
	//Context.GetGraphicsContext().SetRootSignature(*ReflectionTestRS);
	//Context.GetGraphicsContext().SetRenderTargets(1, &pSwapChain->GetBackBufferView());
	//Context.GetGraphicsContext().SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//Context.GetGraphicsContext().SetDescriptorTable(1, curNoisySRV->GetGpuHandle(0));
	//Context.GetGraphicsContext().Draw(3);
}
