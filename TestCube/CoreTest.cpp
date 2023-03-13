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
	//Camera
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_Camera.LookAt(pos, target, up);

	//Vertex Index
	vertices =
	{
		VertexCube({XMFLOAT3(-0.5,-0.5,-0.5),XMFLOAT4(1,0,0,1)}),
		VertexCube({XMFLOAT3(-0.5,+0.5,-0.5),XMFLOAT4(0,1,0,1)}),
		VertexCube({XMFLOAT3(+0.5,+0.5,-0.5),XMFLOAT4(0,0,1,1)}),
		VertexCube({XMFLOAT3(+0.5,-0.5,-0.5),XMFLOAT4(1,1,1,1)}),

		VertexCube({XMFLOAT3(-0.5,-0.5,+0.5),XMFLOAT4(1,1,0,1)}),
		VertexCube({XMFLOAT3(-0.5,+0.5,+0.5),XMFLOAT4(0,1,1,1)}),
		VertexCube({XMFLOAT3(+0.5,+0.5,+0.5),XMFLOAT4(1,0,1,1)}),
		VertexCube({XMFLOAT3(+0.5,-0.5,+0.5),XMFLOAT4(0.2f,0.2f,0.2f,1)})
	};

	indices =
	{
		2,0,1, 2,3,0,
		4,6,5, 4,7,6,
		0,7,4, 0,3,7,
		1,0,4, 1,4,5,
		1,5,2, 5,6,2,
		3,6,7, 3,2,6
	};

	m_VertexBuffer.reset(new Buffer(&m_Device, vertices.data(), (UINT)(vertices.size() * sizeof(VertexCube)), false, false));
	m_IndexBuffer.reset(new Buffer(&m_Device, indices.data(), (UINT)(indices.size() * sizeof(uint16_t)), false, false));

	//Shader
	ComPtr<ID3DBlob> vsByteCode = nullptr;
	ComPtr<ID3DBlob> psByteCode = nullptr;

	vsByteCode = Utility::CompileShader(L"Shader\\vs.hlsl", nullptr, "main", "vs_5_0");
	psByteCode = Utility::CompileShader(L"Shader\\ps.hlsl", nullptr, "main", "ps_5_0");

	//RootSignature
	m_RootSignature.Reset(1, 0);
	m_RootSignature[0].InitAsConstantBuffer(0);
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
	OnKeyboardInput(gt);
	Angle = gt.TotalTime();

	//XMMATRIX world = XMMatrixRotationY(Angle);
	XMMATRIX world = XMLoadFloat4x4(&MathHelper::Identity4x4());
	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();

 	XMMATRIX worldViewProj = world * view * proj;

	XMFLOAT4X4 tmp;
	XMStoreFloat4x4(&tmp, view * proj);

	XMStoreFloat4x4(&WorldViewProj, worldViewProj);

	//ConstantBuffer
	m_ObjConstantBuffer.reset(new Buffer(&m_Device, nullptr, (UINT)(16 * sizeof(float)), true, true));
	m_ObjConstantBuffer->Upload(WorldViewProj.m);
}

void CoreTest::Render(const GameTimer& gt)
{
	GraphicsContext& Context = m_Device.BeginGraphicsContext(L"Test");

	Context.SetVertexBuffer(0, { m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(vertices.size() * sizeof(VertexCube)),sizeof(VertexCube) });
	Context.SetIndexBuffer({ m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(indices.size() * sizeof(uint16_t)),DXGI_FORMAT_R16_UINT });
	Context.SetPipelineState(m_PSO);
	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.TransitionResource(m_SwapChain.GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(m_SwapChain.GetBackBufferView(), DirectX::Colors::Black, nullptr);
	Context.ClearDepthAndStencil(m_SwapChain.GetDepthBufferView(), 1.0f, 0);
	Context.SetRenderTargets(1, &m_SwapChain.GetBackBufferView(), m_SwapChain.GetDepthBufferView());
	Context.SetRootSignature(m_RootSignature);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetDynamicConstantBufferView(0, m_ObjConstantBuffer->GetGpuVirtualAddress());
	Context.DrawIndexedInstanced(36, 1, 0, 0, 0);
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

	m_Camera.SetLens(XM_PIDIV4, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight, 0.1f, 1000.0f);
}



void CoreTest::OnMouseDown(WPARAM btnState, int x, int y)
{
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture(m_hMainWnd);
}

void CoreTest::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void CoreTest::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_LastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_LastMousePos.y));

		m_Camera.Pitch(dy);
		m_Camera.RotateY(dx);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void CoreTest::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		m_Camera.Walk(5.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_Camera.Walk(-5.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_Camera.Strafe(-5.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_Camera.Strafe(5.0f * dt);

	m_Camera.UpdateViewMatrix();
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
