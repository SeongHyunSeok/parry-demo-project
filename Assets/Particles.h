#pragma once

#include <unordered_map>
#include "../Core/Particle.h"
#include "../Core/FrameResource.h"

namespace ParticleAssets
{
	const std::unordered_map<std::string, std::wstring> PartileTexureFiles = {
		{"snowflake", L"Textures/Particles/Snowflake.dds"},
		{"slash", L"Textures/Particles/Slash.dds"}
	};

	const std::unordered_map<std::string, ParticleProperties> ParticleProps =
	{
		{"snow_slash_snowflake", ParticleProperties(
			false, 0.0f,
			{1.0f, 1.0f, 0.0f},
			{0.0f, 0.0f, 0.0f},
			{1.0f, 1.0f, 1.0f, 1.0f},
			3.0f,
			ParticleEmissionShape(ParticleEmissionShapeType::CIRCLE),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				false, 0),
			ParticleEmissionCount(1, 5, 0.1f)
		)}
	};

	const ParticleMaterial ParticleMaterials[] = {
		ParticleMaterial{
			{0.7f, 0.7f, 1.0f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 0
		},
		ParticleMaterial{
			{0.5f, 0.5f, 1.0f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 1
		},
	};
};
