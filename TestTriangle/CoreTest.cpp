#include "CoreTest.h"
#include "Utility.h"
bool CoreTest::Initialize()
{
	if (!AppBase::Initialize())
		return false;

	//SwapChain
	m_SwapChain.reset(new SwapChain(m_hMainWnd, &m_Device));

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
	m_RootSignature.reset(new RootSignature());
	m_RootSignature->Finalize(L"Test Triangle RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, m_Device.g_Device);

	//PSO
	m_PSO.reset(new GraphicsPSO(L"Test Triangle PSO"));
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	m_PSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	m_PSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PSO->SetRenderTargetFormat(
		m_SwapChain->GetBackBufferSRGBFormat(), 
		m_SwapChain->GetDepthStencilFormat(),
		m_SwapChain->GetMSAACount(), 
		m_SwapChain->GetMSAAQuality()
	);
	m_PSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_PSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	m_PSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_PSO->SetSampleMask(UINT_MAX);
	m_PSO->SetVertexShader(reinterpret_cast<BYTE*>(vsByteCode->GetBufferPointer()), vsByteCode->GetBufferSize());
	m_PSO->SetPixelShader(reinterpret_cast<BYTE*>(psByteCode->GetBufferPointer()), psByteCode->GetBufferSize());
	m_PSO->SetRootSignature(*m_RootSignature);

	m_PSO->Finalize(m_Device.g_Device);
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
	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetPipelineState(*m_PSO);
	Context.SetRootSignature(*m_RootSignature);

	//Clear
	if (m_SwapChain->GetMSAACount() > 1)
	{
		Context.SetRenderTargets(1, &m_SwapChain->GetmsaaRenderTargetView(), &m_SwapChain->GetmsaaDepthStencilView());
		Context.TransitionResource(m_SwapChain->GetmsaaRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.ClearRenderTarget(m_SwapChain->GetmsaaRenderTargetView(), DirectX::Colors::Gray, nullptr);
		Context.ClearDepthAndStencil(m_SwapChain->GetmsaaDepthStencilView(), 1.0f, 0);
	}
	else
	{
		Context.SetRenderTargets(1, &m_SwapChain->GetBackBufferView(), &m_SwapChain->GetDepthBufferView());
		Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.ClearRenderTarget(m_SwapChain->GetBackBufferView(), DirectX::Colors::Gray, nullptr);
		Context.ClearDepthAndStencil(m_SwapChain->GetDepthBufferView(), 1.0f, 0);
	}

	Context.DrawInstanced(3, 1, 0, 0);
	if (m_SwapChain->GetMSAACount() > 1)
	{
		Context.TransitionResource(m_SwapChain->GetmsaaRenderTarget(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
		Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RESOLVE_DEST);
		Context.ResolveSubresource(m_SwapChain->GetBackBuffer(), 0, m_SwapChain->GetmsaaRenderTarget(), 0, m_SwapChain->GetBackBufferSRGBFormat());
	}
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);

	Context.Finish(true);
}

void CoreTest::Present()
{
	m_SwapChain->Present();
}

void CoreTest::OnResize()
{
	m_SwapChain->Resize();

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


	CoreTest theApp(hInstance, 1280, 1024);
	if (!theApp.Initialize())
		return 0;

	return theApp.Run();
}
