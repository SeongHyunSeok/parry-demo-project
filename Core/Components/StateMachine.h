#pragma once

#include "../component.h"
#include "../GameEvent.h"
#include "../world.h"
#include "../../Model/Animation.h"

#include <chrono>

enum class StateType : unsigned int
{
	Idle,
	Walk,
	Roll,
	Attack,
	Parry,
	Hit,
	Parried,
	Pattern
};

enum class EnemyPattern : unsigned int
{
	None,
	EnemyAttack1,
	EnemyAttack2,
	EnemyAttack3,
	EnemyAttack1Offbeat,
	EnemyAttack2Offbeat,
	EnemyAttack3Offbeat,

	Combo,

	StrafeLeft,
	StrafeRight,
	BackStep,
	Wait
};

struct EnemyAttackPatternSettings
{
	static constexpr unsigned int PATTERN_COUNT = 7;

	bool enabled[PATTERN_COUNT] = { true, true, true, true, true, true, true };

	bool IsEnabled(EnemyPattern pattern) const
	{
		const unsigned int patternIndex = static_cast<unsigned int>(pattern);
		const unsigned int firstAttackIndex = static_cast<unsigned int>(EnemyPattern::EnemyAttack1);
		const unsigned int comboIndex = static_cast<unsigned int>(EnemyPattern::Combo);

		if (patternIndex < firstAttackIndex || patternIndex > comboIndex)
		{
			return true;
		}

		return enabled[patternIndex - firstAttackIndex];
	}
};

struct StateProperty
{
	XMINT3 dir = { 0, 0, 0 };
	bool shield = false;
	float strength = 0.0f;
	bool active = true;

	EnemyPattern enemyPattern = EnemyPattern::None;
};

class CharacterState;

const float FATATL_HIT_STRENGTH = 1.0f;
const float MIDDLE_HIT_STRENGTH = 0.3f;
const float HIGH_HIT_STRENGTH = 0.7f;

class StateMachineComponent : public Component
{
public:
	StateMachineComponent(
		Handle<GameObject> owner, 
		std::unordered_map<StateType, std::unique_ptr<CharacterState>>&& states,
		StateType initialState
	);
	StateMachineComponent(const StateMachineComponent& rhs) = delete;
	StateMachineComponent(StateMachineComponent&&) = default;

	void Update();
	void HandleInput(const PlayerInupt& playerInput);
	void HandleGameEvent(GameEvent& event);

	void ChangeStateByInput(const PlayerInupt& playerInput);
	void ChangeState(StateType targetStateType, StateProperty property);
	void SetOwnerAnimation(AnimationType animationType);
	void SetOwnerAnimation(AnimationType animationType, float speed);
	bool CheckAnimationEnd();
	bool IsInValidFrame(float offset = 0.0f);
	bool IsAnimationStarting();
	bool CheckDistAndSetToFaceOpponent(Handle<GameObject> opponentHandle, float distThreshold, float* distance=nullptr);
	StateType GetCurrentSatateType();
private:
	std::unordered_map<StateType, std::unique_ptr<CharacterState>> states;
	CharacterState* currentState = nullptr;
	StateType currentStateType = StateType::Idle;
};

class CharacterState
{
public:
	CharacterState() {}
	virtual void Enter(StateMachineComponent* pStateMachine, StateProperty property) = 0;
	virtual void Leave(StateMachineComponent* pStateMachine) = 0;
	virtual void Update(StateMachineComponent* pStateMachine) = 0;
	virtual void HandleInput(
		StateMachineComponent* pStateMachine, 
		const PlayerInupt& playerInput
	) = 0;
	virtual void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) = 0;

	StateProperty createHitStatePropertyFromGameEvent(const GameEvent& event, bool shield){
		StateProperty sp;
		sp.strength = event.strength;
		sp.dir = event.dir;
		sp.shield = shield;
		return sp;
	}
};

class PlayerIdleState : public CharacterState
{
public:
	PlayerIdleState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
private:
	bool shieldInputIn = false;
	bool shield = false;
private:
	AnimationType GetAnimationType(StateMachineComponent* pStateMachine);
};

class PlayerWalkState : public CharacterState
{
public:
	PlayerWalkState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
private:
	bool walkInputIn = false;
	bool shieldInputIn = false;
	bool shield = false;
	XMINT3 inputDirection = { 0, 0, 1 };
	XMINT3 direction = { 0, 0, 1 };

	AnimationType GetAnimationType(StateMachineComponent* pStateMachine);
};

class PlayerRollState : public CharacterState
{
public:
	PlayerRollState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
private:
	XMINT3 direction = { 0, 0, 1 };
	bool twoHand = false;
	AnimationType GetAnimationType();
};

class PlayerAttackState : public CharacterState
{
public:
	PlayerAttackState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
protected:
	const unsigned int MAX_OneHand_COMBO_COUNT = 7;
	const unsigned int MAX_TwoHand_COMBO_COUNT = 3;
	unsigned int maxComboCount = 0;
	unsigned int currentComboCount = 0;
	bool doNextCombo = false;
	bool fatalAttack = false;

	AnimationType GetAnimationType(StateMachineComponent* pStateMachine);
};

class PlayerParryState : public CharacterState
{
public:
	PlayerParryState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
};

class PlayerHitState : public CharacterState
{
public:
	PlayerHitState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
private:
	XMINT3 direction = { 0, 0, 1 };
	float strength = 0.0f;
	bool fatalHit = false;
	bool shield = false;
	bool twoHand = false;
	AnimationType GetAnimationType();
};

class PlayerParriedState : public CharacterState
{
public:
	PlayerParriedState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
};

const float ATTACK_DISTANCE = 10.0f;
const float CLOSE_DISTANCE = 3.0f;
const float MID_DISTANCE = 6.5f;

class EnemyIdleState : public CharacterState
{
public:
	EnemyIdleState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
private:
	bool bActive = false;
};

// 적 패턴 관리를 위한 상태 클래스. 적의 공격 패턴, 이동 패턴 등을 결정하고 실행하는 역할을 담당
class EnemyPatternState : public CharacterState
{
public:
	explicit EnemyPatternState(const EnemyAttackPatternSettings& attackPatternSettings)
		: attackPatternSettings(attackPatternSettings)
	{}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;

private:
	// 적 패턴과 가중치를 나타내는 구조체. 패턴과 해당 패턴의 가중치를 함께 저장
	struct PatternWeight
	{
		EnemyPattern pattern;
		int weight;
	};

	EnemyPattern SelectPattern(StateMachineComponent* pStateMachine);
	EnemyPattern SelectWeightedPattern(const std::vector<PatternWeight>& patterns);

	std::vector<PatternWeight> BuildAttackPatterns(StateMachineComponent* pStateMachine, float distance);
	std::vector<PatternWeight> BuildWatchingPatterns(float distance);

	void ExecutePattern(StateMachineComponent* pStateMachine, EnemyPattern pattern);
	void UpdatePatternHistory(EnemyPattern pattern);
	void DisablePreviousAttack(std::vector<PatternWeight>& patterns) const;
	void DisableUncheckedAttacks(std::vector<PatternWeight>& patterns) const;

	bool CanUseEnemyAttack(StateMachineComponent* pStateMachine);
	bool IsAttackPattern(EnemyPattern pattern) const;
	bool IsWatchingPattern(EnemyPattern pattern) const;

private:
	EnemyPattern previousPattern = EnemyPattern::None;

	// 공격패턴의 연속 사용 횟수를 추적하는 변수. 공격 패턴이 일정 횟수 이상 반복되지 않기 위한 장치
	int consecutiveAttackCount = 0;
	const unsigned int MAX_CONSECUTIVE_ATTACK_COUNT = 3;
	const EnemyAttackPatternSettings& attackPatternSettings;
};

class EnemyAttackState : public PlayerAttackState
{
public:
	EnemyAttackState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;

private:
	bool IsEnemyAttackPattern(EnemyPattern pattern) const;

	AnimationType GetEnemyAttackAnimation(EnemyPattern pattern) const;

private:
	EnemyPattern currentPattern = EnemyPattern::None;

	bool bActive = true;
};

class EnemyHitState : public PlayerHitState
{
public:
	EnemyHitState() {}
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
};

class EnemyParriedState : public PlayerParriedState
{
public:
	EnemyParriedState() {}
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
};

class EnemyWalkState : public CharacterState
{
public:
	EnemyWalkState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;

	AnimationType GetAnimationType(StateMachineComponent* pStateMachine);


private:
	bool IsTimedPattern() const;

private:
	EnemyPattern currentPattern = EnemyPattern::None;

	float moveDuration = 0.0f;

	std::chrono::steady_clock::time_point moveStartTime;
	std::chrono::steady_clock::time_point lastUpdateTime;

	const float MIN_STRAFE_DURATION = 0.45f;
	const float MAX_STRAFE_DURATION = 0.95f;

	const float MIN_BACKSTEP_DURATION = 0.25f;
	const float MAX_BACKSTEP_DURATION = 0.45f;
	const float MIN_CLOSE_BACKSTEP_DURATION = 0.55f;
	const float MAX_CLOSE_BACKSTEP_DURATION = 0.75f;

	const float MIN_WAIT_DURATION = 0.15f;
	const float MAX_WAIT_DURATION = 0.45f;

};
