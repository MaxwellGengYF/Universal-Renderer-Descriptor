#pragma once
#include <Common/Common.h>

static vstd::string_view ShaderTypes[] = {
	""_sv,
	"Vertex"_sv,
	"Fragment"_sv,
	"Geometry"_sv,
	"Hull"_sv,
	"Domain"_sv,
	"Surface"_sv,
	"RayTracing"_sv};

static vstd::string_view PassTypes[] = {
	"Normal"_sv,
	"Vertex"_sv,
	"VertexLM"_sv,
	"VertexLMRGBM"_sv,
	"ForwardBase"_sv,
	"ForwardAdd"_sv,
	"LightPrePassBase"_sv,
	"LightPrePassFinal"_sv,
	"ShadowCaster"_sv,
	""_sv,
	"Deferred"_sv,
	"Meta"_sv,
	"MotionVectors"_sv,
	"ScriptableRenderPipeline"_sv,
	"ScriptableRenderPipelineDefaultUnlit"_sv};

