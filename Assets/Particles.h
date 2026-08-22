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
			false,
			1.0f, 1.0f,
			ParticleMove{ 10.0f, 10.0f, ParticleChangeMode::SMALLER, 2.0f},
			ParticleRotation{{false, false, false}, 15.0f, ParticleChangeMode::SMALLER, 1.0f },
			ParticleSize{2.0f, 2.0f, ParticleChangeMode::NOT_CHANGE, 1.0f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::CIRCLE,
				ParticleEmissionModeType::LOOP,
				5.f, 5.f, 10.f
			),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				false, {MathHelper::Pi * 0.5f, 0.0f, 0.0f},
				0),
			ParticleEmissionCount(2, 30, 0.01f)
		)},
		{"snow_slash_slash", ParticleProperties(
			false,
			1.0f, 1.0f,
			ParticleMove{ 0.0f, 0.0f, ParticleChangeMode::NOT_CHANGE, 1.0f},
			ParticleRotation{{false, false, true}, 15.0f, ParticleChangeMode::SMALLER, 1.0f },
			ParticleSize{8.0f, 8.0f, ParticleChangeMode::NOT_CHANGE, 1.0f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::CIRCLE,
				ParticleEmissionModeType::LOOP,
				0.f, 0.f, 0.f
			),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				true, {MathHelper::Pi * 0.5f, 0.0f, 0.0f},
				1),
			ParticleEmissionCount(1, 1, 0.0f)
		)}
	};

	const ParticleMaterial ParticleMaterials[] = {
		ParticleMaterial{
			{0.8f, 0.8f, 1.0f, 0.8f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 0
		},
		ParticleMaterial{
			{0.8f, 0.8f, 1.0f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 1
		},
	};
};
