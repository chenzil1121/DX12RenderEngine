#include "CoreTest.h"
#include "Utility.h"

bool CoreTest::Initialize()
{
	if (!AppBase::Initialize())
		return false;

	//SwapChain
	m_SwapChain.reset(new SwapChain(m_hMainWnd, &m_Device));

	//ImGui
	m_ImGui.reset(new ImGuiWrapper(m_hMainWnd, &m_Device, SWAP_CHAIN_BUFFER_COUNT, m_SwapChain->GetBackBufferSRGBFormat()));

	CreateResources();
	return true;
}

void CoreTest::CreateResources()
{
	//Camera
	XMVECTOR pos = XMVectorSet(0.0, 0.0, -1.5, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_Camera.LookAt(pos, target, up);

	//Model
	//m_Model.reset(new Model("..\\Asset\\FlightHelmet\\FlightHelmet.gltf", &m_Device));
	//m_Model.reset(new Model("..\\Asset\\DamagedHelmet\\DamagedHelmet.gltf", &m_Device));
	//m_Model.reset(new Model("..\\Asset\\BoomBoxWithAxes\\BoomBoxWithAxes.gltf", &m_Device));
	m_Model.reset(new Model("..\\Asset\\MetalRoughSpheresNoTextures\\MetalRoughSpheresNoTextures.gltf", &m_Device));

	m_Model->AddDirectionalLight({ 3.0f, 3.0f, 3.0f }, { m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] });
	m_Model->m_LightBuffers.reset(new Buffer(&m_Device, m_Model->m_Lights.data(), m_Model->m_Lights.size() * sizeof(Light), true, true));
	m_MainPassCB.FirstLightIndex = XMINT3(1, 0, 0);
	
	//EnvMapPass
	m_EnvMapPass.reset(new EnvMap(&m_Device, m_SwapChain.get(), "..\\Asset\\EnvMap\\papermill.dds"));

	//IBL
	m_IBL.reset(new IBL(&m_Device, m_EnvMapPass->GetEnvMapView()));

	CreateBasePass();
}

void CoreTest::CreateBasePass()
{
	//Shader
	ComPtr<ID3DBlob> BasePassVsByteCode = nullptr;
	ComPtr<ID3DBlob> BasePassPsByteCode = nullptr;

	BasePassVsByteCode = Utility::CompileShader(L"Shader\\BasePass.hlsl", nullptr, "VS", "vs_5_1");
	BasePassPsByteCode = Utility::CompileShader(L"Shader\\BasePass.hlsl", nullptr, "PS", "ps_5_1");

	//BasePass RootSignature
	m_BasePassRS.reset(new RootSignature(6, 2));
	m_BasePassRS->InitStaticSampler(0, Sampler::LinearWrap);
	m_BasePassRS->InitStaticSampler(1, Sampler::LinearClamp);
	(*m_BasePassRS)[0].InitAsDescriptorTable(1);
	(*m_BasePassRS)[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3);
	(*m_BasePassRS)[1].InitAsDescriptorTable(1);
	(*m_BasePassRS)[1].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 3);
	
	(*m_BasePassRS)[2].InitAsConstantBuffer(0);
	(*m_BasePassRS)[3].InitAsConstantBuffer(1);
	(*m_BasePassRS)[4].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_ALL, 1);
	(*m_BasePassRS)[5].InitAsConstantBuffer(2);

	m_BasePassRS->Finalize(L"BasePassRootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, m_Device.g_Device);

	//BasePass PSO
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	//Opaque
	m_BasePassOpaquePSO.reset(new GraphicsPSO(L"BasePassPSO"));
	m_BasePassOpaquePSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	m_BasePassOpaquePSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_BasePassOpaquePSO->SetRenderTargetFormat(
		m_SwapChain->GetBackBufferSRGBFormat(),
		m_SwapChain->GetDepthStencilFormat(),
		m_SwapChain->GetMSAACount(),
		m_SwapChain->GetMSAAQuality()
	);
	m_BasePassOpaquePSO->SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT));
	m_BasePassOpaquePSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	m_BasePassOpaquePSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_BasePassOpaquePSO->SetSampleMask(UINT_MAX);
	m_BasePassOpaquePSO->SetVertexShader(reinterpret_cast<BYTE*>(BasePassVsByteCode->GetBufferPointer()), BasePassVsByteCode->GetBufferSize());
	m_BasePassOpaquePSO->SetPixelShader(reinterpret_cast<BYTE*>(BasePassPsByteCode->GetBufferPointer()), BasePassPsByteCode->GetBufferSize());
	m_BasePassOpaquePSO->SetRootSignature(*m_BasePassRS);

	m_BasePassOpaquePSO->Finalize(m_Device.g_Device);
	//Blend
	m_BasePassBlendPSO.reset(new GraphicsPSO(L"BasePassPSO"));
	m_BasePassBlendPSO->SetInputLayout(mInputLayout.size(), mInputLayout.data());
	m_BasePassBlendPSO->SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_BasePassBlendPSO->SetRenderTargetFormat(
		m_SwapChain->GetBackBufferSRGBFormat(),
		m_SwapChain->GetDepthStencilFormat(),
		m_SwapChain->GetMSAACount(),
		m_SwapChain->GetMSAAQuality()
	);
	CD3DX12_BLEND_DESC blendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendState.RenderTarget[0].BlendEnable = TRUE;
	blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	m_BasePassBlendPSO->SetBlendState(blendState);
	m_BasePassBlendPSO->SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));
	m_BasePassBlendPSO->SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT));
	m_BasePassBlendPSO->SetSampleMask(UINT_MAX);
	m_BasePassBlendPSO->SetVertexShader(reinterpret_cast<BYTE*>(BasePassVsByteCode->GetBufferPointer()), BasePassVsByteCode->GetBufferSize());
	m_BasePassBlendPSO->SetPixelShader(reinterpret_cast<BYTE*>(BasePassPsByteCode->GetBufferPointer()), BasePassPsByteCode->GetBufferSize());
	m_BasePassBlendPSO->SetRootSignature(*m_BasePassRS);

	m_BasePassBlendPSO->Finalize(m_Device.g_Device);
}

void CoreTest::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	//ObjectConstantBuffer
	for (size_t i = 0; i < m_Model->m_Meshes.size(); i++)
	{
		for (size_t j = 0; j < m_Model->m_Meshes[i].size(); j++)
		{
			auto meshWorld = XMLoadFloat4x4(&m_Model->m_Meshes[i][j].m_Constants.World);
			//Rotation
			auto mat4 = mat4_cast(m_ModelRotation);
			auto modelRotation = XMLoadFloat4x4(&XMFLOAT4X4(mat4.m00, mat4.m01, mat4.m02, mat4.m03,
				mat4.m10, mat4.m11, mat4.m12, mat4.m13,
				mat4.m20, mat4.m21, mat4.m22, mat4.m23,
				mat4.m30, mat4.m31, mat4.m32, mat4.m33));
			//Scale
			auto modelScale = XMMatrixScaling(100.0f, 100.0f, 100.0f);

			auto Mat = meshWorld * modelRotation * modelScale;
			//auto Mat = meshWorld * modelRotation;

			auto MatInvertTran = MathHelper::InverseTranspose(Mat);
			XMFLOAT4X4 finalWorld;
			XMFLOAT4X4 finalWorldInvertTran;
			XMStoreFloat4x4(&finalWorld, XMMatrixTranspose(Mat));
			XMStoreFloat4x4(&finalWorldInvertTran, XMMatrixTranspose(MatInvertTran));
			MeshConstants meshConstants;
			meshConstants.World = finalWorld;
			meshConstants.WorldInvertTran = finalWorldInvertTran;

			m_Model->m_Meshes[i][j].m_ConstantsBuffer.reset(new Buffer(&m_Device, nullptr, (UINT)(sizeof(MeshConstants)), true, true));
			m_Model->m_Meshes[i][j].m_ConstantsBuffer->Upload(&meshConstants);
		}
	}

	//PassConstantBuffer
	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();
	XMFLOAT3 pos = m_Camera.GetPosition3f();

	XMStoreFloat4x4(&m_MainPassCB.ViewProj, XMMatrixTranspose(view * proj));
	XMStoreFloat4x4(&m_MainPassCB.ViewProjInvert, XMMatrixTranspose(MathHelper::Inverse(view * proj)));
	m_MainPassCB.CameraPos = pos;
	m_MainPassCB.PrefilteredEnvMipLevels = 9;

	m_PassConstantBuffer.reset(new Buffer(&m_Device, nullptr, (UINT)(sizeof(PassConstants)), true, true));
	m_PassConstantBuffer->Upload(&m_MainPassCB);

	//LightsBuffer
	m_Model->m_Lights[0].Direction = { m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] };
	m_Model->m_LightBuffers.reset(new Buffer(&m_Device, m_Model->m_Lights.data(), m_Model->m_Lights.size() * sizeof(Light), true, true));
	m_Model->m_LightBuffers->Upload(m_Model->m_Lights.data());
	
	//IMGUI
	UpdateUI();
}

void CoreTest::UpdateUI()
{
	m_ImGui->NewFrame();
	
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::Button("Load model"))
		{
			;
		}
		ImGui::gizmo3D("Model Rotation", m_ModelRotation, ImGui::GetTextLineHeight() * 10);
		ImGui::SameLine();
		ImGui::gizmo3D("Light direction", m_LightDirection, ImGui::GetTextLineHeight() * 10);
		
		std::array<const char*, static_cast<size_t>(DebugViewType::NumDebugViews)> DebugViews;
		DebugViews[static_cast<size_t>(DebugViewType::None)] = "None";
		DebugViews[static_cast<size_t>(DebugViewType::BaseColor)] = "Base Color";
		DebugViews[static_cast<size_t>(DebugViewType::Transparency)] = "Transparency";
		DebugViews[static_cast<size_t>(DebugViewType::NormalMap)] = "Normal Map";
		DebugViews[static_cast<size_t>(DebugViewType::Occlusion)] = "Occlusion";
		DebugViews[static_cast<size_t>(DebugViewType::Emissive)] = "Emissive";
		DebugViews[static_cast<size_t>(DebugViewType::Metallic)] = "Metallic";
		DebugViews[static_cast<size_t>(DebugViewType::Roughness)] = "Roughness";
		DebugViews[static_cast<size_t>(DebugViewType::DiffuseColor)] = "Diffuse color";
		DebugViews[static_cast<size_t>(DebugViewType::F0)] = "F0 (Specular color)";
		DebugViews[static_cast<size_t>(DebugViewType::F90)] = "F90";
		DebugViews[static_cast<size_t>(DebugViewType::MeshNormal)] = "Mesh normal";
		DebugViews[static_cast<size_t>(DebugViewType::PerturbedNormal)] = "Perturbed normal";
		DebugViews[static_cast<size_t>(DebugViewType::NdotV)] = "n*v";
		DebugViews[static_cast<size_t>(DebugViewType::DiffuseIBL)] = "Diffuse IBL";
		DebugViews[static_cast<size_t>(DebugViewType::SpecularIBL)] = "Specular IBL";

		ImGui::Combo("Debug view", reinterpret_cast<int*>(&m_MainPassCB.DebugView), DebugViews.data(), static_cast<int>(DebugViews.size()));
	}
	ImGui::End();
	ImGui::Render();
}

void CoreTest::Render(const GameTimer& gt)
{
	GraphicsContext& Context = m_Device.BeginGraphicsContext(L"Render");
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetViewportAndScissor(m_viewport, m_scissor);
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
	//EnvMap
	m_EnvMapPass->Render(Context, m_PassConstantBuffer.get());
	//BasePass
	Context.SetPipelineState(*m_BasePassOpaquePSO);
	Context.SetRootSignature(*m_BasePassRS);
	Context.SetConstantBuffer(3, m_PassConstantBuffer->GetGpuVirtualAddress());
	Context.SetBufferSRV(4, m_Model->m_LightBuffers->GetGpuVirtualAddress());
	auto IBLView = m_IBL->GetIBLView();
	auto IBLViewHeapInfo = IBLView->GetHeapInfo();
	Context.SetDescriptorHeap(IBLViewHeapInfo.first, IBLViewHeapInfo.second);
	Context.SetDescriptorTable(1, IBLView->GetGpuHandle());

	//Opaque Mesh
	for (size_t i = 0; i < m_Model->m_Meshes[static_cast<size_t>(LayerType::Opaque)].size(); i++)
	{
		Mesh& mesh = m_Model->m_Meshes[static_cast<size_t>(LayerType::Opaque)][i];
		Context.SetVertexBuffer(0, { mesh.m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Vertices.size() * sizeof(Vertex)),sizeof(Vertex) });
		Context.SetIndexBuffer({ mesh.m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Indices.size() * sizeof(uint16_t)),DXGI_FORMAT_R16_UINT });
		Context.SetConstantBuffer(2, mesh.m_ConstantsBuffer->GetGpuVirtualAddress());
		Context.SetConstantBuffer(5, m_Model->m_Materials[mesh.m_MatID].m_ConstantsBuffer->GetGpuVirtualAddress());
		
		auto FirstTexView = m_Model->m_Materials[mesh.m_MatID].GetTextureView();
		auto HeapInfo = FirstTexView->GetHeapInfo();
		Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
		Context.SetDescriptorTable(0, FirstTexView->GetGpuHandle());

		Context.DrawIndexed(mesh.m_Indices.size());
	}
	//Blend Mesh
	Context.SetPipelineState(*m_BasePassBlendPSO);
	for (size_t i = 0; i < m_Model->m_Meshes[static_cast<size_t>(LayerType::Blend)].size(); i++)
	{
		Mesh& mesh = m_Model->m_Meshes[static_cast<size_t>(LayerType::Blend)][i];
		Context.SetVertexBuffer(0, { mesh.m_VertexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Vertices.size() * sizeof(Vertex)),sizeof(Vertex) });
		Context.SetIndexBuffer({ mesh.m_IndexBuffer->GetGpuVirtualAddress(),(UINT)(mesh.m_Indices.size() * sizeof(uint16_t)),DXGI_FORMAT_R16_UINT });
		Context.SetConstantBuffer(2, mesh.m_ConstantsBuffer->GetGpuVirtualAddress());
		Context.SetConstantBuffer(5, m_Model->m_Materials[mesh.m_MatID].m_ConstantsBuffer->GetGpuVirtualAddress());

		auto FirstTexView = m_Model->m_Materials[mesh.m_MatID].GetTextureView();
		auto HeapInfo = FirstTexView->GetHeapInfo();
		Context.SetDescriptorHeap(HeapInfo.first, HeapInfo.second);
		Context.SetDescriptorTable(0, FirstTexView->GetGpuHandle());

		Context.DrawIndexed(mesh.m_Indices.size());
	}

	//ImGUI MSAAµÄÎÊÌâ!!!!!
	m_ImGui->RenderDrawData(Context);
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

	bool ImGuiCaptureKeyboard = false;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImGuiCaptureKeyboard = io.WantCaptureKeyboard;
	}
	if (!ImGuiCaptureKeyboard)
	{
		if (GetAsyncKeyState('W') & 0x8000)
			m_Camera.Walk(5.0f * dt);

		if (GetAsyncKeyState('S') & 0x8000)
			m_Camera.Walk(-5.0f * dt);

		if (GetAsyncKeyState('A') & 0x8000)
			m_Camera.Strafe(-5.0f * dt);

		if (GetAsyncKeyState('D') & 0x8000)
			m_Camera.Strafe(5.0f * dt);
	}
	m_Camera.UpdateViewMatrix();
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
