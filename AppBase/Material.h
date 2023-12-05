#pragma once
#include"RenderDevice.h"
#include"TextureViewer.h"
#include"TextureLoader.h"
#include"Buffer.h"
#include"MathHelper.h"

struct PBRParameter
{
	XMFLOAT4 BaseColorFactor;
	float RoughnessFactor;
	float MetallicFactor;
	BOOL HasUV;
};

class BasePBRMat
{
public:
	BasePBRMat(RenderDevice* pDeivce, std::string Name, std::vector<Texture*> pTexs, PBRParameter Parameter);

	TextureViewer* GetTextureView() { return m_TexView.get(); }

	std::string GetName() { return m_Name; }

	std::unique_ptr<Buffer> m_ConstantsBuffer;

	static BasePBRMat DefaulMat(RenderDevice* pDeivce);
	static std::unique_ptr<Texture>DefaultBaseColorTex;
	static std::unique_ptr<Texture>DefaultRoughnessMetallicTex;
	static std::unique_ptr<Texture>DefaultNormalTex;

private:
	std::string m_Name;
	std::unique_ptr<TextureViewer>m_TexView;
	PBRParameter m_Parameter;
};