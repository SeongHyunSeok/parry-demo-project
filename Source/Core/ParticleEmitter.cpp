#include "ParticleEmitter.h"

ParticleEmitter::ParticleEmitter(
	DirectX::XMFLOAT3 translation,
	DirectX::XMFLOAT3 scale,
	DirectX::XMFLOAT3 rotation,
	const ParticleProperties* const particleProperties
) : translation{ translation }, scale{ scale }, rotation{rotation}, particleProperties{particleProperties}
{
	baseScale = XMMatrixScaling(scale.x, scale.y, scale.z);
	baseOffset = XMMatrixTranslation(translation.x, translation.y, translation.z);
}

void ParticleEmitter::Update(float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook, std::mt19937& randomEngine)
{
	if (particleProperties == nullptr)
		return;

	timePos += dt;

	// Create Particles
	if (curCycle < particleProperties->emissionCount.cycle) {
		if (lastCycleTimePose == 0.0f || lastCycleTimePose + particleProperties->emissionCount.interval <= timePos) {
			Emit(randomEngine);
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
		UpdateParticle(particles[i], dt, cameraUp, cameraLook);

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

void ParticleEmitter::Emit(std::mt19937& randomEngine) {

	std::uniform_real_distribution<float> radiusDistribution(
		particleProperties->emissionShape.minRadius,
		particleProperties->emissionShape.maxRadius);
	std::uniform_real_distribution<float> angleDistribution(0, 2 * MathHelper::Pi);
	std::uniform_real_distribution<float> sphereDistribution(-1.0f, 1.0f);
	std::uniform_real_distribution<float> speedDistribution(particleProperties->move.startMinSpeed, particleProperties->move.startMaxSpeed);
	std::uniform_real_distribution<float> lifetimeDistribution(particleProperties->minLifetime, particleProperties->maxLifetime);
	std::uniform_real_distribution<float> sizeDistribution(particleProperties->size.startMinSize, particleProperties->size.startMaxSize);

	for (int i = 0; i < particleProperties->emissionCount.count; ++i) {
		XMMATRIX particleRotation = XMMatrixIdentity();
		DirectX::XMVECTOR particlePosition = { 0.0f, 0.0f, particleProperties->emissionShape.maxRadius, 1.0f };
		XMFLOAT3 particleDirection = { 0.0f, 0.0f, 0.0f };

		switch (particleProperties->emissionShape.mode) {
		case ParticleEmissionModeType::RANDOM: {
			float radius = radiusDistribution(randomEngine);

			switch (particleProperties->emissionShape.shape) {
			case ParticleEmissionShapeType::CIRCLE: {
				particlePosition = { 0.0f, 0.0f, radius, 1.0f };

				float angle = angleDistribution(randomEngine);
				particleRotation = XMMatrixRotationY(angle + rotation.y) * XMMatrixRotationX(rotation.x) * XMMatrixRotationZ(rotation.z);

				XMStoreFloat3(&particleDirection, XMVector3Normalize(XMVector4Transform(particlePosition, particleRotation)));
				break;
			}
			case ParticleEmissionShapeType::SPHERE:
			case ParticleEmissionShapeType::HEMISPHERE:
			{
				particlePosition = {
					sphereDistribution(randomEngine),
					sphereDistribution(randomEngine),
					sphereDistribution(randomEngine),
					0.0f
				};

				if (particleProperties->emissionShape.shape == ParticleEmissionShapeType::HEMISPHERE) {
					if (particlePosition.m128_f32[1] < 0) {
						particlePosition.m128_f32[1] = -particlePosition.m128_f32[1];
					}
					particleRotation = XMMatrixRotationY(rotation.y) * XMMatrixRotationX(rotation.x) * XMMatrixRotationZ(rotation.z);
				}
				particlePosition = XMVector3Normalize(particlePosition);
				XMStoreFloat3(&particleDirection, particlePosition);

				particlePosition = radius * particlePosition;
				particlePosition.m128_f32[3] = 1.0f;
				break;
			}
			}
			break;
		}
		case ParticleEmissionModeType::LOOP: {
			float angle = (timePos * particleProperties->emissionShape.loopSpeed) + float(i/10.0f) ;

			particleRotation = XMMatrixRotationY(angle + rotation.y) * XMMatrixRotationX(rotation.x) * XMMatrixRotationZ(rotation.z);

			XMStoreFloat3(&particleDirection, XMVector3Normalize(XMVector4Transform(particlePosition, particleRotation)));
			break;
		}
		};


		XMMATRIX transform = baseScale * particleRotation * baseOffset;
		particlePosition = XMVector4Transform(particlePosition, transform);

		XMFLOAT3 pos;
		XMStoreFloat3(&pos, particlePosition);
		// apply base size
		
		XMFLOAT3 localRotation = particleProperties->renderer.localRotation;
		XMMATRIX emitterRotation = XMMatrixRotationY(rotation.y + localRotation.y) * XMMatrixRotationX(rotation.x + localRotation.x) * XMMatrixRotationZ(rotation.z + localRotation.z);
		XMFLOAT3 up;
		XMStoreFloat3(&up, XMVector4Transform(XMVECTOR{ 0.0f, 1.0f, 0.0f, 1.0f }, emitterRotation));
		XMFLOAT3 look;
		XMStoreFloat3(&look, XMVector4Transform(XMVECTOR{ 0.0f, 0.0f, 1.0f, 1.0f }, emitterRotation));

		const float startSize = sizeDistribution(randomEngine);
		Particle particle(
			{ pos },
			{ startSize, startSize, 0.0f},
			up,
			look,
			particleProperties->startColor,
			Vector3::ScalarProduct(particleDirection, speedDistribution(randomEngine)),
			lifetimeDistribution(randomEngine),
			particleProperties->rotation.startRotationSpeed,
			particleProperties->renderer.materialIndex
		);
		particle.rotateAngle = angleDistribution(randomEngine);
		particles.emplace_back(particle);
	}
}


void ParticleEmitter::UpdateParticle(Particle& particle, float dt, const XMFLOAT3& cameraUp, const XMFLOAT3& cameraLook) {
	particle.timePos += dt;

	particle.rotateSpeed += dt * 10 * (particleProperties->rotation.changeSpeed) * ((int)particleProperties->rotation.mode);
	particle.rotateSpeed = max(0.0f, particle.rotateSpeed);
	particle.rotateAngle += (dt * particle.rotateSpeed);

	if (!particleProperties->renderer.localAlignment) {
		particle.look = cameraLook;
		particle.up = cameraUp;

		if (particleProperties->renderer.alignToVelocity) {
			const XMVECTOR lookVector = XMVector3Normalize(XMLoadFloat3(&particle.look));
			const XMVECTOR velocityVector = XMLoadFloat3(&particle.velocity);
			const XMVECTOR projectedVelocity = XMVectorSubtract(
				velocityVector,
				XMVectorMultiply(XMVector3Dot(velocityVector, lookVector), lookVector));

			if (XMVectorGetX(XMVector3LengthSq(projectedVelocity)) > 0.000001f) {
				XMStoreFloat3(&particle.up, XMVector3Normalize(projectedVelocity));
			}
		}

		XMVECTOR lookVector = XMLoadFloat3(&particle.look);
		XMVECTOR upVector = XMLoadFloat3(&particle.up);
		if (particleProperties->rotation.rotate[2]) {
			XMMATRIX rotation = XMMatrixRotationAxis(lookVector, particle.rotateAngle);
			XMVECTOR rotatedUp = XMVector4Transform(upVector, rotation);
			XMStoreFloat3(&particle.up, rotatedUp);
		}
	}
	else {
		XMVECTOR lookVector = XMLoadFloat3(&particle.look);
		XMVECTOR upVector = XMLoadFloat3(&particle.up);
		float changeAngle = dt * particle.rotateSpeed;
		if (particleProperties->rotation.rotate[0]) {
			XMVECTOR rightVector = XMVector3Normalize(XMVector3Cross(lookVector, upVector));
			XMMATRIX rotation = XMMatrixRotationAxis(rightVector, changeAngle);

			XMVECTOR rotatedLook = XMVector4Transform(lookVector, rotation);
			XMStoreFloat3(&particle.look, rotatedLook);
			XMVECTOR rotatedUp = XMVector4Transform(upVector, rotation);
			XMStoreFloat3(&particle.up, rotatedUp);
		}
		else if (particleProperties->rotation.rotate[1]) {
			XMMATRIX rotation = XMMatrixRotationAxis(upVector, changeAngle);
			XMVECTOR rotatedLook = XMVector4Transform(lookVector, rotation);
			XMStoreFloat3(&particle.look, rotatedLook);
		}
		else if (particleProperties->rotation.rotate[2]) {
			XMMATRIX rotation = XMMatrixRotationAxis(lookVector, changeAngle);
			XMVECTOR rotatedUp = XMVector4Transform(upVector, rotation);
			XMStoreFloat3(&particle.up, rotatedUp);
		}
	}
	
	float speed = Vector3::Length(particle.velocity);
	speed += dt * 10 * (particleProperties->move.changeSpeed) * ((int)particleProperties->move.mode);
	if (speed < 0.0f) {
		particle.isFinished = true;
		return;
	}
	if (speed > 0.000001f) {
		particle.velocity = Vector3::ScalarProduct(particle.velocity, speed, true);
	}
	particle.position = Vector3::Add(particle.position, particle.velocity, dt);

	const float sizeDelta = dt * 10 * particleProperties->size.changeSpeed * ((int)particleProperties->size.mode);
	particle.size.x = max(0.0f, particle.size.x + sizeDelta);
	particle.size.y = max(0.0f, particle.size.y + sizeDelta);

	if (particleProperties->renderer.fadeOut && particle.lifeTime > 0.0f) {
		const float lifeRatio = max(0.0f, 1.0f - particle.timePos / particle.lifeTime);
		particle.color.x = particleProperties->startColor.x * lifeRatio;
		particle.color.y = particleProperties->startColor.y * lifeRatio;
		particle.color.z = particleProperties->startColor.z * lifeRatio;
		particle.color.w = particleProperties->startColor.w * lifeRatio;
	}

	if (particle.lifeTime <= particle.timePos) {
		particle.isFinished = true;
	}

}
