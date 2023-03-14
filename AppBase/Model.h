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
	XMFLOAT4X4 WorldInvertTran = MathHelper::Identity4x4();
};

struct PassConstants
{
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProjInvert = MathHelper::Identity4x4();
	XMFLOAT3 CameraPos = { 0.0f,0.0f,0.0f };
	float pad0 = 0.0f;
	XMINT3 FirstLightIndex = { 0,0,0 };
	int PrefilteredEnvMipLevels;
	DebugViewType DebugView;
};

class Mesh
{
public:
	std::vector<Vertex>m_Vertices;
	std::vector<std::uint16_t>m_Indices;
	size_t m_MatID;
	std::unique_ptr<Buffer> m_VertexBuffer;
	std::unique_ptr<Buffer> m_IndexBuffer;
	std::unique_ptr<Buffer> m_ConstantsBuffer;
	MeshConstants m_Constants;
	std::string m_AlphaMode;

	Mesh(std::vector<Vertex> vertices, std::vector<std::uint16_t> indices, size_t MatID, XMFLOAT4X4 transformation, std::string alphaMode, RenderDevice* Device);
private:
	void SetupMesh(RenderDevice* Device);
};

class Model
{
public:
	Model(std::string path, RenderDevice* Device)
	{
		this->m_pDevice = Device;
		m_Materials.push_back(std::move(BasePBRMat::DefaulMat(Device)));
		LoadModel(path);
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
	void LoadModel(std::string path);
	void ProcessNode(aiNode* node, const aiScene* scene, XMFLOAT4X4 accTransform);
	void ProcessMesh(aiMesh* mesh, const aiScene* scene, XMFLOAT4X4 transformation);
	void LoadMaterial(aiMaterial* mat, bool HasUV);
};