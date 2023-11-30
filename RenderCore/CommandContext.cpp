#include"CommandContext.h"
#include"RenderDevice.h"
#include"RayTracingShaderTable.h"

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	auto& AvailableContexts = m_AvailableContexts[Type];

	CommandContext* ret = nullptr;
	if (AvailableContexts.empty())
	{
		ret = new CommandContext(Type, m_pRenderDevice);
		m_ContextPool[Type].emplace_back(ret);
		ret->Initialize();
	}
	else
	{
		ret = AvailableContexts.front();
		AvailableContexts.pop();
		ret->Reset();
	}
	ASSERT(ret != nullptr);
	ASSERT(ret->m_Type == Type);
	return ret;
}

void ContextManager::FreeContext(CommandContext* UsedContext)
{
	ASSERT(UsedContext != nullptr);
	m_AvailableContexts[UsedContext->m_Type].push(UsedContext);
}

void ContextManager::DestroyAllContexts()
{
	for (uint32_t i = 0; i < 4; ++i)
		m_ContextPool[i].clear();
}

CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type, RenderDevice* pDevice):
	m_Type(Type),
	m_pRenderDevice(pDevice),
	m_DynamicViewDescriptorHeap(pDevice->g_GPUDescriptorHeap[0], Utility::DynamicDescriptorAllocationChunkSize[0]),
	m_DynamicSamplerDescriptorHeap(pDevice->g_GPUDescriptorHeap[1], Utility::DynamicDescriptorAllocationChunkSize[1])
{
	m_CommandList = nullptr;
	m_CurrentAllocator = nullptr;
	ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));

	m_CurGraphicsRootSignature = nullptr;
	m_CurComputeRootSignature = nullptr;
	m_CurPipelineState = nullptr;
	m_NumBarriersToFlush = 0;
}

CommandContext::~CommandContext()
{
	if (m_CommandList != nullptr)
		m_CommandList->Release();
}

void CommandContext::Reset()
{
	/*
	* 每一个Context独有一个CommandList，但可以关联到不同的CommandAllocator，由于指令实际分配在CommandAllocator上
	* 在GPU执行完成前不能Reset CommandAllocator，CommandAllocatorPool会自动管理CommandAllocator的Reset
	*/
	ASSERT(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
	m_CurrentAllocator = m_pRenderDevice->g_CommandManager.GetQueue(m_Type).RequestAllocator();
	m_CommandList->Reset(m_CurrentAllocator, nullptr);

	m_CurGraphicsRootSignature = nullptr;
	m_CurComputeRootSignature = nullptr;
	m_CurPipelineState = nullptr;
	ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));
	m_NumBarriersToFlush = 0;

}

void CommandContext::Initialize()
{
	m_pRenderDevice->g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
}

uint64_t CommandContext::Finish(bool WaitForCompletion)
{
	FlushResourceBarriers();

	CommandQueue& Queue = m_pRenderDevice->g_CommandManager.GetQueue(m_Type);

	uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
	//这里把使用过的Allocator加入m_ReadyAllocators队列，并不会重置Allocator
	//而是在RequestAllocator时候根据FenceValue来判断Allocator是否使用完毕来Reset
	Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
	m_CurrentAllocator = nullptr;
	m_pRenderDevice->g_DynamicUploadHeap.FinishFrame(FenceValue, Queue.GetLastCompletedFenceValue());

	//并不是每一个CommandContext都需要同步，比如绘制不同材质的物体，可能需要新的CommandContext但不需要等待上一次CommandContext执行完毕
	if (WaitForCompletion)
		Queue.WaitForFence(FenceValue);

	m_pRenderDevice->g_ContextManager.FreeContext(this);

	return FenceValue;
}

void CommandContext::TransitionResource(GpuResource* Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
	D3D12_RESOURCE_STATES OldState = Resource->m_UsageState;

	if (OldState != NewState)
	{
		ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = Resource->GetResource();
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;
		if (NewState == Resource->m_TransitioningState)
		{
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
			Resource->m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
		}
		else
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Resource->m_UsageState = NewState;
	}
	if (FlushImmediate || m_NumBarriersToFlush == 16)
		FlushResourceBarriers();

}

void CommandContext::BeginResourceTransition(GpuResource* Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
	if (Resource->m_TransitioningState != (D3D12_RESOURCE_STATES)-1)
	{
		TransitionResource(Resource, Resource->m_TransitioningState, FlushImmediate);
	}
	D3D12_RESOURCE_STATES OldState = Resource->m_UsageState;

	if (OldState != NewState)
	{
		ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Transition.pResource = Resource->GetResource();
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;

		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

		Resource->m_TransitioningState = NewState;
	}

	if (FlushImmediate || m_NumBarriersToFlush == 16)
		FlushResourceBarriers();
}

void CommandContext::InsertUAVBarrier(GpuResource* Resource, bool FlushImmediate)
{
	ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
	D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];
	
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.UAV.pResource = Resource->GetResource();

	if (FlushImmediate)
		FlushResourceBarriers();
}

void RayTracingContext::DispatchRays(ShaderTable* RayGenerationShaderRecord, ShaderTable* MissShaderTable, ShaderTable* HitGroupTable, UINT Width, UINT Height, UINT Depth, ShaderTable* CallableShaderTable)
{
	FlushResourceBarriers();
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.RayGenerationShaderRecord.StartAddress = RayGenerationShaderRecord->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = RayGenerationShaderRecord->GetSizeInBytes();
	dispatchDesc.MissShaderTable.StartAddress = MissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = MissShaderTable->GetSizeInBytes();
	dispatchDesc.MissShaderTable.StrideInBytes = MissShaderTable->GetShaderRecordSize();
	dispatchDesc.HitGroupTable.StartAddress = HitGroupTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = HitGroupTable->GetSizeInBytes();
	dispatchDesc.HitGroupTable.StrideInBytes = HitGroupTable->GetShaderRecordSize();
	if (CallableShaderTable)
	{
		dispatchDesc.CallableShaderTable.StartAddress = CallableShaderTable->GetGPUVirtualAddress();
		dispatchDesc.CallableShaderTable.SizeInBytes = CallableShaderTable->GetSizeInBytes();
		dispatchDesc.CallableShaderTable.StrideInBytes = CallableShaderTable->GetShaderRecordSize();
	}
	dispatchDesc.Width = Width;
	dispatchDesc.Height = Height;
	dispatchDesc.Depth = Depth;
	m_CommandList->DispatchRays(&dispatchDesc);
}
