#pragma once

#include "d3dUtil.h"

struct Particle
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 size;
	DirectX::XMFLOAT3 up;
	DirectX::XMFLOAT3 look;
	DirectX::XMFLOAT4 color;
	UINT materialIndex;
	float timePos = 0.0f;
	bool isFinished = false;
};

struct ShaderParticle
{
	DirectX::XMFLOAT3 PosW;
	DirectX::XMFLOAT3 SizeW;
	DirectX::XMFLOAT3 UpW;
	DirectX::XMFLOAT3 LookW;
	DirectX::XMFLOAT4 Color;
	UINT MaterialIndex;
};


enum class ParticleEmissionShapeType
{
	CIRCLE,
	SPHERE,
	HEMISPHERE
};

enum class ParticleEmissionModeType
{
	RANDOM,
	LOOP
};

enum class ParticleRenderMode
{
	BILLBOARD,
	MESH
};

struct ParticleEmissionCount
{
	UINT count;
	UINT cycle;
	float interval;
};

struct ParticleMaterial
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.5f;

	UINT DiffuseMapIndex = 0;
};

struct ParticeRenderer
{
	ParticleRenderMode mode;
	bool localAlignment;
	UINT materialIndex;
};

struct ParticleEmissionShape
{
	ParticleEmissionShapeType shape = ParticleEmissionShapeType::CIRCLE;
	float radius = 0.0f;
	ParticleEmissionModeType mode = ParticleEmissionModeType::RANDOM;
};


struct ParticleProperties
{
	bool looping = false;
	bool startSpeed = 0.0f;
	DirectX::XMFLOAT3 startSize;
	DirectX::XMFLOAT3 startRotation;
	DirectX::XMFLOAT4 startColor;
	float startLifetime = 1.0f;
	ParticleEmissionShape emissionShape;
	ParticeRenderer renderer;
	ParticleEmissionCount emissionCount;
};
