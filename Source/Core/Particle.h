#pragma once

#include "d3dUtil.h"

struct Particle
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 size;
	DirectX::XMFLOAT3 up;
	DirectX::XMFLOAT3 look;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 velocity;
	float lifeTime;
	float rotateSpeed;
	UINT materialIndex;
	float timePos = 0.0f;
	bool isFinished = false;
	float rotateAngle = 0.0f;
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
	DirectX::XMFLOAT3 localRotation;
	UINT materialIndex;

	// 불꽃 겹칠 시 밝기 더하기
	bool additive = false;
	// 이동방향에 맞게 정렬
	bool alignToVelocity = false;
	// 점점 투명하게
	bool fadeOut = false;
};

struct ParticleEmissionShape
{
	ParticleEmissionShapeType shape = ParticleEmissionShapeType::CIRCLE;
	ParticleEmissionModeType mode = ParticleEmissionModeType::RANDOM;
	float minRadius = 0.0f;
	float maxRadius = 0.0f;
	float loopSpeed = 1.0f;
};

enum class ParticleChangeMode : int
{
	NOT_CHANGE = 0,
	BIGGER = 1,
	SMALLER = -1,
};

struct ParticleMove
{
	float startMinSpeed = 0.0f;
	float startMaxSpeed = 0.0f;
	ParticleChangeMode mode = ParticleChangeMode::NOT_CHANGE;
	float changeSpeed = 1.0;
};

struct ParticleSize
{
	float startMinSize = 0.0f;
	float startMaxSize = 0.0f;
	ParticleChangeMode mode = ParticleChangeMode::NOT_CHANGE;
	float changeSpeed = 1.0;
};

struct ParticleRotation
{
	bool rotate[3] = {false, false, false};
	float startRotationSpeed = 0.0f;
	ParticleChangeMode mode = ParticleChangeMode::NOT_CHANGE;
	float changeSpeed = 1.0;
};


struct ParticleProperties
{
	bool looping = false;
	float minLifetime = 1.0f;
	float maxLifetime = 1.0f;
	ParticleMove move;
	ParticleRotation rotation;
	ParticleSize size;
	DirectX::XMFLOAT4 startColor;
	ParticleEmissionShape emissionShape;
	ParticeRenderer renderer;
	ParticleEmissionCount emissionCount;
};
