#pragma once

#include "DataDrivenShaderPlatformInfo.h"
#include "MeshPassProcessor.h"
#include "CoreMinimal.h"
#include "MeshMaterialShader.h"

class FToonOutlineMeshPassProcessor : public FMeshPassProcessor
{
public:
	//const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext
	FToonOutlineMeshPassProcessor(
		const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1
	) override final;

private:
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode
	);

private:
	FMeshPassProcessorRenderState PassDrawRenderState;
};

class FToonOutlineVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlineVS, MeshMaterial);

public:
	/** Default constructor. */
	FToonOutlineVS() = default;
	FToonOutlineVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		// if we has params  bind them here
		OutlineWidth.Bind(Initializer.ParameterMap, TEXT("OutlineWidth"));
		//BindSceneTextureUniformBufferDependentOnShadingPath(Initializer, PassUniformBuffer, PassUniformBuffer);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}

	// You can call this function to bind every mesh personality data
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		const float OutlineWidthFromMat = Material.GetOutlineWidth();
		
		ShaderBindings.Add(OutlineWidth, OutlineWidthFromMat);
	}

	/** The parameter to use for setting the Mesh OutLine Scale. */
	LAYOUT_FIELD(FShaderParameter, OutlineWidth);
};

/**
 * Pixel shader for rendering a single, constant color.
 */
class FToonOutlinePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FToonOutlinePS, MeshMaterial);
public:

	FToonOutlinePS() = default;
	FToonOutlinePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		// if we has color bind it here
		OutlineColor.Bind(Initializer.ParameterMap, TEXT("OutlineColor"));
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Set Define in Shader. 
		//OutEnvironment.SetDefine(TEXT("Define"), Value);
	}

	// FShader interface.
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			(Parameters.VertexFactoryType->GetFName() == FName(TEXT("FLocalVertexFactory")) || 
				Parameters.VertexFactoryType->GetFName() == FName(TEXT("TGPUSkinVertexFactoryDefault")));
	}
	
	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material,  ShaderElementData, ShaderBindings);

		// Get ToonOutLine Data from Material
		const FLinearColor OutlineColorFromMat = Material.GetOutlineColor();
		FVector3f Color(OutlineColorFromMat.R, OutlineColorFromMat.G, OutlineColorFromMat.G);
		//FVector3f Color(1.0, 0.0, 0.0);
		// Bind to Shader
		ShaderBindings.Add(OutlineColor, Color);
	}
	
	/** The parameter to use for setting the Mesh OutLine Color. */
	LAYOUT_FIELD(FShaderParameter, OutlineColor);
};