#pragma once
#include"Utility.h"
#include"RenderDevice.h"
#include"Texture.h"
#include"TextureViewer.h"
#include"MathHelper.h"

class TextureLoader
{
public:
	static std::unique_ptr<Texture> LoadTextureFromFile(std::string Path, RenderDevice* pDevice, bool AutoGenMipMaps = true);

	static void Init();

	static bool isInit;
private:
	TextureLoader();
};