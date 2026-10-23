#pragma once

#include "../component.h"

#include <algorithm>

struct CombatModeSettings
{
	float playerMaxHealth = 1000.0f;
	float playerMaxStamina = 100.0f;
	float enemyMaxHealth = 2000.0f;
	float playerAttackDamage = 100.0f;
	float playerFatalDamage = 350.0f;
	float enemyAttackDamage = 180.0f;
	float shieldDamageReductionPercent = 65.0f;
	float shieldBlockStaminaCost = 20.0f;
	float attackStaminaCost = 20.0f;
	float rollStaminaCost = 25.0f;
	float parryStaminaCost = 15.0f;
	float staminaRecoveryPerSecond = 28.0f;
	float staminaRecoveryDelay = 0.8f;
	float enemyAttackAnimationSpeed = 1.0f;
	float parryJudgementFrameOffset = 0.0f;
	float rollJudgementFrameOffset = 0.0f;

	bool operator==(const CombatModeSettings&) const = default;
};

inline float CalculateGuardedDamage(float incomingDamage, float reductionPercent)
{
	const float safeDamage = (std::max)(0.0f, incomingDamage);
	const float reduction = std::clamp(reductionPercent, 0.0f, 100.0f) / 100.0f;
	return safeDamage * (1.0f - reduction);
}

enum class CombatAction
{
	Attack,
	Roll,
	Parry
};

class CombatStatsComponent : public Component
{
public:
	explicit CombatStatsComponent(Handle<GameObject> owner)
		: Component(owner)
	{}
	CombatStatsComponent(const CombatStatsComponent&) = delete;
	CombatStatsComponent(CombatStatsComponent&&) = default;

	void ConfigurePlayer(const CombatModeSettings& settings)
	{
		combatActive = true;
		usesStamina = true;
		maxHealth = (std::max)(1.0f, settings.playerMaxHealth);
		maxStamina = (std::max)(1.0f, settings.playerMaxStamina);
		attackCost = (std::max)(0.0f, settings.attackStaminaCost);
		rollCost = (std::max)(0.0f, settings.rollStaminaCost);
		parryCost = (std::max)(0.0f, settings.parryStaminaCost);
		guardCost = (std::max)(0.0f, settings.shieldBlockStaminaCost);
		staminaRecoveryPerSecond = (std::max)(0.0f, settings.staminaRecoveryPerSecond);
		staminaRecoveryDelay = (std::max)(0.0f, settings.staminaRecoveryDelay);
		Reset();
	}

	void ConfigureEnemy(const CombatModeSettings& settings)
	{
		combatActive = true;
		usesStamina = false;
		maxHealth = (std::max)(1.0f, settings.enemyMaxHealth);
		maxStamina = 0.0f;
		Reset();
	}

	void Disable()
	{
		combatActive = false;
	}

	void Reset()
	{
		health = maxHealth;
		stamina = maxStamina;
		timeSinceStaminaUse = staminaRecoveryDelay;
	}

	void Update(float elapsedTime)
	{
		if (!combatActive || !usesStamina || IsDead())
			return;
		const float deltaTime = (std::max)(elapsedTime, 0.0f);
		const float recoveryBefore = (std::max)(0.0f, timeSinceStaminaUse - staminaRecoveryDelay);
		timeSinceStaminaUse += deltaTime;
		const float recoveryAfter = (std::max)(0.0f, timeSinceStaminaUse - staminaRecoveryDelay);
		const float recoveryTime = recoveryAfter - recoveryBefore;
		if (recoveryTime > 0.0f)
		{
			stamina = (std::min)(maxStamina,
				stamina + staminaRecoveryPerSecond * recoveryTime);
		}
	}

	bool TryConsumeStamina(CombatAction action)
	{
		if (!combatActive || !usesStamina)
			return true;

		float cost = 0.0f;
		switch (action)
		{
		case CombatAction::Attack: cost = attackCost; break;
		case CombatAction::Roll: cost = rollCost; break;
		case CombatAction::Parry: cost = parryCost; break;
		}
		if (stamina < cost)
			return false;

		stamina -= cost;
		timeSinceStaminaUse = 0.0f;
		return true;
	}

	void ConsumeGuardStamina()
	{
		if (!combatActive || !usesStamina || guardCost <= 0.0f)
			return;
		stamina = (std::max)(0.0f, stamina - guardCost);
		timeSinceStaminaUse = 0.0f;
	}

	void TakeDamage(float damage)
	{
		if (!combatActive || IsDead())
			return;
		health = (std::max)(0.0f, health - (std::max)(damage, 0.0f));
	}

	bool IsCombatActive() const { return combatActive; }
	bool IsDead() const { return health <= 0.0f; }
	float GetHealth() const { return health; }
	float GetMaxHealth() const { return maxHealth; }
	float GetStamina() const { return stamina; }
	float GetMaxStamina() const { return maxStamina; }
	float GetHealthRatio() const { return maxHealth > 0.0f ? health / maxHealth : 0.0f; }
	float GetStaminaRatio() const { return maxStamina > 0.0f ? stamina / maxStamina : 0.0f; }

private:
	bool combatActive = false;
	bool usesStamina = false;
	float health = 1.0f;
	float maxHealth = 1.0f;
	float stamina = 0.0f;
	float maxStamina = 0.0f;
	float attackCost = 0.0f;
	float rollCost = 0.0f;
	float parryCost = 0.0f;
	float guardCost = 0.0f;
	float staminaRecoveryPerSecond = 0.0f;
	float staminaRecoveryDelay = 0.0f;
	float timeSinceStaminaUse = 0.0f;
};
