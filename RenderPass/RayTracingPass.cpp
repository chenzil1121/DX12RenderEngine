#include "RayTracingPass.h"

RayTracingPass::RayTracingPass(RenderDevice* pCore, SwapChain* pSwapChain, Scene* scene):
	pCore(pCore),
	pSwapChain(pSwapChain)
{
	Create(scene);
	Resize(pCore->g_DisplayWidth, pCore->g_DisplayHeight, (FLOAT)pCore->g_DisplayWidth / (FLOAT)pCore->g_DisplayHeight);
}

void RayTracingPass::Create(Scene* scene)
{
	CreateBindless(scene);

	//Shader
	ComPtr<IDxcBlob> rayTacingShader = Utility::CompileLibrary(L"../RenderPass/Shader/Raytracing.hlsl", L"lib_6_3");

	//Global RootSignature
	globalRS.reset(new RootSignature(8, 2));
	globalRS->InitStaticSampler(0, Sampler::anisotropicClamp);
	globalRS->InitStaticSampler(1, Sampler::LinearWrap);
	(*globalRS)[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	(*globalRS)[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
	(*globalRS)[2].InitAsBufferSRV(0);
	(*globalRS)[3].InitAsBufferSRV(1);
	(*globalRS)[4].InitAsConstantBuffer(0);
	(*globalRS)[5].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL, 1);
	(*globalRS)[6].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3, D3D12_SHADER_VISIBILITY_ALL, 3);
	//Bindless
	(*globalRS)[7].InitAsDescriptorTable(3);
	(*globalRS)[7].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, UINT_MAX, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);
	(*globalRS)[7].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, UINT_MAX, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);
	(*globalRS)[7].SetTableRange(2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, UINT_MAX, 2, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE, 0);
	
	globalRS->Finalize(L"Raytracing Global RootSignature", D3D12_ROOT_SIGNATURE_FLAG_NONE, pCore->g_Device);

	//RayTracingPSO
	rayTracingPSO.reset(new RayTracingPSO());
	std::vector<const WCHAR*>entryPoints = { L"MyRaygenShader" ,L"MyShadowClosestHitShader" ,L"MyShadowMissShader" ,L"MySecondaryClosestHitShader" ,L"MySecondaryMissShader" };
	rayTracingPSO->SetShader(rayTacingShader->GetBufferPointer(), rayTacingShader->GetBufferSize(), entryPoints);
	rayTracingPSO->SetHitGroup(L"MyShadowHitGroup", L"", L"MyShadowClosestHitShader");
	rayTracingPSO->SetHitGroup(L"MySecondaryHitGroup", L"", L"MySecondaryClosestHitShader");
	rayTracingPSO->SetShaderConfig(4 * sizeof(float), 2 * sizeof(float));
	rayTracingPSO->SetPipelineConfig(2);
	rayTracingPSO->SetGlobalRootSignature(globalRS->GetSignature());

	rayTracingPSO->Finalize(pCore->g_Device);

	//Build AccelerationStructure
	std::vector<std::pair<D3D12_RAYTRACING_GEOMETRY_DESC, XMFLOAT4X4>> geomDescs;
	for (size_t i = 0; i < scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)].size(); i++)
	{
		Mesh& mesh = scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)][i];

		std::pair<D3D12_RAYTRACING_GEOMETRY_DESC, XMFLOAT4X4> geomDesc;
		geomDesc.first.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geomDesc.first.Triangles.VertexBuffer.StartAddress = mesh.GetVertexBufferAddress();
		geomDesc.first.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
		geomDesc.first.Triangles.VertexCount = mesh.m_Vertices.size();
		geomDesc.first.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geomDesc.first.Triangles.IndexBuffer = mesh.GetIndexBufferAddress();
		geomDesc.first.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		geomDesc.first.Triangles.IndexCount = mesh.m_Indices.size();
		geomDesc.first.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geomDesc.second = mesh.m_Constants.World;

		geomDescs.push_back(geomDesc);
	}
	accelerationStructure.reset(new AccelerationStructure(pCore, geomDescs, 0));

	//ShaderTable
	void* rayGenShaderIdentifier;
	void* shadowMissShaderIdentifier;
	void* shadowHitGroupShaderIdentifier;
	void* secondaryMissShaderIdentifier;
	void* secondaryHitGroupShaderIdentifier;

	rayGenShaderIdentifier = GetShaderIdentifier(L"MyRaygenShader", *rayTracingPSO);
	shadowMissShaderIdentifier = GetShaderIdentifier(L"MyShadowMissShader", *rayTracingPSO);
	shadowHitGroupShaderIdentifier = GetShaderIdentifier(L"MyShadowHitGroup", *rayTracingPSO);
	secondaryMissShaderIdentifier = GetShaderIdentifier(L"MySecondaryMissShader", *rayTracingPSO);
	secondaryHitGroupShaderIdentifier = GetShaderIdentifier(L"MySecondaryHitGroup", *rayTracingPSO);

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		rayGenShaderTable.reset(new ShaderTable(numShaderRecords, shaderRecordSize, L"RayGenShaderTable"));
		rayGenShaderTable->push_back(ShaderRecord(rayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		rayGenShaderTable->Finalize(pCore);
	}
	//Miss shader table
	{
		UINT numShaderRecords = 2;
		UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		missShaderTable.reset(new ShaderTable(numShaderRecords, shaderRecordSize, L"ShadowMissShaderTable"));
		missShaderTable->push_back(ShaderRecord(shadowMissShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		missShaderTable->push_back(ShaderRecord(secondaryMissShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		missShaderTable->Finalize(pCore);
	}
	// Hit group shader table
	{
		UINT numShaderRecords = 2;
		UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		hitGroupShaderTable.reset(new ShaderTable(numShaderRecords, shaderRecordSize, L"ShadowHitGroupShaderTable"));
		hitGroupShaderTable->push_back(ShaderRecord(shadowHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		hitGroupShaderTable->push_back(ShaderRecord(secondaryHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		hitGroupShaderTable->Finalize(pCore);
	}
}

void RayTracingPass::CreateBindless(Scene* scene)
{
	std::vector<Buffer*> vertexsBuffers;
	std::vector<Buffer*> indexsBuffers;
	std::vector<BufferViewerDesc> vertexBufViewDescs;
	std::vector<BufferViewerDesc> indexBufViewDescs;

	std::vector<InstanceInfo> instanceInfos;
	int instanceId = 0;

	for (size_t i = 0; i < scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)].size(); i++)
	{
		Mesh& mesh = scene->m_Meshes[static_cast<size_t>(LayerType::Opaque)][i];
		
		vertexsBuffers.push_back(mesh.m_VertexBuffer.get());
		indexsBuffers.push_back(mesh.m_IndexBuffer.get());

		BufferViewerDesc vertexBufViewDesc;
		vertexBufViewDesc.ViewerType = BufferViewerType::SRV;
		vertexBufViewDesc.NumElements = mesh.m_Vertices.size();
		vertexBufViewDesc.ElementSize = sizeof(Vertex);
		vertexBufViewDescs.push_back(vertexBufViewDesc);
		BufferViewerDesc indexBufViewDesc;
		indexBufViewDesc.ViewerType = BufferViewerType::SRV;
		indexBufViewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		indexBufViewDesc.NumElements = mesh.m_Indices.size();
		indexBufViewDescs.push_back(indexBufViewDesc);

		BasePBRMat& mat = scene->m_Materials[mesh.m_MatID];

		InstanceInfo info;
		info.VertexBufferID = instanceId;
		info.IndexBufferID = instanceId;
		info.BaseColorTexID = mat.m_IDs[0];
		info.RoughnessMetallicTexID = mat.m_IDs[1];
		info.NormalTexID = mat.m_IDs[2];
		info.parameter = mat.GetParameter();
		instanceInfos.push_back(info);
		instanceId++;
	}

	bindlessVertexSRV.reset(new BufferViewer(pCore, vertexsBuffers, vertexBufViewDescs.data(), true));
	bindlessIndexSRV.reset(new BufferViewer(pCore, indexsBuffers, indexBufViewDescs.data(), true));
	instanceInfosBuffer.reset(new Buffer(pCore, instanceInfos.data(), (UINT(sizeof(InstanceInfo) * instanceInfos.size())), false, false, L"InstanceInfos"));
	
	std::vector<Texture*>pTex;
	std::vector<TextureViewerDesc> texViewDescs;
	for (size_t i = 0; i < scene->m_Textures.size(); i++)
	{
		TextureViewerDesc texViewDesc;
		texViewDesc.TexType = TextureType::Texture2D;
		texViewDesc.ViewerType = TextureViewerType::SRV;
		texViewDesc.MostDetailedMip = 0;
		texViewDesc.NumMipLevels = scene->m_Textures[i]->GetDesc().MipLevels;
		texViewDescs.push_back(texViewDesc);
		pTex.push_back(scene->m_Textures[i].get());
	}
	bindlessPBRTexSRV.reset(new TextureViewer(pCore, pTex, texViewDescs.data(), true));
}

void RayTracingPass::UpdateTASMatrixs(std::vector<XMFLOAT4X4>& matrixs)
{
	accelerationStructure->UpdateMatrix(matrixs);
}

void RayTracingPass::DispatchRays(RayTracingContext& Context, XMFLOAT3 cameraPos, Light pointLight, TextureViewer* skyViewer, TextureViewer* GbufferSRV)
{
	RayTracingConstants rtConstant;
	rtConstant.CameraPosition = { cameraPos.x ,cameraPos.y ,cameraPos.z ,1.0f };
	rtConstant.FrameCount = frameCount++;
	rtConstant.VertexSRVOffsetInHeap = bindlessVertexSRV->GetFirstGpuHandleOffsetInHeap();
	rtConstant.IndexSRVOffsetInHeap = bindlessIndexSRV->GetFirstGpuHandleOffsetInHeap();
	rtConstant.PBRTexSRVOffsetInHeap = bindlessPBRTexSRV->GetFirstGpuHandleOffsetInHeap();
	rtConstant.PointLight = pointLight;

	rayTracingConstantBuffer.reset(new Buffer(pCore, nullptr, sizeof(RayTracingConstants), true, true));
	rayTracingConstantBuffer->Upload(&rtConstant);

	Context.SetRayTracingGlobalRootSignature(*globalRS);

	Context.TransitionResource(reflectOutput.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(shadowOutput.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	//RootParameter 0
	auto HeapInfo = shadowOutputUAV->GetHeapInfo();
	Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
	Context.SetRayTracingRootDescriptorTable(0, shadowOutputUAV->GetGpuHandle(0));

	//RootParameter 1
	Context.SetRayTracingRootDescriptorTable(1, reflectOutputUAV->GetGpuHandle(0));

	//RootParameter 2
	Context.SetRayTracingRootShaderResourceView(2, accelerationStructure->GetTLASGPUVirtualAddress());

	//RootParameter 3
	Context.SetRayTracingRootShaderResourceView(3, instanceInfosBuffer->GetGpuVirtualAddress());

	//RootParameter 4
	Context.SetRayTracingRootConstantBufferView(4, rayTracingConstantBuffer->GetGpuVirtualAddress());

	//RootParameter 5
	Context.SetRayTracingRootDescriptorTable(5, skyViewer->GetGpuHandle());

	//RootParameter 6
	Context.SetRayTracingRootDescriptorTable(6, GbufferSRV->GetGpuHandle());
	
	//RootParameter 7
	Context.SetRayTracingRootDescriptorTable(7, HeapInfo.second->GetGPUDescriptorHandleForHeapStart());

	Context.SetRayTracingPipelineState(*rayTracingPSO);
	Context.DispatchRays(
		rayGenShaderTable.get(),
		missShaderTable.get(),
		hitGroupShaderTable.get(),
		pCore->g_DisplayWidth,
		pCore->g_DisplayHeight,
		1
	);

	Context.TransitionResource(reflectOutput.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(shadowOutput.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}
