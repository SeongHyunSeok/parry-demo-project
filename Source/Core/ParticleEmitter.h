#pragma once

#include "Particle.h"

#include <random>

class ParticleEmitter
{
	DirectX::XMFLOAT3 translation;
	DirectX::XMFLOAT3 scale;
	DirectX::XMFLOAT3 rotation;
	float timePos = 0.0f;
	float lastCycleTimePose = 0.0f;
	UINT curCycle = 0;

	XMMATRIX baseScale;
	XMMATRIX baseOffset;
public:
	std::vector<Particle> particles;

	bool isFinished = false;
	const ParticleProperties* const particleProperties = nullptr;
public:
	ParticleEmitter(
		DirectX::XMFLOAT3 translation,
		DirectX::XMFLOAT3 scale,
		DirectX::XMFLOAT3 rotation,
		const ParticleProperties* const particleProperties
	);
	void Update(float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook, std::mt19937& randomEngine);

private:
	void Emit(std::mt19937& randomEngine);
	void UpdateParticle(Particle& particle, float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook);
};
