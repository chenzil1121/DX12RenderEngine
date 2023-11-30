#pragma once
#include"Texture.h"

enum class TextureType
{
	Texture1D,
	Texture2D,
	Texture2DArray,
	Texture3D,
	TextureCubeMap,
};

enum class TextureViewerType
{
	RTV,
	DSV,
	SRV,
	UAV,
};

//SRV UAV都在GPU，当材质数量过多才需要SRV UAV暂存在CPU中
struct TextureViewerDesc
{
	TextureType TexType;
	TextureViewerType ViewerType;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	//最细节的mipmap
	uint32_t MostDetailedMip;
	//使用的mipmap数量
	uint32_t NumMipLevels;

	union
	{
		/// For a texture array, first array slice to address in the view
		uint32_t FirstArraySlice;

		/// For a 3D texture, first depth slice to address the view
		uint32_t FirstDepthSlice;
	};

	union
	{
		/// For a texture array, number of array slices to address in the view.
		/// Set to 0 to address all array slices.
		uint32_t NumArraySlices;
		/// For a 3D texture, number of depth slices to address in the view
		/// Set to 0 to address all depth slices.
		uint32_t NumDepthSlices;
	};
};

class TextureViewer
{
public:
	TextureViewer(RenderDevice* Device, Texture* tex, TextureViewerDesc ViewerDesc, bool GpuHeapflag);
	TextureViewer(RenderDevice* Device, std::vector<Texture*>& texs, TextureViewerDesc* ViewerDescs, bool GpuHeapflag);

	~TextureViewer()
	{
		if (IsGpuHeap)
			pDevice->FreeGPUDescriptors(m_Viewer.GetDescriptorHeap()->GetDesc().Type, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_Viewer));
		else
			pDevice->FreeCPUDescriptors(m_Viewer.GetDescriptorHeap()->GetDesc().Type, D3D12_COMMAND_LIST_TYPE_DIRECT, std::move(m_Viewer));
	};

	void CreateView(std::vector<Texture*>& texs, TextureViewerDesc* ViewerDescs, bool flag, UINT Count);

	void CreateRTV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset);
	
	void CreateDSV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset);
	
	void CreateSRV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset);

	void CreateUAV(Texture* tex, TextureViewerDesc ViewerDesc, UINT Offset);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t Offset = 0)
	{
		return m_Viewer.GetCpuHandle(Offset);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t Offset = 0)
	{
		return m_Viewer.GetGpuHandle(Offset);
	}

	UINT64 GetFirstGpuHandleOffsetInHeap()
	{
		return m_Viewer.GetFirstGpuHandleOffsetInHeap();
	}

	std::pair<D3D12_DESCRIPTOR_HEAP_TYPE, ID3D12DescriptorHeap*> GetHeapInfo()
	{
		auto pHeap = m_Viewer.GetDescriptorHeap();
		return std::make_pair(pHeap->GetDesc().Type, pHeap);
	}

	TextureViewerDesc GetDesc(UINT Offset = 0)
	{
		return m_Desc[Offset];
	}

private:
	RenderDevice* pDevice;
	bool IsGpuHeap = false;
	DescriptorHeapAllocation m_Viewer;
	std::vector<TextureViewerDesc> m_Desc;
	UINT Count;
};

