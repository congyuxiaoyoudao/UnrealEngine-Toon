#include "ToonOutlinePassRendering.h"

#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"
#include "SimpleMeshDrawCommandPass.h"
#include "ProfilingDebugging/CsvProfiler.h"

//IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(BackfaceOutlinePipeline, FToonOutlineVS, FToonOutlinePS, true);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlineVS, TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlinePS, TEXT("/Engine/Private/ToonOutLine.usf"), TEXT("MainPS"), SF_Pixel);

FToonOutlineMeshPassProcessor::FToonOutlineMeshPassProcessor(
	const FScene* Scene, 
	const FSceneView* InViewIfDynamicMeshCommand, 
	const FMeshPassProcessorRenderState& InPassDrawRenderState, 
	FMeshPassDrawListContext* InDrawListContext)
	:FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext),
	PassDrawRenderState(InPassDrawRenderState)
{
	if (PassDrawRenderState.GetDepthStencilState() == nullptr)
	{
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>().GetRHI());
	}
	if (PassDrawRenderState.GetBlendState() == nullptr)
	{
		PassDrawRenderState.SetBlendState(TStaticBlendState<>().GetRHI());
	}
}

void FToonOutlineMeshPassProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);

	if (Material != nullptr && Material->GetRenderingThreadShaderMap())
	{
		const FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
		// Only Toon shading model and enable render toon outline can render this pass
		if (ShadingModels.HasShadingModel(MSM_Toon) && Material->RenderToonOutline())
		{
			const EBlendMode BlendMode = Material->GetBlendMode();

			bool bResult = true;
			if (BlendMode == BLEND_Opaque)
			{
				Process(
					MeshBatch,
					BatchElementMask,
					StaticMeshId,
					PrimitiveSceneProxy,
					*MaterialRenderProxy,
					*Material,
					FM_Solid,
					CM_CCW); //cull back
			}
		}
	}
}

bool FToonOutlineMeshPassProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<FToonOutlineVS, FToonOutlinePS> ToonOutlinePassShader;
	{
		FMaterialShaderTypes ShaderTypes;
		// 指定使用的shader
		ShaderTypes.AddShaderType<FToonOutlineVS>();
		ShaderTypes.AddShaderType<FToonOutlinePS>();

		const FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

		FMaterialShaders Shaders;
		if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
		{
			UE_LOG(LogShaders, Warning, TEXT("Shader Not Found!"));
			return false;
		}

		Shaders.TryGetVertexShader(ToonOutlinePassShader.VertexShader);
		Shaders.TryGetPixelShader(ToonOutlinePassShader.PixelShader);
	}


	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(ToonOutlinePassShader.VertexShader, ToonOutlinePassShader.PixelShader);
	//PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>().GetRHI());

	//FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	PassDrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,// Enable DepthTest, It reverse about OpenGL(which is less)
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,// enable stencil test when cull back
		0x00,// disable stencil read
		0x00>// disable stencil write
		::GetRHI());
	PassDrawRenderState.SetStencilRef(0);
	
	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		ToonOutlinePassShader,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);

	return true;
}

//
// Register Pass to Global Manager
void SetupToonOutLinePassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	//DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>::GetRHI());
}

FMeshPassProcessor* CreateToonOutLinePassProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext
)
{
	FMeshPassProcessorRenderState ToonOutLinePassState;
	SetupToonOutLinePassState(ToonOutLinePassState);

	return new FToonOutlineMeshPassProcessor(
		Scene,
		InViewIfDynamicMeshCommand,
		ToonOutLinePassState,
		InDrawListContext
	);
}

FRegisterPassProcessorCreateFunction RegisterToonOutLineMeshPass(
	&CreateToonOutLinePassProcessor,
	EShadingPath::Deferred,
	EMeshPass::ToonOutlinePass,
	EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView
);


// FInt32Range GetDynamicMeshElementRange(const FViewInfo& View, uint32 PrimitiveIndex)
// {
// 	int32 Start = 0;	// inclusive
// 	int32 AfterEnd = 0;	// exclusive
//
// 	// DynamicMeshEndIndices contains valid values only for visible primitives with bDynamicRelevance.
// 	if (View.PrimitiveVisibilityMap[PrimitiveIndex])
// 	{
// 		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];
// 		if (ViewRelevance.bDynamicRelevance)
// 		{
// 			Start = (PrimitiveIndex == 0) ? 0 : View.DynamicMeshEndIndices[PrimitiveIndex - 1];
// 			AfterEnd = View.DynamicMeshEndIndices[PrimitiveIndex];
// 		}
// 	}
//
// 	return FInt32Range(Start, AfterEnd);
// }
DECLARE_STATS_GROUP(TEXT("ParallelCommandListMarkers"), STATGROUP_ParallelCommandListMarkers, STATCAT_Advanced); 
DECLARE_CYCLE_STAT(TEXT("ToonOutlinePass"), STAT_CLP_ToonOutlinePass, STATGROUP_ParallelCommandListMarkers);

BEGIN_SHADER_PARAMETER_STRUCT(FToonOutlineMeshPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FToonOutlineMeshPassParameters* GetOutlinePassParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSceneTextures& SceneTextures)
{
	FToonOutlineMeshPassParameters* PassParameters = GraphBuilder.AllocParameters<FToonOutlineMeshPassParameters>();
	PassParameters->View = View.ViewUniformBuffer;

	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	return PassParameters;
}

/**
 * Render()
 * 
 */
void FDeferredShadingSceneRenderer::RenderToonOutlinePass(FRDGBuilder& GraphBuilder, FSceneTextures& SceneTextures)
{
	RDG_EVENT_SCOPE(GraphBuilder, "ToonOutlinePass");
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderToonOutlinePass);

	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderToonOutlinePass, FColor::Emerald);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		const bool bShouldRenderView = View.ShouldRenderView();
		if(bShouldRenderView)
		{
			FToonOutlineMeshPassParameters* PassParameters = GetOutlinePassParameters(GraphBuilder, View, SceneTextures);

			View.ParallelMeshDrawCommandPasses[EMeshPass::ToonOutlinePass].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

			// GraphBuilder.AddPass(
			// 	RDG_EVENT_NAME("ToonOutlinePass"),
			// 	PassParameters,
			// 	ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
			// 	[this, &View, PassParameters](const FRDGPass* InPass, FRHICommandListImmediate& RHICmdList)
			// {
			// 	FRDGParallelCommandListSet ParallelCommandListSet(InPass, RHICmdList, GET_STATID(STAT_CLP_ToonOutlinePass), View, FParallelCommandListBindings(PassParameters));
			// 	ParallelCommandListSet.SetHighPriority();
			// 	View.ParallelMeshDrawCommandPasses[EMeshPass::ToonOutlinePass].DispatchDraw(&ParallelCommandListSet, RHICmdList, &PassParameters->InstanceCullingDrawParams);
			// });
			GraphBuilder.AddDispatchPass(
				RDG_EVENT_NAME("ToonOutlinePass"),
				PassParameters,
				ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
				[&View, PassParameters](FRDGDispatchPassBuilder& DispatchPassBuilder)
			{		
				View.ParallelMeshDrawCommandPasses[EMeshPass::ToonOutlinePass].Dispatch(DispatchPassBuilder, &PassParameters->InstanceCullingDrawParams);
			});
		}
	}
}