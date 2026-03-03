#pragma once

#include "../component.h"
#include "../GameEvent.h"
#include "../world.h"
#include "../Model/Animation.h"

enum class StateType : unsigned int
{
	Idle,
	Walk,
	Roll,
	Attack,
	Parry,
	Hit
};

struct StateProperty
{
	XMINT3 dir = { 0, 0, 0 };
	bool shield = false;
};

class CharacterState;

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
	void HandleEvent(GameEvent event);

	void ChangeStateByInput(const PlayerInupt& playerInput);
	void ChangeState(StateType targetStateType, StateProperty property);
	void SetOwnerAnimation(AnimationType animationType);
	void SetOwnerAnimation(AnimationType animationType, float speed);
	bool CheckAnimationEnd();
private:
	std::unordered_map<StateType, std::unique_ptr<CharacterState>> states;
	CharacterState* currentState = nullptr;
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
	virtual void HandleEvent(StateMachineComponent* pStateMachine, GameEvent event) = 0;
};

class PlayerIdleState : public CharacterState
{
public:
	PlayerIdleState() {}
	void Enter(StateMachineComponent* pStateMachine, StateProperty property) override;
	void Leave(StateMachineComponent* pStateMachine) override;
	void Update(StateMachineComponent* pStateMachine) override;
	void HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput) override;
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent event) override;
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
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent event) override;
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
	void HandleEvent(StateMachineComponent* pStateMachine, GameEvent event) override;
private:
	XMINT3 direction = { 0, 0, 1 };

	AnimationType GetAnimationType();
};