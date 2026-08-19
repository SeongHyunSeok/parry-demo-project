#pragma once

#include "Particle.h"

class ParticleEmitter
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 size;
	DirectX::XMFLOAT3 rotation;
	float timePos = 0.0f;
	float lastCycleTimePose = 0.0f;
	UINT curCycle = 0;
public:
	std::vector<Particle> particles;

	bool isFinished = false;
	const ParticleProperties* const particleProperties = nullptr;
public:
	ParticleEmitter(
		DirectX::XMFLOAT3 position,
		DirectX::XMFLOAT3 size,
		DirectX::XMFLOAT3 rotation,
		const ParticleProperties* const particleProperties
	);
	void Update(float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook);

private:
	void Emit();
	void UpdateParticle(Particle& particle, float dt, float startLifetime, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook);
};
