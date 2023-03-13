#include "TextureLoader.h"

std::unique_ptr<Texture> LoadTextureFromFile(std::string Path, RenderDevice* m_pDevice)
{
	auto last = Path.find_last_of('.');
	std::string ext = Path.substr(last + 1, Path.length() - last - 1);

	const char* Pathcstr = Path.c_str();
	//��һ�ε��÷���ת������ַ������ȣ�����ȷ��Ϊwchar_t*���ٶ����ڴ�ռ�
	int pSize = MultiByteToWideChar(CP_OEMCP, 0, Pathcstr, strlen(Pathcstr) + 1, NULL, 0);
	wchar_t* Pathwcstr = new wchar_t[pSize];
	//�ڶ��ε��ý����ֽ��ַ���ת����˫�ֽ��ַ���
	MultiByteToWideChar(CP_OEMCP, 0, Pathcstr, strlen(Pathcstr) + 1, Pathwcstr, pSize);

	std::unique_ptr<Texture> texture;

	if (ext == "dds")
	{
		ID3D12Resource* tex = nullptr;
		std::unique_ptr<uint8_t[]> ddsData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		LoadDDSTextureFromFile(m_pDevice->g_Device, Pathwcstr, &tex, ddsData, subresources);
		texture.reset(new Texture(m_pDevice, tex->GetDesc(), subresources.data(), tex, Pathwcstr));
	}
	else
	{
		ID3D12Resource* tex = nullptr;
		std::unique_ptr<uint8_t[]> decodedData;
		D3D12_SUBRESOURCE_DATA subresource;
		LoadWICTextureFromFile(m_pDevice->g_Device, Pathwcstr, &tex, decodedData, subresource);
		texture.reset(new Texture(m_pDevice, tex->GetDesc(), &subresource, tex, Pathwcstr));
	}

	delete[] Pathwcstr;
	return std::move(texture);
}
