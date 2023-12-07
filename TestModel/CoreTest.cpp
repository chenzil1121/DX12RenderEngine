#include "CoreTest.h"
#include "Utility.h"

bool CoreTest::Initialize()
{
	if (!AppBase::Initialize())
		return false;
	CreateResources();

	//SwapChain
	m_SwapChain.reset(new SwapChain(m_hMainWnd, &m_Device, 1, 0));

	//ImGui
	m_ImGui.reset(new ImGuiWrapper(m_hMainWnd, &m_Device, SWAP_CHAIN_BUFFER_COUNT, m_SwapChain->GetBackBufferSRGBFormat()));

	//EnvMapPass
	m_EnvMapPass.reset(new EnvMap(&m_Device, m_SwapChain.get(), "../Asset/EnvMap/papermill.dds"));

	//IBL
	m_IBL.reset(new IBL(&m_Device, m_EnvMapPass->GetEnvMapView()));

	//BasePass
	m_BasePass.reset(new BasePass(&m_Device, m_SwapChain.get()));

	//GbufferPass
	m_GbufferPass.reset(new Gbuffer(&m_Device, m_SwapChain.get()));

	//DeferredLightPass
	m_DeferredLightPass.reset(new DeferredLightPass(&m_Device, m_SwapChain.get()));

#ifdef DXR
	//RayTracingPass
	m_RayTracingPass.reset(new RayTracingPass(&m_Device, m_SwapChain.get(), m_Scene.get()));

	//SVGF
	m_svgfShadowPass.reset(new SVGFPass(&m_Device, m_SwapChain.get()));
	m_svgfReflectionPass.reset(new SVGFPass(&m_Device, m_SwapChain.get(), true, 2));

	//ModulateIllumination
	m_ModulateIlluminationPass.reset(new ModulateIllumination(&m_Device, m_SwapChain.get()));
#endif

	//FXAA
	m_FXAA.reset(new FXAA(&m_Device, m_SwapChain.get()));

	//VarianceShadowMap
	m_VSM.reset(new VarianceShadowMap(
		&m_Device,
		{ -m_LightDirection[0] * 3.0f, -m_LightDirection[1] * 3.0f, -m_LightDirection[2] * 3.0f },
		{ m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] }
	));

	return true;
}

void CoreTest::CreateResources()
{
	//Camera
	XMVECTOR pos = XMVectorSet(0.0, 0.0, -1.5, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	m_Camera.LookAt(pos, target, up);

	//Bistro Camera
	/*pos = XMVectorSet(25.4722691f, 4.88068056f, 58.8330536f, 1.0f);
	target = XMVectorSet(-10.0, 0.0, 0.0, 1.0f);
	m_Camera.LookAt(pos, target, up);*/

	//Scene
	m_Scene.reset(new Scene("../Asset/GLTFModel/GLTFModel.gltf", &m_Device));
	//m_Scene.reset(new Scene("../Asset/Bistro/Bistro.gltf", &m_Device));
	//m_Scene.reset(new Scene("../Asset/Sponza_new/Sponza_new.gltf", &m_Device));

	m_Scene->AddDirectionalLight({ 10.0f, 10.0f, 10.0f }, { m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] });
	m_Scene->AddPointLight({ 5.0f, 5.0f, 5.0f }, { 0.0f, 20.0f, 3.0f }, 0.0, 1.0);
	m_MainPassCB.FirstLightIndex = XMINT3(1, 0, 0);
}

void CoreTest::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	//ObjectConstantBuffer
#ifdef DXR
	std::vector<XMFLOAT4X4>matrixs;//For RayTracing AccelerationStructure
#endif 
	int objectID = 0;
	for (size_t i = 0; i < m_Scene->m_Meshes.size(); i++)
	{
		for (size_t j = 0; j < m_Scene->m_Meshes[i].size(); j++)
		{
			Mesh& mesh = m_Scene->m_Meshes[i][j];
			MeshConstants& meshConstants = mesh.m_Constants;
			meshConstants.PreWorld = meshConstants.World;
			meshConstants.MeshID = objectID++;
			auto meshWorld = XMLoadFloat4x4(&mesh.GetWorldMatrix());
			//Rotation
			auto mat4 = mat4_cast(m_SceneRotation);
			auto SceneRotation = XMLoadFloat4x4(&XMFLOAT4X4(mat4.m00, mat4.m01, mat4.m02, mat4.m03,
				mat4.m10, mat4.m11, mat4.m12, mat4.m13,
				mat4.m20, mat4.m21, mat4.m22, mat4.m23,
				mat4.m30, mat4.m31, mat4.m32, mat4.m33));
			//Scale
			//auto SceneScale = XMMatrixScaling(100.0f, 100.0f, 100.0f);
			//auto Mat = meshWorld * SceneRotation * SceneScale;
			auto Mat = meshWorld * SceneRotation;;
			auto MatInvertTran = MathHelper::InverseTranspose(Mat);

			XMStoreFloat4x4(&meshConstants.World, XMMatrixTranspose(Mat));
			XMStoreFloat4x4(&meshConstants.WorldInvertTran, XMMatrixTranspose(MatInvertTran));

			mesh.UploadConstantsBuffer(&m_Device, &meshConstants);
			
#ifdef DXR
			if (i == (size_t)LayerType::Opaque)
				matrixs.push_back(meshConstants.World);
#endif
		}
	}

#ifdef DXR
	m_RayTracingPass->UpdateTASMatrixs(matrixs);
#endif

	//PassConstantBuffer
	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();
	XMFLOAT3 pos = m_Camera.GetPosition3f();

	m_MainPassCB.PreViewProj = m_MainPassCB.ViewProj;
	XMStoreFloat4x4(&m_MainPassCB.ViewProj, XMMatrixTranspose(view * proj));
	XMStoreFloat4x4(&m_MainPassCB.ViewProjInvert, XMMatrixTranspose(MathHelper::Inverse(view * proj)));
	XMStoreFloat4x4(&m_MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_MainPassCB.Proj, XMMatrixTranspose(proj));
	
	m_MainPassCB.CameraPos = pos;
	m_MainPassCB.NearZ = m_Camera.GetNearZ();
	m_MainPassCB.FarZ = m_Camera.GetFarZ();
	m_MainPassCB.FrameDimension = { (int)m_SwapChain->GetBackBufferDesc().Width , (int)m_SwapChain->GetBackBufferDesc().Height };
	m_MainPassCB.PrefilteredEnvMipLevels = 9;

	m_PassConstantBuffer.reset(new Buffer(&m_Device, nullptr, (UINT)(sizeof(PassConstants)), true, true));
	m_PassConstantBuffer->Upload(&m_MainPassCB);

	//LightsBuffer
	m_Scene->m_Lights[0].Direction = { m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] };
	m_Scene->m_LightBuffers.reset(new Buffer(&m_Device, nullptr, m_Scene->m_Lights.size() * sizeof(Light), true, true));
	m_Scene->m_LightBuffers->Upload(m_Scene->m_Lights.data());

	//VarianceShadowMap
	m_VSM->UpdateLightVP(
		{ -m_LightDirection[0] * 3.0f,-m_LightDirection[1] * 3.0f,-m_LightDirection[2] * 3.0f },
		{ m_LightDirection[0], m_LightDirection[1], m_LightDirection[2] }
	);
	
	//IMGUI
	UpdateUI();
}

void CoreTest::UpdateUI()
{
	m_ImGui->NewFrame();
	
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::Button("Load Scene"))
		{
			;
		}
		ImGui::gizmo3D("Scene Rotation", m_SceneRotation, ImGui::GetTextLineHeight() * 10);
		ImGui::SameLine();
		ImGui::gizmo3D("Light direction", m_LightDirection, ImGui::GetTextLineHeight() * 10);
	}
	ImGui::End();
	ImGui::Render();
}

void CoreTest::Render(const GameTimer& gt)
{
	GraphicsContext& Context = m_Device.BeginGraphicsContext(L"Render");

#ifdef FOWARD
	//VarianceShadowMap
	m_VSM->Render(Context, m_Scene.get());

	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetRenderTargets(1, &m_SwapChain->GetBackBufferView(), &m_SwapChain->GetDepthBufferView());
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(m_SwapChain->GetBackBufferView(), DirectX::Colors::LightSlateGray, nullptr);
	Context.ClearDepthAndStencil(m_SwapChain->GetDepthBufferView(), 1.0f, 0);

	//EnvMap
	m_EnvMapPass->Render(Context, m_PassConstantBuffer.get());

	//BasePassOpaque
	m_BasePass->Render(Context, m_PassConstantBuffer.get(), m_Scene.get(), m_IBL.get(), m_VSM.get(), true);
	//BasePassBlend
	m_BasePass->Render(Context, m_PassConstantBuffer.get(), m_Scene.get(), m_IBL.get(), m_VSM.get(), false);

	//FXAA
	m_FXAA->Render(Context);

	m_ImGui->RenderDrawData(Context);
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);

	Context.Finish(true);
#endif // FOWARD

#ifdef DEFFER
	//VarianceShadowMap
	m_VSM->Render(Context, m_Scene.get());

	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetRenderTargets(1, &m_SwapChain->GetBackBufferView(), &m_SwapChain->GetDepthBufferView());
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(m_SwapChain->GetBackBufferView(), DirectX::Colors::LightSlateGray, nullptr);
	Context.ClearDepthAndStencil(m_SwapChain->GetDepthBufferView(), 1.0f, 0);

	//GbufferPass
	m_GbufferPass->Render(Context, m_PassConstantBuffer.get(), m_Scene.get());

	//DeferredLight
	m_DeferredLightPass->Render(Context, m_PassConstantBuffer.get(), m_GbufferPass->GetGbufferSRV(), m_IBL->GetIBLView(), m_VSM.get(), m_Scene.get());

	//EnvMap
	m_EnvMapPass->Render(Context, m_PassConstantBuffer.get());

	//BasePassBlend
	m_BasePass->Render(Context, m_PassConstantBuffer.get(), m_Scene.get(), m_IBL.get(), m_VSM.get(), false);

	//FXAA
	m_FXAA->Render(Context);

	m_ImGui->RenderDrawData(Context);
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);

	Context.Finish(true);
#endif // DEFFER

#ifdef DXR
	Context.SetViewportAndScissor(m_viewport, m_scissor);
	Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context.SetRenderTargets(1, &m_SwapChain->GetBackBufferView(), &m_SwapChain->GetDepthBufferView());
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	Context.ClearRenderTarget(m_SwapChain->GetBackBufferView(), DirectX::Colors::LightSlateGray, nullptr);
	Context.ClearDepthAndStencil(m_SwapChain->GetDepthBufferView(), 1.0f, 0);

	//GbufferPass
	m_GbufferPass->Render(Context, m_PassConstantBuffer.get(), m_Scene.get());

	//RayTracingPass
	m_RayTracingPass->DispatchRays(Context.GetRayTracingContext(), m_Camera.GetPosition3f(), m_Scene->m_Lights[1], m_EnvMapPass->GetEnvMapView(), m_GbufferPass->GetGbufferSRV());

	//SVGF
	m_svgfShadowPass->Compute(Context.GetComputeContext(), m_GbufferPass->GetGbufferSRV(), m_GbufferPass->GetPreSRV(), m_RayTracingPass->GetShadowOutputSRV());
	m_svgfReflectionPass->Compute(Context.GetComputeContext(), m_GbufferPass->GetGbufferSRV(), m_GbufferPass->GetPreSRV(), m_RayTracingPass->GetReflectOutputSRV());

	//ModulateIllumination
	m_ModulateIlluminationPass->Render(Context, m_PassConstantBuffer.get(), m_Scene->m_Lights[1], m_GbufferPass->GetGbufferSRV(), m_svgfShadowPass->GetFilteredSRV(), m_svgfReflectionPass->GetFilteredSRV());

	//EnvMap
	m_EnvMapPass->Render(Context, m_PassConstantBuffer.get());

	m_ImGui->RenderDrawData(Context);
	Context.TransitionResource(m_SwapChain->GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);

	Context.Finish(true);
#endif // DXR

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
	
	m_FXAA->Resize(m_Device.g_DisplayWidth, m_Device.g_DisplayHeight, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight);
#if (defined DEFFER)||(defined DXR)
	m_GbufferPass->Resize(m_Device.g_DisplayWidth, m_Device.g_DisplayHeight, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight);
#endif

#ifdef DXR
	m_RayTracingPass->Resize(m_Device.g_DisplayWidth, m_Device.g_DisplayHeight, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight);

	m_svgfShadowPass->Resize(m_Device.g_DisplayWidth, m_Device.g_DisplayHeight, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight);
	m_svgfReflectionPass->Resize(m_Device.g_DisplayWidth, m_Device.g_DisplayHeight, (FLOAT)m_Device.g_DisplayWidth / (FLOAT)m_Device.g_DisplayHeight);
#endif
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


	CoreTest theApp(hInstance, 1920, 1080);
	if (!theApp.Initialize())
		return 0;

	return theApp.Run();
}
