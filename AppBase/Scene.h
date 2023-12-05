#pragma once
#include<assimp/Importer.hpp>
#include<assimp/scene.h>
#include<assimp/postprocess.h>
#include<assimp/GltfMaterial.h>

#include"TextureLoader.h"
#include"Buffer.h"
#include"Material.h"

enum class LayerType
{
	Opaque,
	Blend,
	Mask,
	LayerNum
};

enum class DebugViewType
{
	None,
	BaseColor,
	Transparency,
	NormalMap,
	Occlusion,
	Emissive,
	Metallic,
	Roughness,
	DiffuseColor,
	F0,
	F90,
	MeshNormal,
	PerturbedNormal,
	NdotV,
	DiffuseIBL,
	SpecularIBL,
	NumDebugViews
};

struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 TexCoords;
};

enum class LightType
{
	Directional,
	Point,
	Spot
};

struct Light
{
	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;                        
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
	float FalloffEnd = 10.0f;                           
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  
	LightType type;
};

struct MeshConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 PreWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldInvertTran = MathHelper::Identity4x4();
	int MeshID;
};

struct PassConstants
{
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProjInvert = MathHelper::Identity4x4();
	XMFLOAT4X4 PreViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT3 CameraPos = { 0.0f,0.0f,0.0f };
	float NearZ;
	float FarZ;
	XMINT2 FrameDimension;
	int PrefilteredEnvMipLevels;
	XMINT3 FirstLightIndex = { 0,0,0 };
};

class Mesh
{
public:
	std::vector<Vertex>m_Vertices;
	std::vector<std::uint32_t>m_Indices;
	size_t m_MatID;
	std::unique_ptr<Buffer> m_VertexBuffer;
	std::unique_ptr<Buffer> m_IndexBuffer;
	std::unique_ptr<Buffer> m_ConstantsBuffer;
	MeshConstants m_Constants;
	XMFLOAT4X4 m_WorldMatrix;
	std::string m_AlphaMode;

	Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices, size_t MatID, XMFLOAT4X4 transformation, std::string alphaMode, RenderDevice* Device);

	D3D12_GPU_VIRTUAL_ADDRESS GetVertexBufferAddress() { return m_VertexBuffer->GetGpuVirtualAddress(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetIndexBufferAddress() { return m_IndexBuffer->GetGpuVirtualAddress(); }

	XMFLOAT4X4 GetWorldMatrix() { return m_WorldMatrix; }

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView()
	{
		return {
			GetVertexBufferAddress(),
			(UINT)(m_Vertices.size() * sizeof(Vertex)),
			sizeof(Vertex)
		};
	}

	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView()
	{
		return {
			GetIndexBufferAddress(),
			(UINT)(m_Indices.size() * sizeof(std::uint32_t)),
			DXGI_FORMAT_R32_UINT
		};
	}

	void UploadConstantsBuffer(RenderDevice* renderCore, MeshConstants* data)
	{
		m_ConstantsBuffer.reset(new Buffer(renderCore, nullptr, sizeof(MeshConstants), true, true));
		m_ConstantsBuffer->Upload(data);
	}

private:
	void SetupMesh(RenderDevice* Device);
};

class Scene
{
public:
	Scene(std::string path, RenderDevice* Device)
	{
		this->m_pDevice = Device;
		m_Materials.push_back(std::move(BasePBRMat::DefaulMat(Device)));
		LoadScene(path);
	}
	static XMFLOAT4X4 ConvertMatrix(aiMatrix4x4 matrix)
	{
		
		XMFLOAT4X4 res(
			(float)matrix.a1, (float)matrix.b1, (float)matrix.c1, (float)matrix.d1,
			(float)matrix.a2, (float)matrix.b2, (float)matrix.c2, (float)matrix.d2,
			(float)matrix.a3, (float)matrix.b3, (float)matrix.c3, (float)matrix.d3,
			(float)matrix.a4, (float)matrix.b4, (float)matrix.c4, (float)matrix.d4
		);
		return res;
	}
	void AddDirectionalLight(XMFLOAT3 Strength, XMFLOAT3 Direction)
	{
		Light light;
		light.Strength = Strength;
		light.Direction = Direction;
		light.type = LightType::Directional;
		m_Lights.push_back(light);
	}
	void AddPointLight(XMFLOAT3 Strength, XMFLOAT3 Position, float FalloffStart = 1.0f, float FalloffEnd = 10.0f)
	{
		Light light;
		light.Strength = Strength;
		light.Position = Position;
		light.FalloffStart = FalloffStart;
		light.FalloffEnd = FalloffEnd;
		light.type = LightType::Point;
		m_Lights.push_back(light);
	}

	std::array< std::vector<Mesh>, static_cast<size_t>(LayerType::LayerNum)>m_Meshes;
	std::vector<std::unique_ptr<Texture>>m_Textures;
	std::vector<BasePBRMat>m_Materials;
	std::vector<Light>m_Lights;
	std::unique_ptr<Buffer>m_LightBuffers;
private:
	RenderDevice* m_pDevice;
	std::string m_Directory;
	void LoadScene(std::string path);
	void ProcessNode(aiNode* node, const aiScene* scene, XMFLOAT4X4 accTransform);
	void ProcessMesh(aiMesh* mesh, const aiScene* scene, XMFLOAT4X4 transformation);
	void LoadMaterial(aiMaterial* mat, bool HasUV);
};