#include "AccelerationStructure.h"

void AccelerationStructure::Build(std::vector<std::pair<D3D12_RAYTRACING_GEOMETRY_DESC, XMFLOAT4X4>>& GeomDesc)
{
	RayTracingContext& Context = m_pRenderDevice->BeginGraphicsContext(L"Build AccelerationStructure").GetRayTracingContext();

	std::vector<std::unique_ptr<Buffer>>scratchBuffers(GeomDesc.size() + 1);
	m_Instances.reserve(GeomDesc.size());

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	//Build Bottom	
	for (size_t i = 0; i < GeomDesc.size(); i++)
	{
		//PreBuild
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;
		inputs.NumDescs = 1;
		inputs.pGeometryDescs = &GeomDesc[i].first;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
		m_pRenderDevice->g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);
		
		std::unique_ptr<Buffer> bottomLeveLAS;
		bottomLeveLAS.reset(new Buffer(
			m_pRenderDevice,
			nullptr,
			info.ResultDataMaxSizeInBytes,
			false,
			false,
			std::wstring(std::to_wstring(i) + L" BottomLeveLAS").c_str(),
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		);
		scratchBuffers[i].reset(new Buffer(
			m_pRenderDevice,
			nullptr,
			info.ScratchDataSizeInBytes,
			false,
			false,
			std::wstring(std::to_wstring(i) + L" ScratchBuffer").c_str(),
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
		);

		//Build
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomASDesc = {};
		bottomASDesc.Inputs = inputs;
		bottomASDesc.DestAccelerationStructureData = bottomLeveLAS->GetGpuVirtualAddress();
		bottomASDesc.ScratchAccelerationStructureData = scratchBuffers[i]->GetGpuVirtualAddress();

		Context.BuildRaytracingAccelerationStructure(&bottomASDesc);
		if ((i != 0 && i % 15 == 0) || i == GeomDesc.size() - 1)
			Context.InsertUAVBarrier(bottomLeveLAS.get(), true);
		else
			Context.InsertUAVBarrier(bottomLeveLAS.get(), false);

		m_Instances.emplace_back(std::make_pair(std::move(bottomLeveLAS), GeomDesc[i].second));
	}
	
	//Build Top
	//PreBuild
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = buildFlags;
	inputs.NumDescs = GeomDesc.size();
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	m_pRenderDevice->g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	m_TopLevelAS.reset(new Buffer(
		m_pRenderDevice, 
		nullptr, 
		info.ResultDataMaxSizeInBytes, 
		false, 
		false, 
		L"TopLevelAS",
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
	);
	scratchBuffers[scratchBuffers.size()-1].reset(new Buffer(
		m_pRenderDevice, 
		nullptr, 
		info.ScratchDataSizeInBytes, 
		false, 
		false, 
		L"TopLevel ScratchBuffer",
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	);

	//The instance desc should be inside a buffer, create and map the buffer
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC>InstanceDescs(m_Instances.size());
	for (size_t i = 0; i < InstanceDescs.size(); i++)
	{
		InstanceDescs[i].InstanceID = i;
		InstanceDescs[i].InstanceContributionToHitGroupIndex = i * m_InstanceOffsetInShaderTable;
		InstanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		memcpy(InstanceDescs[i].Transform, &m_Instances[i].second, sizeof(InstanceDescs[i].Transform));
		InstanceDescs[i].AccelerationStructure = m_Instances[i].first->GetGpuVirtualAddress();
		InstanceDescs[i].InstanceMask = 0xFF;
	}
	std::unique_ptr<Buffer> pInstanceDesc;
	pInstanceDesc.reset(new Buffer(
		m_pRenderDevice, 
		InstanceDescs.data(),
		sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * InstanceDescs.size(),
		false, 
		false, 
		L"pInstanceDesc")
	);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topASDesc = {};
	inputs.InstanceDescs = pInstanceDesc->GetGpuVirtualAddress();
	topASDesc.Inputs = inputs;
	topASDesc.DestAccelerationStructureData = m_TopLevelAS->GetGpuVirtualAddress();
	topASDesc.ScratchAccelerationStructureData = scratchBuffers[scratchBuffers.size() - 1]->GetGpuVirtualAddress();

	Context.BuildRaytracingAccelerationStructure(&topASDesc);
	Context.InsertUAVBarrier(m_TopLevelAS.get(), true);
	Context.Finish(true);
}

void AccelerationStructure::UpdateMatrix(std::vector<XMFLOAT4X4>& matrixs)
{
	ASSERT(matrixs.size() == m_Instances.size(), "Bottom Need Update!");

	std::unique_ptr<Buffer> scratchBuffers;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	//Update Top
	//PreBuild
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = buildFlags;
	inputs.NumDescs = matrixs.size();
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	m_pRenderDevice->g_Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	scratchBuffers.reset(new Buffer(
		m_pRenderDevice,
		nullptr,
		info.ScratchDataSizeInBytes,
		false,
		false,
		L"TopLevel ScratchBuffer",
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	);

	//The instance desc should be inside a buffer, create and map the buffer
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC>InstanceDescs(m_Instances.size());
	for (size_t i = 0; i < InstanceDescs.size(); i++)
	{
		InstanceDescs[i].InstanceID = i;
		InstanceDescs[i].InstanceContributionToHitGroupIndex = i * m_InstanceOffsetInShaderTable;
		InstanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		memcpy(InstanceDescs[i].Transform, &matrixs[i], sizeof(InstanceDescs[i].Transform));
		InstanceDescs[i].AccelerationStructure = m_Instances[i].first->GetGpuVirtualAddress();
		InstanceDescs[i].InstanceMask = 0xFF;
	}
	std::unique_ptr<Buffer> pInstanceDesc;
	pInstanceDesc.reset(new Buffer(
		m_pRenderDevice,
		InstanceDescs.data(),
		sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * InstanceDescs.size(),
		false,
		false,
		L"pInstanceDesc")
	);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topASDesc = {};
	inputs.InstanceDescs = pInstanceDesc->GetGpuVirtualAddress();
	topASDesc.Inputs = inputs;
	topASDesc.DestAccelerationStructureData = m_TopLevelAS->GetGpuVirtualAddress();
	topASDesc.ScratchAccelerationStructureData = scratchBuffers->GetGpuVirtualAddress();
	topASDesc.SourceAccelerationStructureData = m_TopLevelAS->GetGpuVirtualAddress();

	RayTracingContext& Context = m_pRenderDevice->BeginGraphicsContext(L"Update Top AccelerationStructure Matrixs").GetRayTracingContext();
	Context.BuildRaytracingAccelerationStructure(&topASDesc);
	Context.InsertUAVBarrier(m_TopLevelAS.get(), true);
	Context.Finish(true);
}
