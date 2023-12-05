#include"Scene.h"

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices, size_t MatID, XMFLOAT4X4 transformation, std::string alphaMode, RenderDevice* Device) :
	m_Vertices(vertices),
	m_Indices(indices),
	m_MatID(MatID), 
	m_AlphaMode(alphaMode)
{
	m_WorldMatrix = transformation;
	XMStoreFloat4x4(&m_Constants.WorldInvertTran, MathHelper::InverseTranspose(XMLoadFloat4x4(&m_Constants.World)));
	SetupMesh(Device);
}

void Mesh::SetupMesh(RenderDevice* Device)
{
	m_VertexBuffer.reset(new Buffer(Device, m_Vertices.data(), (UINT)(m_Vertices.size() * sizeof(Vertex)), false, false));
	m_IndexBuffer.reset(new Buffer(Device, m_Indices.data(), (UINT)(m_Indices.size() * sizeof(std::uint32_t)), false, false));
}

void Scene::LoadScene(std::string path)
{
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_ConvertToLeftHanded | aiProcess_Triangulate);
	ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode, import.GetErrorString());
	Utility::Printf("Load %s succeed!\n%d Meshes %d Materials %d Animations %d Textures %d Lights %d Cameras\n", path.c_str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures, scene->mNumLights, scene->mNumCameras);

	m_Directory = path.substr(0, path.find_last_of('/'));

	ProcessNode(scene->mRootNode, scene, MathHelper::Identity4x4());
}

void Scene::ProcessNode(aiNode* node, const aiScene* scene, XMFLOAT4X4 accTransform)
{
	XMStoreFloat4x4(&accTransform, XMLoadFloat4x4(&ConvertMatrix(node->mTransformation)) * XMLoadFloat4x4(&accTransform));
	
	for (size_t i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, scene, accTransform);
	}
	for (size_t i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene, accTransform);
	}
}

void Scene::ProcessMesh(aiMesh* mesh, const aiScene* scene, XMFLOAT4X4 transformation)
{
	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	bool HasUV = FALSE;

	for (size_t i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;

		vertex.Position.x = mesh->mVertices[i].x;
		vertex.Position.y = mesh->mVertices[i].y;
		vertex.Position.z = mesh->mVertices[i].z;

		vertex.Normal.x = mesh->mNormals[i].x;
		vertex.Normal.y = mesh->mNormals[i].y;
		vertex.Normal.z = mesh->mNormals[i].z;

		if (mesh->mTextureCoords[0])
		{
			vertex.TexCoords.x = mesh->mTextureCoords[0][i].x;
			vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
			HasUV = TRUE;
		}
		else
			vertex.TexCoords = XMFLOAT2(0.0f, 0.0f);
		vertices.push_back(vertex);
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	aiString aiAlphaMode("OPAQUE");
	size_t matID = 0;
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		std::string matName = material->GetName().C_Str();

		material->Get(AI_MATKEY_GLTF_ALPHAMODE, aiAlphaMode);
		
		bool skip = false;
		for (size_t i = 0; i < m_Materials.size(); i++)
		{
			if (matName == m_Materials[i].GetName())
			{
				skip = true;
				matID = i;
				break;
			}
		}
		if (!skip)
		{
			LoadMaterial(material, HasUV);
			matID = m_Materials.size() - 1;
		}
	}

	if (aiAlphaMode == aiString("OPAQUE"))
		m_Meshes[static_cast<size_t>(LayerType::Opaque)].emplace_back(Mesh(vertices, indices, matID, transformation, aiAlphaMode.C_Str(), m_pDevice));
	else if(aiAlphaMode == aiString("BLEND"))
		m_Meshes[static_cast<size_t>(LayerType::Blend)].emplace_back(Mesh(vertices, indices, matID, transformation, aiAlphaMode.C_Str(), m_pDevice));
	
}

void Scene::LoadMaterial(aiMaterial* mat, bool HasUV)
{
	PBRParameter pbrParameter;
	pbrParameter.HasUV = HasUV;
	aiColor4D baseColor;
	ai_real roughnessFactor;
	ai_real metallicFactor;
	if (AI_SUCCESS == mat->Get(AI_MATKEY_BASE_COLOR, baseColor))
	{
		pbrParameter.BaseColorFactor.x = baseColor.r;
		pbrParameter.BaseColorFactor.y = baseColor.g;
		pbrParameter.BaseColorFactor.z = baseColor.b;
		pbrParameter.BaseColorFactor.w = baseColor.a;
	}
	else
		pbrParameter.BaseColorFactor = { 1.0f,1.0f,1.0f,1.0f };

	if (AI_SUCCESS != mat->Get(AI_MATKEY_METALLIC_FACTOR, pbrParameter.RoughnessFactor))
		pbrParameter.RoughnessFactor = 1.0f;
	if (AI_SUCCESS != mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, pbrParameter.MetallicFactor))
		pbrParameter.MetallicFactor=1.0f;

	std::array<aiTextureType, 3> types;
	std::vector<Texture*>pTexs;
	types[0] = aiTextureType_BASE_COLOR;
	types[1] = aiTextureType_METALNESS;
	types[2] = aiTextureType_NORMALS;
	for (size_t k = 0; k < types.size(); k++)
	{
		if (mat->GetTextureCount(types[k]) == 0)
		{
			switch (types[k])
			{
			case aiTextureType_BASE_COLOR:
				pTexs.push_back(BasePBRMat::DefaultBaseColorTex.get());
				break;
			case aiTextureType_METALNESS:
				pTexs.push_back(BasePBRMat::DefaultRoughnessMetallicTex.get());
				break;
			case aiTextureType_NORMALS:
				pTexs.push_back(BasePBRMat::DefaultNormalTex.get());
				break;
			}
		}
		{
			for (size_t i = 0; i < mat->GetTextureCount(types[k]); i++)
			{
				aiString str;
				mat->GetTexture(types[k], i, &str);
				bool skip = false;

				for (size_t j = 0; j < m_Textures.size(); j++)
				{
					if (Utility::WstringToString(m_Textures[j]->GetName()) == str.C_Str())
					{
						skip = true;
						pTexs.push_back(m_Textures[j].get());
						break;
					}
				}
				if (!skip)
				{
					std::string Path = std::string(m_Directory) + '\\' + str.C_Str();
					m_Textures.push_back(std::move(TextureLoader::LoadTextureFromFile(Path, m_pDevice)));
					pTexs.push_back(m_Textures[m_Textures.size() - 1].get());
				}
			}
		}
	}
	m_Materials.emplace_back(m_pDevice, mat->GetName().C_Str(), pTexs, pbrParameter);
}
