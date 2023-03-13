#include "CoreTest.h"
#include "Utility.h"
bool CoreTest::Initialize()
{
	if (!AppBase::Initialize())
		return false;

	//SwapChain
	m_SwapChain.Create(m_hMainWnd, &m_Device);

	CreateResources();
	return true;
}

void CoreTest::CreateResources()
{
	//Vertex Index
	vertices =
	{
		VertexTri({XMFLOAT3(-0.5f, -0.5f, 0.0f),XMFLOAT4(Colors::Red)}),
		VertexTri({XMFLOAT3(0.0f, +0.5f, 0.0f),XMFLOAT4(Colors::Green)}),
		VertexTri({XMFLOAT3(+0.5f, -0.5f, 0.0f),XMFLOAT4(Colors::Blue)})
	};

	indices =
	{
		0,1,2
	};

	m_VertexBuffer.reset(new Buffer(&m_Device, vertices.data(), (UINT)(vertices.size() * sizeof(VertexTri)), false, false));
	m_IndexBuffer.reset(new Buffer(&m_Device, indices.data(), (UINT)(indices.size() * sizeof(uint16_t)), false, false));

	//Shader
	ComPtr<ID3DBlob> vsByteCode = nullptr;
	ComPtr<ID3DBlob> psByteCode = nullptr;

	vsByteCode = Utility::CompileShader(L"Shader\\vs.hlsl", nullptr, "main", "vs_5_0");
	psByteCode = Utility::CompileShader(L"Shader\\ps.hlsl", nullptr, "main", "ps_5_0");

	//RootSignature
	m_RootSignature.Finalize(L"Test", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, m_Device.g_Device);

	//PSO
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	m_PSO.SetInputLayout(mInputLayout.size(), mInputLayout.data());
	m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PSO.SetRenderTargetFormat(
		m_SwapChain.GetDesc()->BufferDesc.Format, 
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		m_SwapChain.GetDesc()->SampleDesc.Count, 
		m_SwapChain.GetDesc()->SampleDesc.Quality
	);
	m_PSO.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_PSO.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	m_PSO.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_PSO.SetSampleMask(UINT_MAX);
	m_PSO.SetVertexShader(reinterpret_cast<BYTE*>(vsByteCode->GetBufferPointer()), vsByteCode->GetBufferSize());
	m_PSO.SetPixelShader(reinterpret_cast<BYTE*>(psByteCode->GetBufferPointer()), psByteCode->GetBufferSize());
	m_PSO.SetRootSignature(m_RootSignature);

	m_PSO.Finalize(m_Device.g_Device);
}

void CoreTest::Update(const GameTimer& gt)
{
	;
}

void CoreTest::Render(const GameTimer& gt)
{
	GraphicsContext& Context = m_Device.BeginGraphicsContext(L"Test");

	Context.SetVertexBuffer(0, { m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(vertices.size() * sizeof(VertexTri)),sizeof(VertexTri) });
	Context.SetIndexBuffer({ m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(indices.size() * sizeof(uint16_t)),DXGI_FORMAT_R16_UINT });
	Context.SetPipelineState(m_PSO);
	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.TransitionResource(m_SwapChain.GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(m_SwapChain.GetBackBufferView(), DirectX::Colors::Black, nullptr);
	Context.ClearDepthAndStencil(m_SwapChain.GetDepthBufferView(), 1.0f, 0);
	Context.SetRenderTargets(1, &m_SwapChain.GetBackBufferView(), m_SwapChain.GetDepthBufferView());
	Context.SetRootSignature(m_RootSignature);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.DrawInstanced(3, 1, 0, 0);
	Context.TransitionResource(m_SwapChain.GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
	Context.Finish(true);
}

void CoreTest::Present()
{
	m_SwapChain.Present();
}

void CoreTest::OnResize()
{
	m_SwapChain.Resize();

	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = static_cast<float>(m_Device.g_DisplayWidth);
	m_viewport.Height = static_cast<float>(m_Device.g_DisplayHeight);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissor = { 0, 0, m_Device.g_DisplayWidth, m_Device.g_DisplayHeight };
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif


	CoreTest theApp(hInstance);
	if (!theApp.Initialize())
		return 0;

	return theApp.Run();
}
