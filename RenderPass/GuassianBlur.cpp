#include "GuassianBlur.h"

GuassianBlur::GuassianBlur(RenderDevice* pCore, float sigma):
	pCore(pCore)
{
	CalcGaussWeights(sigma);

	Create();
}

void GuassianBlur::Create()
{
	//Shader
	ComPtr<ID3DBlob> HorzBlurByteCode = nullptr;
	ComPtr<ID3DBlob> VertBlurByteCode = nullptr;

	HorzBlurByteCode = Utility::CompileShader(L"Shader/GuassianBlur.hlsl", nullptr, "HorzBlurCS", "cs_5_1");
	VertBlurByteCode = Utility::CompileShader(L"Shader/GuassianBlur.hlsl", nullptr, "VertBlurCS", "cs_5_1");

	//RootSignature
	GuassianBlurRS.reset(new RootSignature(3));
	(*GuassianBlurRS)[0].InitAsConstantBuffer(0);
	(*GuassianBlurRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	(*GuassianBlurRS)[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);

	GuassianBlurRS->Finalize(L"GuassianBlur RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//HorzBlurPSO
	HorzBlurPSO.reset(new ComputePSO(L"HorzBlurPSO"));
	HorzBlurPSO->SetComputeShader(reinterpret_cast<BYTE*>(HorzBlurByteCode->GetBufferPointer()), HorzBlurByteCode->GetBufferSize());
	HorzBlurPSO->SetRootSignature(*GuassianBlurRS);

	HorzBlurPSO->Finalize(pCore->g_Device);
	//VertBlurPSO
	VertBlurPSO.reset(new ComputePSO(L"VertBlurPSO"));
	VertBlurPSO->SetComputeShader(reinterpret_cast<BYTE*>(VertBlurByteCode->GetBufferPointer()), VertBlurByteCode->GetBufferSize());
	VertBlurPSO->SetRootSignature(*GuassianBlurRS);

	VertBlurPSO->Finalize(pCore->g_Device);
}

void GuassianBlur::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is 
	int blurRadius = (int)ceil(2.0f * sigma);

	ASSERT(blurRadius <= maxBlurRadius);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	constants.BlurRadius = (int)weights.size() / 2;
	constants.w0 = weights[0]; constants.w1 = weights[1]; constants.w2 = weights[2];
	constants.w3 = weights[3]; constants.w4 = weights[4]; constants.w5 = weights[5];
	constants.w6 = weights[6]; constants.w7 = weights[7]; constants.w8 = weights[8];
	constants.w9 = weights[9]; constants.w10 = weights[10];

	CB.reset(new Buffer(pCore, &constants, sizeof(GuassianBlurConstants), false, true, L"GuassianBlurConstants"));
}

void GuassianBlur::Compute(ComputeContext& Context, Texture* tex, TextureViewer* texSRV)
{
	//Create BlurOutput
	D3D12_RESOURCE_DESC texDesc = tex->GetDesc();
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	blurPing.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"BlurPing"));
	blurPong.reset(new Texture(pCore, texDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"BlurPhong"));

	TextureViewerDesc texViewDesc;
	texViewDesc.TexType = TextureType::Texture2D;
	texViewDesc.ViewerType = TextureViewerType::UAV;
	texViewDesc.MostDetailedMip = 0;

	blurPingUAV.reset(new TextureViewer(pCore, blurPing.get(), texViewDesc, true));
	blurPongUAV.reset(new TextureViewer(pCore, blurPong.get(), texViewDesc, true));

	texViewDesc.ViewerType = TextureViewerType::SRV;
	texViewDesc.NumMipLevels = tex->GetDesc().MipLevels;

	blurPingSRV.reset(new TextureViewer(pCore, blurPing.get(), texViewDesc, true));
	blurPongSRV.reset(new TextureViewer(pCore, blurPong.get(), texViewDesc, true));

	Context.SetPipelineState(*HorzBlurPSO);
	Context.SetRootSignature(*GuassianBlurRS);

	Context.SetConstantBuffer(0, CB->GetGpuVirtualAddress());
	Context.SetDescriptorTable(1, texSRV->GetGpuHandle());
	Context.SetDescriptorTable(2, blurPingUAV->GetGpuHandle());

	UINT numGroupsX = (UINT)ceilf(blurPing->GetDesc().Width / 256.0f);
	UINT numGroupsY = blurPing->GetDesc().Height;
	Context.Dispatch(numGroupsX, numGroupsY, 1);

	Context.TransitionResource(blurPing.get(), D3D12_RESOURCE_STATE_GENERIC_READ, true);

	Context.SetPipelineState(*VertBlurPSO);

	Context.SetConstantBuffer(0, CB->GetGpuVirtualAddress());
	Context.SetDescriptorTable(1, blurPingSRV->GetGpuHandle());
	Context.SetDescriptorTable(2, blurPongUAV->GetGpuHandle());

	numGroupsX = blurPing->GetDesc().Width;
	numGroupsY = (UINT)ceilf(blurPing->GetDesc().Height / 256.0f);
	Context.Dispatch(numGroupsX, numGroupsY, 1);

	Context.TransitionResource(tex, D3D12_RESOURCE_STATE_COPY_DEST);
	Context.TransitionResource(blurPong.get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
	Context.CopyResource(tex, blurPong.get());
	Context.TransitionResource(tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}


