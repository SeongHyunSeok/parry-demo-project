#pragma once

#include "../component.h"
#include "../GameEvent.h"
#include "../world.h"
#include "../../Model/Animation.h"

enum class StateType : unsigned int
{
	Idle,
	Walk,
	Roll,
	Attack,
	Parry,
	Hit,
	Parried
};

struct StateProperty
{
	XMINT3 dir = { 0, 0, 0 };
	bool shield = false;
	float strength = 0.0f;
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
	bool CheckDistAndSetToFaceOpponent(Handle<GameObject> opponentHandle, float distThreshold);
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
	const unsigned int MAX_COMBO_COUNT = 8;
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

class EnemyIdleState : public CharacterState
{
public:
	EnemyIdleState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event) override;
};

class EnemyAttackState : public PlayerAttackState
{
public:
	EnemyAttackState() {}
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
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
