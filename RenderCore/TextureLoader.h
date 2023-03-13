#pragma once
#include"Utility.h"
#include"RenderDevice.h"
#include"Texture.h"
#include"TextureViewer.h"
#include"MathHelper.h"
#include"DDSTextureLoader12.h"
#include"WICTextureLoader12.h"
std::unique_ptr<Texture> LoadTextureFromFile(std::string Path, RenderDevice* m_pDevicem);
