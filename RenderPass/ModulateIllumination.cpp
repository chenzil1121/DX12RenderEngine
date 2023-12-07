#include "ModulateIllumination.h"

ModulateIllumination::ModulateIllumination(RenderDevice* pCore, SwapChain* swapChain):
	pCore(pCore),
	pSwapChain(swapChain)
{
	Create();
}

void ModulateIllumination::Create()
{
	//Shader
	ComPtr<ID3DBlob> ModulateIlluminationVSByteCode = nullptr;
	ComPtr<ID3DBlob> ModulateIlluminationPSByteCode = nullptr;

	ModulateIlluminationVSByteCode = Utility::CompileShader(L"../RenderPass/Shader/ModulateIllumination.hlsl", nullptr, "VS", "vs_5_1");
	ModulateIlluminationPSByteCode = Utility::CompileShader(L"../RenderPass/Shader/ModulateIllumination.hlsl", nullptr, "PS", "ps_5_1");

	//RootSignature
	modulateIlluminationRS.reset(new RootSignature(5));
	(*modulateIlluminationRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
	(*modulateIlluminationRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1);
	(*modulateIlluminationRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 1);
	(*modulateIlluminationRS)[3].InitAsConstantBuffer(0);
	(*modulateIlluminationRS)[4].InitAsConstantBuffer(1);

	modulateIlluminationRS->Finalize(L"ModulateIllumination RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//PSO
	modulateIlluminationPSO.reset(new GraphicsPSO(L"ModulateIlluminationPSO"));
	modulateIlluminationPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	modulateIlluminationPSO->SetRenderTargetFormat(
		pSwapChain->GetBackBufferSRGBFormat(),
		DXGI_FORMAT_UNKNOWN,
		pSwapChain->GetMSAACount(),
		pSwapChain->GetMSAAQuality()
	);
	modulateIlluminationPSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	modulateIlluminationPSO->SetDepthStencilState(depthStencilDesc);
	modulateIlluminationPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	modulateIlluminationPSO->SetSampleMask(UINT_MAX);
	modulateIlluminationPSO->SetVertexShader(reinterpret_cast<BYTE*>(ModulateIlluminationVSByteCode->GetBufferPointer()), ModulateIlluminationVSByteCode->GetBufferSize());
	modulateIlluminationPSO->SetPixelShader(reinterpret_cast<BYTE*>(ModulateIlluminationPSByteCode->GetBufferPointer()), ModulateIlluminationPSByteCode->GetBufferSize());
	modulateIlluminationPSO->SetRootSignature(*modulateIlluminationRS);

	modulateIlluminationPSO->Finalize(pCore->g_Device);
}

void ModulateIllumination::Render(GraphicsContext& Context, Buffer* PassConstantBuffer, Light pointLight, TextureViewer* GbufferSRV, TextureViewer* FilteredShadowSRV, TextureViewer* FilteredReflectionSRV)
{
	pointLightConstantBuffer.reset(new Buffer(pCore, nullptr, sizeof(Light), true, true, L"ModulateIllumination PointLight CB"));
	pointLightConstantBuffer->Upload(&pointLight);

	Context.SetPipelineState(*modulateIlluminationPSO);
	Context.SetRootSignature(*modulateIlluminationRS);
	Context.SetRenderTargets(1, &pSwapChain->GetBackBufferView());
	//RootParameter 0
	Context.SetDescriptorTable(0, GbufferSRV->GetGpuHandle(0));
	//RootParameter 1
	Context.SetDescriptorTable(1, FilteredShadowSRV->GetGpuHandle(0));
	//RootParameter 2
	Context.SetDescriptorTable(2, FilteredReflectionSRV->GetGpuHandle(0));
	//RootParameter 3
	Context.SetConstantBuffer(3, PassConstantBuffer->GetGpuVirtualAddress());
	//RootParameter 4
	Context.SetConstantBuffer(4, pointLightConstantBuffer->GetGpuVirtualAddress());

	Context.Draw(3);
}
