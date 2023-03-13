#pragma once
#include"CommandListManager.h"
#include"GpuResource.h"
#include"PipelineState.h"
#include"RootSignature.h"
#include"DescriptorHeap.h"

class RenderDevice;
class GraphicsContext;
class ComputeContext;
	 
class CommandContext
{
	friend class ContextManager;
public:
	CommandContext(D3D12_COMMAND_LIST_TYPE Type, RenderDevice* Device);
	~CommandContext();
	void Reset();
	void Initialize();
	uint64_t Finish(bool WaitForCompletion = false);

	GraphicsContext& GetGraphicsContext() {
		ASSERT(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
		return reinterpret_cast<GraphicsContext&>(*this);
	}

	ComputeContext& GetComputeContext() {
		return reinterpret_cast<ComputeContext&>(*this);
	}

	//采用split Barrier
	void TransitionResource(GpuResource* Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	void BeginResourceTransition(GpuResource* Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	inline void FlushResourceBarriers();

	void SetPipelineState(const PSO& PSO);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);

	void CopyResource(GpuResource* Dest, GpuResource* Src);

	ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList; }

protected:
	RenderDevice* m_pRenderDevice;
	ID3D12GraphicsCommandList* m_CommandList;
	ID3D12CommandAllocator* m_CurrentAllocator;

	ID3D12RootSignature* m_CurGraphicsRootSignature;
	ID3D12RootSignature* m_CurComputeRootSignature;
	ID3D12PipelineState* m_CurPipelineState;

	DynamicSuballocationsManager m_DynamicViewDescriptorHeap;
	DynamicSuballocationsManager m_DynamicSamplerDescriptorHeap;

	D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
	UINT m_NumBarriersToFlush;

	ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	std::wstring m_ID;
	void SetID(const std::wstring& ID) { m_ID = ID; }

	D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsContext : public CommandContext
{
public:
	void ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, const float* Colour, D3D12_RECT* Rect);
	void ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE DSV, float ClearDepthValue, UINT8 ClearStencilValue);

	void SetRootSignature(const RootSignature& RootSig);
	void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE* DSV = nullptr);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);
	void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
	void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
	void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
	void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
	void SetBufferSRV(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS SRV);
	void SetBufferUAV(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS UAV);
	void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);
	void Draw(UINT VertexCount, UINT VertexStartOffset = 0);
	void DrawIndexed(UINT IndexCount, UINT StartIndexLocation = 0, INT BaseVertexLocation = 0);
	void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
		UINT StartVertexLocation = 0, UINT StartInstanceLocation = 0);
	void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
		INT BaseVertexLocation, UINT StartInstanceLocation);
	void ResolveSubresource(GpuResource* pDstResource, UINT DstSubresource, GpuResource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format);
};

class ComputeContext :public CommandContext
{
public:
};

/*
* inline函数需定义在头文件中，因为编译时期展开内联需要获取函数定义
*/

inline void CommandContext::FlushResourceBarriers()
{
	if (m_NumBarriersToFlush > 0)
	{
		m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
		m_NumBarriersToFlush = 0;
	}
}

inline void CommandContext::SetPipelineState(const PSO& PSO)
{
	ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
	if (PipelineState == m_CurPipelineState)
		return;

	m_CommandList->SetPipelineState(PipelineState);
	m_CurPipelineState = PipelineState;
}

inline void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr)
{
	if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
	{
		m_CurrentDescriptorHeaps[Type] = HeapPtr;
		m_CommandList->SetDescriptorHeaps(1, &HeapPtr);
	}
}

inline void CommandContext::CopyResource(GpuResource* Dest, GpuResource* Src)
{
	FlushResourceBarriers();
	m_CommandList->CopyResource(Dest->GetResource(), Src->GetResource());
}

inline void GraphicsContext::ClearRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, const float *Colour, D3D12_RECT* Rect)
{
	FlushResourceBarriers();
	m_CommandList->ClearRenderTargetView(RTV, Colour, (Rect == nullptr) ? 0 : 1, Rect);
}

inline void GraphicsContext::ClearDepthAndStencil(D3D12_CPU_DESCRIPTOR_HANDLE DSV, float ClearDepthValue, UINT8 ClearStencilValue)
{
	FlushResourceBarriers();
	m_CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, ClearDepthValue, ClearStencilValue, 0, nullptr);

}

inline void GraphicsContext::SetRootSignature(const RootSignature& RootSig)
{
	if (RootSig.GetSignature() == m_CurGraphicsRootSignature)
		return;

	m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature = RootSig.GetSignature());
}

inline void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE* DSV)
{
	m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, DSV);
}

inline void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
{
	ASSERT(rect.left < rect.right&& rect.top < rect.bottom);
	m_CommandList->RSSetViewports(1, &vp);
	m_CommandList->RSSetScissorRects(1, &rect);
}

inline void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
{
	m_CommandList->IASetPrimitiveTopology(Topology);
}

inline void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView)
{
	m_CommandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView)
{
	m_CommandList->IASetVertexBuffers(Slot, 1, &VBView);
}

inline void GraphicsContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
{
	m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, CBV);
}

inline void GraphicsContext::SetBufferSRV(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS SRV)
{
	m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV);
}

inline void GraphicsContext::SetBufferUAV(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS UAV)
{
	m_CommandList->SetGraphicsRootUnorderedAccessView(RootIndex, UAV);
}

inline void GraphicsContext::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
{
	m_CommandList->SetGraphicsRootDescriptorTable(RootIndex, FirstHandle);
}

inline void GraphicsContext::Draw(UINT VertexCount, UINT VertexStartOffset)
{
	DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
}

inline void GraphicsContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}

inline void GraphicsContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
	UINT StartVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_CommandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

inline void GraphicsContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
	INT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_CommandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

inline void GraphicsContext::ResolveSubresource(GpuResource* pDstResource, UINT DstSubresource, GpuResource* pSrcResource,
	UINT SrcSubresource, DXGI_FORMAT Format)
{
	FlushResourceBarriers();
	m_CommandList->ResolveSubresource(pDstResource->GetResource(), DstSubresource, pSrcResource->GetResource(), SrcSubresource, Format);
}

//注意声明顺序，CommandContext得先声明
class ContextManager
{
public:
	ContextManager(RenderDevice* Device):m_pRenderDevice(Device) {}

	CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
	void FreeContext(CommandContext* UsedContext);
	void DestroyAllContexts();

private:
	RenderDevice* m_pRenderDevice;
	std::vector<std::unique_ptr<CommandContext>> m_ContextPool[4];
	std::queue<CommandContext*> m_AvailableContexts[4];
};