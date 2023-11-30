#include "Material.h"

std::unique_ptr<Texture>BasePBRMat::DefaultBaseColorTex;
std::unique_ptr<Texture>BasePBRMat::DefaultRoughnessMetallicTex;
std::unique_ptr<Texture>BasePBRMat::DefaultNormalTex;

BasePBRMat::BasePBRMat(RenderDevice* pDeivce, std::string Name, std::vector<Texture*> pTexs, PBRParameter Parameter):
	m_Name(Name),
	m_Parameter(Parameter)
{
	std::array<TextureViewerDesc, 3>texViewDescs;
	for (size_t i = 0; i < texViewDescs.size(); i++)
	{
		texViewDescs[i].TexType = TextureType::Texture2D;
		texViewDescs[i].ViewerType = TextureViewerType::SRV;
		texViewDescs[i].MostDetailedMip = 0;
		texViewDescs[i].NumMipLevels = pTexs[i]->GetDesc().MipLevels;
	}

	m_TexView.reset(new TextureViewer(pDeivce, pTexs, texViewDescs.data(), true));

	GraphicsContext& Context = pDeivce->BeginGraphicsContext(L"Init PBRTexture State");
	for (int i = 0; i < 3; i++)
	{
		Context.TransitionResource(pTexs[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	Context.Finish(true);

	m_ConstantsBuffer.reset(new Buffer(pDeivce, &m_Parameter, (UINT)(sizeof(PBRParameter)), false, true));
}

BasePBRMat BasePBRMat::DefaulMat(RenderDevice* pDeivce)
{
	PBRParameter parameter;
	parameter.BaseColorFactor = { 1.0f,1.0f,1.0f,1.0f };
	parameter.MetallicFactor = 1.0f;
	parameter.RoughnessFactor = 1.0f;
	parameter.HasUV = FALSE;

	D3D12_RESOURCE_DESC texDesc;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1;
	texDesc.Height = 1;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE clearColor;
	clearColor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	memcpy(clearColor.Color, Colors::White, sizeof(float) * 4);

	if (!DefaultBaseColorTex)
	{
		DefaultBaseColorTex.reset(new Texture(pDeivce, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"Default BaseColorTex"));
		
		TextureViewerDesc viewDesc;
		viewDesc.TexType = TextureType::Texture2D;
		viewDesc.ViewerType = TextureViewerType::RTV;
		viewDesc.MostDetailedMip = 0;
		std::unique_ptr<TextureViewer> view;
		view.reset(new TextureViewer(pDeivce, DefaultBaseColorTex.get(), viewDesc, false));

		GraphicsContext& Context = pDeivce->BeginGraphicsContext(L"Init DefaultBaseColorTex");
		Context.SetRenderTargets(1, &view->GetCpuHandle());
		Context.ClearRenderTarget(view->GetCpuHandle(), clearColor.Color, nullptr);
		Context.TransitionResource(DefaultBaseColorTex.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.Finish();
	}
	if (!DefaultRoughnessMetallicTex)
	{
		clearColor.Color[0] = 1.0;
		clearColor.Color[1] = 1.0;
		clearColor.Color[2] = 1.0;
		clearColor.Color[3] = 1.0;
		DefaultRoughnessMetallicTex.reset(new Texture(pDeivce, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"Default RoughnessMetallicTex"));
		
		TextureViewerDesc viewDesc;
		viewDesc.TexType = TextureType::Texture2D;
		viewDesc.ViewerType = TextureViewerType::RTV;
		viewDesc.MostDetailedMip = 0;
		std::unique_ptr<TextureViewer> view;
		view.reset(new TextureViewer(pDeivce, DefaultRoughnessMetallicTex.get(), viewDesc, false));

		GraphicsContext& Context = pDeivce->BeginGraphicsContext(L"Init DefaultRoughnessMetallicTex");
		Context.SetRenderTargets(1, &view->GetCpuHandle());
		Context.ClearRenderTarget(view->GetCpuHandle(), clearColor.Color, nullptr);
		Context.TransitionResource(DefaultRoughnessMetallicTex.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.Finish();
	}
	if (!DefaultNormalTex)
	{
		clearColor.Color[0] = 0.5;
		clearColor.Color[1] = 0.5;
		clearColor.Color[2] = 1.0;
		clearColor.Color[3] = 1.0;
		DefaultNormalTex.reset(new Texture(pDeivce, texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, clearColor, L"Default NormalTex"));
		
		TextureViewerDesc viewDesc;
		viewDesc.TexType = TextureType::Texture2D;
		viewDesc.ViewerType = TextureViewerType::RTV;
		viewDesc.MostDetailedMip = 0;
		std::unique_ptr<TextureViewer> view;
		view.reset(new TextureViewer(pDeivce, DefaultNormalTex.get(), viewDesc, false));

		GraphicsContext& Context = pDeivce->BeginGraphicsContext(L"Init DefaultNormalTex");
		Context.SetRenderTargets(1, &view->GetCpuHandle());
		Context.ClearRenderTarget(view->GetCpuHandle(), clearColor.Color, nullptr);
		Context.TransitionResource(DefaultNormalTex.get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		Context.Finish(true);
	}

	std::vector<Texture*> texs = { DefaultBaseColorTex.get(),DefaultRoughnessMetallicTex.get(),DefaultNormalTex.get() };
	return BasePBRMat(pDeivce, "DefaulMat", texs, parameter);
}
