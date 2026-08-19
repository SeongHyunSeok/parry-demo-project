#include "ParticleEmitter.h"

ParticleEmitter::ParticleEmitter(
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 size,
	DirectX::XMFLOAT3 rotation,
	const ParticleProperties* const particleProperties
) : position{position}, size{size}, rotation{rotation}, particleProperties{particleProperties}
{ }

void ParticleEmitter::Update(float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook)
{
	if (particleProperties == nullptr)
		return;

	timePos += dt;

	// Create Particles
	if (curCycle < particleProperties->emissionCount.cycle) {
		if (lastCycleTimePose == 0.0f || lastCycleTimePose + particleProperties->emissionCount.interval <= timePos) {
			Emit();
			lastCycleTimePose = timePos;
			++curCycle;
			if (particleProperties->looping && curCycle >= particleProperties->emissionCount.cycle) {
				curCycle = 0;
			}
		}
	}

	// Update Particles
	unsigned int particleCount = particles.size();
	for (unsigned int i = 0; i < particleCount;) {
		UpdateParticle(particles[i], dt, particleProperties->startLifetime, cameraUp, cameraLook);

		if (particles[i].isFinished) {
			std::swap(particles[i], particles.back());
			particles.pop_back();
			--particleCount;
		}
		else {
			++i;
		}
	}

	// Check Finished
	if (curCycle >= particleProperties->emissionCount.cycle && particles.empty()) {
		isFinished = true;
	}
}

void ParticleEmitter::Emit() {
	for (int i = 0; i < particleProperties->emissionCount.count; ++i) {
		particles.emplace_back(Particle(
			position,
			particleProperties->startSize,
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f },
			particleProperties->startColor,
			particleProperties->renderer.materialIndex
		));
	}
}

void ParticleEmitter::UpdateParticle(Particle& particle, float dt, float startLifetime, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook) {
	particle.up = cameraUp;
	particle.look = cameraLook;
	particle.timePos += dt;
	particle.position.y += 0.05f;

	if (startLifetime <= particle.timePos) {
		particle.isFinished = true;
	}
}
