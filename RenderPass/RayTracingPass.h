#pragma once
#include "RenderDevice.h"
#include "SwapChain.h"
#include "RayTracingPipelineState.h"
#include "RayTracingShaderTable.h"
#include "AccelerationStructure.h"
#include "BufferView.h"
#include "Scene.h"
#include "GbufferPass.h"
#include "TextureViewer.h"

struct InstanceInfo
{
	int VertexBufferID;
	int IndexBufferID;
	int BaseColorTexID;
	int RoughnessMetallicTexID;
	int NormalTexID;
	int pad0;
	int pad1;
	int pad2;
	PBRParameter parameter;
};

struct RayTracingConstants
{
	XMFLOAT4 CameraPosition;
	uint32_t FrameCount;
	uint32_t VertexSRVOffsetInHeap;
	uint32_t IndexSRVOffsetInHeap;
	uint32_t PBRTexSRVOffsetInHeap;
	Light PointLight;
};

class RayTracingPass
{
public:
	RayTracingPass(RenderDevice* pCore, SwapChain* pSwapChain, Scene* scene);

	void Create(Scene* scene);

	void CreateBindless(Scene* scene);

	void CreateShadowOutput()
	{
		D3D12_RESOURCE_DESC uavDesc = pSwapChain->GetBackBufferDesc();
		uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		shadowOutput.reset(new Texture(pCore, uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ShadowOutput"));

		TextureViewerDesc uavViewdesc;
		uavViewdesc.TexType = TextureType::Texture2D;
		uavViewdesc.ViewerType = TextureViewerType::UAV;
		uavViewdesc.MostDetailedMip = 0;
		TextureViewerDesc srvViewdesc;
		srvViewdesc.TexType = TextureType::Texture2D;
		srvViewdesc.ViewerType = TextureViewerType::SRV;
		srvViewdesc.MostDetailedMip = 0;
		srvViewdesc.NumMipLevels = 1;
		shadowOutputUAV.reset(new TextureViewer(pCore, shadowOutput.get(), uavViewdesc, true));
		shadowOutputSRV.reset(new TextureViewer(pCore, shadowOutput.get(), srvViewdesc, true));
	}

	void CreateReflectOutput()
	{
		D3D12_RESOURCE_DESC uavDesc = pSwapChain->GetBackBufferDesc();
		uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		reflectOutput.reset(new Texture(pCore, uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ReflectOutput"));

		TextureViewerDesc uavViewdesc;
		uavViewdesc.TexType = TextureType::Texture2D;
		uavViewdesc.ViewerType = TextureViewerType::UAV;
		uavViewdesc.MostDetailedMip = 0;
		TextureViewerDesc srvViewdesc;
		srvViewdesc.TexType = TextureType::Texture2D;
		srvViewdesc.ViewerType = TextureViewerType::SRV;
		srvViewdesc.MostDetailedMip = 0;
		srvViewdesc.NumMipLevels = 1;
		reflectOutputUAV.reset(new TextureViewer(pCore, reflectOutput.get(), uavViewdesc, true));
		reflectOutputSRV.reset(new TextureViewer(pCore, reflectOutput.get(), srvViewdesc, true));
	}

	void UpdateTASMatrixs(std::vector<XMFLOAT4X4>& matrixs);

	void Resize(UINT width, UINT height, float aspectRatio)
	{
		CreateShadowOutput();
		CreateReflectOutput();
	}

	TextureViewer* GetShadowOutputSRV()
	{
		return shadowOutputSRV.get();
	}

	TextureViewer* GetReflectOutputSRV()
	{
		return reflectOutputSRV.get();
	}

	void Render(RayTracingContext& Context, XMFLOAT3 cameraPos, Light pointLight, TextureViewer* skyViewer, TextureViewer* GbufferSRV);

private:
	RenderDevice* pCore;
	SwapChain* pSwapChain;
	uint32_t frameCount = 0;
	std::unique_ptr<RayTracingPSO> rayTracingPSO;
	std::unique_ptr<RootSignature> globalRS;
	std::unique_ptr<ShaderTable> rayGenShaderTable;
	std::unique_ptr<ShaderTable> missShaderTable;
	std::unique_ptr<ShaderTable> hitGroupShaderTable;

	std::unique_ptr<Buffer> rayTracingConstantBuffer;
	std::unique_ptr<Buffer> instanceInfosBuffer;
	std::unique_ptr<BufferViewer> bindlessVertexSRV;
	std::unique_ptr<BufferViewer> bindlessIndexSRV;
	std::unique_ptr<TextureViewer> bindlessPBRTexSRV;

	std::unique_ptr<Texture>shadowOutput;
	std::unique_ptr<TextureViewer>shadowOutputUAV;
	std::unique_ptr<TextureViewer>shadowOutputSRV;

	std::unique_ptr<Texture>reflectOutput;
	std::unique_ptr<TextureViewer>reflectOutputUAV;
	std::unique_ptr<TextureViewer>reflectOutputSRV;

	std::unique_ptr<AccelerationStructure>accelerationStructure;
};