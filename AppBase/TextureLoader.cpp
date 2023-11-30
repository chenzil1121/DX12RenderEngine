#include"TextureLoader.h"
#include"ResourceUploadBatch.h"
#include"DirectXTex.h"
#include"WICTextureLoader.h"
#include"DDSTextureLoader.h"

bool TextureLoader::isInit = false;

std::unique_ptr<Texture> TextureLoader::LoadTextureFromFile(std::string Path, RenderDevice* pDevice, bool AutoGenMipMaps)
{
	Init();

	auto last = Path.find_last_of('.');
	std::string ext = Path.substr(last + 1, Path.length() - last - 1);

	const char* Pathcstr = Path.c_str();
	//第一次调用返回转换后的字符串长度，用于确认为wchar_t*开辟多大的内存空间
	int pSize = MultiByteToWideChar(CP_OEMCP, 0, Pathcstr, strlen(Pathcstr) + 1, NULL, 0);
	wchar_t* Pathwcstr = new wchar_t[pSize];
	//第二次调用将单字节字符串转换成双字节字符串
	MultiByteToWideChar(CP_OEMCP, 0, Pathcstr, strlen(Pathcstr) + 1, Pathwcstr, pSize);

	std::unique_ptr<Texture> texture;

	if (ext == "dds")
	{
		ID3D12Resource* tex = nullptr;
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		LoadDDSTextureFromFile(pDevice->g_Device, Pathwcstr, &tex, ddsData, subresources);
		texture.reset(new Texture(pDevice, tex->GetDesc(), subresources.data(), tex, Pathwcstr));
	}
	else
	{
		ID3D12Resource* tex = nullptr;
		auto image = std::make_unique<ScratchImage>();
		auto mipChain = std::make_unique<ScratchImage>();
		LoadFromWICFile(Pathwcstr, WIC_FLAGS_NONE, nullptr, *image);

		if (IsBGR(image->GetMetadata().format))
		{
			GenerateMipMaps(image->GetImages(), image->GetImageCount(), image->GetMetadata(), TEX_FILTER_DEFAULT, 0, *mipChain);

			CreateTexture(pDevice->g_Device, mipChain->GetMetadata(), &tex);

			std::vector<D3D12_SUBRESOURCE_DATA> subresources;

			PrepareUpload(pDevice->g_Device, mipChain->GetImages(), mipChain->GetImageCount(), mipChain->GetMetadata(), subresources);

			texture.reset(new Texture(pDevice, tex->GetDesc(), subresources.data(), tex, Pathwcstr));
		}
		else
		{
			std::unique_ptr<uint8_t[]> decodedData;
			D3D12_SUBRESOURCE_DATA subresource;

			LoadWICTextureFromFileEx(pDevice->g_Device, Pathwcstr, 0, D3D12_RESOURCE_FLAG_NONE, WIC_LOADER_MIP_RESERVE, &tex, decodedData, subresource);

			ResourceUploadBatch upload(pDevice->g_Device);

			upload.Begin();

			upload.Upload(tex, 0, &subresource, 1);

			upload.Transition(tex,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			if (upload.IsSupportedForGenerateMips(tex->GetDesc().Format))
				upload.GenerateMips(tex);

			// Upload the resources to the GPU.
			auto finish = upload.End(pDevice->g_CommandManager.GetGraphicsQueue().GetCommandQueue());

			// Wait for the upload thread to terminate
			finish.wait();

			texture.reset(new Texture(pDevice, tex->GetDesc(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, tex, Pathwcstr));
		}
	}
	Utility::Print(Pathwcstr);
	Utility::Print("\n");
	delete[] Pathwcstr;
	return std::move(texture);
}

void TextureLoader::Init()
{
	if (!isInit)
		isInit = true;
	else
		return;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))

	return;
}
