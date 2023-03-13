#include"Model.h"

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<std::uint16_t> indices, std::vector<TextureInfo> textures, XMFLOAT4X4 transformation, std::string alphaMode, RenderDevice* Device):
	m_Vertices(vertices),
	m_Indices(indices),
	m_TextureInfos(textures),
	m_AlphaMode(alphaMode)
{
	m_Constants.World = transformation;
	XMStoreFloat4x4(&m_Constants.WorldInvertTran, MathHelper::InverseTranspose(XMLoadFloat4x4(&m_Constants.World)));
	SetupMesh(Device);
}

void Mesh::SetupMesh(RenderDevice* Device)
{
	m_VertexBuffer.reset(new Buffer(Device, m_Vertices.data(), (UINT)(m_Vertices.size() * sizeof(Vertex)), false, false));
	m_IndexBuffer.reset(new Buffer(Device, m_Indices.data(), (UINT)(m_Indices.size() * sizeof(std::uint16_t)), false, false));
}

void Model::LoadModel(std::string path)
{
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_ConvertToLeftHanded | aiProcess_Triangulate);
	ASSERT(scene && !(scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && scene->mRootNode, import.GetErrorString());
	Utility::Printf("Load %s succeed!\n%d Meshes %d Materials %d Animations %d Textures %d Lights %d Cameras\n", path.c_str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumAnimations, scene->mNumTextures, scene->mNumLights, scene->mNumCameras);

	m_Directory = path.substr(0, path.find_last_of('\\'));

	ProcessNode(scene->mRootNode, scene, MathHelper::Identity4x4());
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, XMFLOAT4X4 accTransform)
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

void Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, XMFLOAT4X4 transformation)
{
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	std::vector<TextureInfo> textures;

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
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		material->Get(AI_MATKEY_GLTF_ALPHAMODE, aiAlphaMode);
		
		std::vector<TextureInfo>diffuseMaps = LoadMaterialTextures(material, aiTextureType_BASE_COLOR, "BaseColor");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		
		std::vector<TextureInfo>roughnessMetallicMaps = LoadMaterialTextures(material, aiTextureType_METALNESS, "RoughnessMetallic");
		textures.insert(textures.end(), roughnessMetallicMaps.begin(), roughnessMetallicMaps.end());
		
		std::vector<TextureInfo>normalMaps = LoadMaterialTextures(material, aiTextureType_NORMALS, "Normal");
		textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());	
	}
	if (aiAlphaMode == aiString("OPAQUE"))
		m_Meshes[static_cast<size_t>(LayerType::Opaque)].emplace_back(Mesh(vertices, indices, textures, transformation, aiAlphaMode.C_Str(), m_pDevice));
	else if(aiAlphaMode == aiString("BLEND"))
		m_Meshes[static_cast<size_t>(LayerType::Blend)].emplace_back(Mesh(vertices, indices, textures, transformation, aiAlphaMode.C_Str(), m_pDevice));
	
}

std::vector<TextureInfo> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
{
	std::vector<TextureInfo> textureInfos;
	for (size_t i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		bool skip = false;
		for (size_t j = 0; j < m_TextureInfosLoaded.size(); j++)
		{
			if (std::strcmp(m_TextureInfosLoaded[j].path.C_Str(), str.C_Str()) == 0)
			{
				textureInfos.push_back(m_TextureInfosLoaded[j]);
				skip = true;
				break;
			}
		}
		if (!skip)
		{
			std::string Path = std::string(m_Directory) + '\\' + str.C_Str();
			m_Textures.push_back(std::move(LoadTextureFromFile(Path, m_pDevice)));

			TextureInfo textureInfo;
			textureInfo.id = m_Textures.size() - 1;
			textureInfo.type = typeName;
			textureInfo.path = str;
			m_TextureInfosLoaded.push_back(textureInfo);
			textureInfos.push_back(textureInfo);

			TextureViewerDesc desc;
			auto texDesc = m_Textures[m_Textures.size() - 1]->GetDesc();
			desc.TexType = TextureType::Texture2D;
			desc.ViewerType = TextureViewerType::SRV;
			desc.MostDetailedMip = 0;
			desc.NumMipLevels = texDesc.MipLevels;
			m_TextureViewers.emplace_back(new TextureViewer(m_pDevice, m_Textures[m_Textures.size() - 1].get(), desc, true));
		}
	}
	return textureInfos;
}
