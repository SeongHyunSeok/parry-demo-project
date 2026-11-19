#pragma once

#include <array>
#include <unordered_map>
#include "../Core/Particle.h"
#include "../Core/FrameResource.h"

namespace ParticleAssets
{
	// The array order is the texture descriptor index used by ParticleMaterial.
	const std::array<std::pair<std::string, std::wstring>, 5> PartileTexureFiles = {{
		{"snowflake", L"Textures/Particles/Snowflake.dds"},
		{"slash", L"Textures/Particles/Slash.dds"},
		{"effect", L"Textures/Particles/effect.dds"},
		{"parry_trace", L"Textures/Particles/trace.dds"},
		{"parry_flare", L"Textures/Particles/flare.dds"}
	}};

	const std::unordered_map<std::string, ParticleProperties> ParticleProps =
	{
		{"snow_slash_snowflake", ParticleProperties(
			false,
			1.0f, 1.0f,
			ParticleMove{ 10.0f, 10.0f, ParticleChangeMode::SMALLER, 2.0f},
			ParticleRotation{{false, false, false}, 15.0f, ParticleChangeMode::SMALLER, 1.0f },
			ParticleSize{0.5f, 0.5f, ParticleChangeMode::NOT_CHANGE, 1.0f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::SPHERE,
				ParticleEmissionModeType::RANDOM,
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
		)},
		{"effect_parry", ParticleProperties(
			false,
			1.0f, 1.0f,
			ParticleMove{ 0.0f, 0.0f, ParticleChangeMode::BIGGER, 2.0f},
			ParticleRotation{{false, false, false}, 15.0f, ParticleChangeMode::BIGGER, 1.0f },
			ParticleSize{5.f, 5.f, ParticleChangeMode::NOT_CHANGE, 1.0f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::SPHERE,
				ParticleEmissionModeType::RANDOM,
				0.f, 5.f, 10.f
			),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				false, {MathHelper::Pi * 0.5f, 0.0f, 0.0f},
				2),
			ParticleEmissionCount(1, 1, 0.01f)
		)},
		{"effect_parry_sparks", ParticleProperties(
			false,
			0.3f, 0.6f,
			ParticleMove{ 8.0f, 18.0f, ParticleChangeMode::SMALLER, 1.2f },
			ParticleRotation{{false, false, false}, 0.0f, ParticleChangeMode::NOT_CHANGE, 0.0f },
			ParticleSize{0.9f, 1.8f, ParticleChangeMode::SMALLER, 0.12f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::SPHERE,
				ParticleEmissionModeType::RANDOM,
				0.0f, 0.12f, 0.0f
			),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				false, {0.0f, 0.0f, 0.0f},
				3,
				true,
				true,
				true),
			ParticleEmissionCount(32, 1, 0.0f)
		)},
		{"effect_parry_flash", ParticleProperties(
			false,
			0.35f, 0.55f,
			ParticleMove{ 0.0f, 0.0f, ParticleChangeMode::NOT_CHANGE, 0.0f },
			ParticleRotation{{false, false, false}, 0.0f, ParticleChangeMode::NOT_CHANGE, 0.0f },
			ParticleSize{6.5f, 8.0f, ParticleChangeMode::NOT_CHANGE, 0.0f },
			{1.0f, 1.0f, 1.0f, 1.0f},
			ParticleEmissionShape(
				ParticleEmissionShapeType::SPHERE,
				ParticleEmissionModeType::RANDOM,
				0.0f, 0.0f, 0.0f
			),
			ParticeRenderer(
				ParticleRenderMode::BILLBOARD,
				false, {0.0f, 0.0f, 0.0f},
				4,
				true,
				false,
				true),
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
		ParticleMaterial{
			{1.0f, 0.25f, 0.01f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 2
		},
		ParticleMaterial{
			{3.5f, 0.48f, 0.015f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 3
		},
		ParticleMaterial{
			{4.0f, 1.35f, 0.12f, 1.0f},
			{ 0.01f, 0.01f, 0.01f },
			0.0f, 4
		}
	};
};
