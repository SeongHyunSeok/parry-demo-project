#include "StateMachine.h"
#include "Equipment.h"
#include "Animation.h"

StateMachineComponent::StateMachineComponent(
	Handle<GameObject> owner,
	std::unordered_map<StateType, std::unique_ptr<CharacterState>>&& states,
	StateType initialState
) : Component{ owner } {
	this->states = move(states);
	currentState = this->states[initialState].get();
	currentState->Enter(this, StateProperty());
}

void StateMachineComponent::Update()
{
	currentState->Update(this);
}

void StateMachineComponent::HandleInput(const PlayerInupt& playerInput)
{
	currentState->HandleInput(this, playerInput);
}

void StateMachineComponent::HandleEvent(GameEvent event)
{
	currentState->HandleEvent(this, event);
}

void StateMachineComponent::ChangeStateByInput(const PlayerInupt& playerInput)
{
	StateProperty sp;
	sp.dir = playerInput.dir;
	sp.shield = playerInput.shield;

	StateType target = StateType::Idle;
	switch (playerInput.type) {
	case InputType::None: 
		target = StateType::Idle; break;
	case InputType::Walk: 
		target = StateType::Walk; break;
	case InputType::Roll:
		target = StateType::Roll; break;
	case InputType::Attack:
		target = StateType::Attack; break;
	case InputType::Parray:
		target = StateType::Parry; break;
	}
	ChangeState(target, sp);
}

void StateMachineComponent::ChangeState(StateType targetStateType, StateProperty stateProperty)
{
	currentState->Leave(this);
	currentState = states[targetStateType].get();
	currentState->Enter(this, stateProperty);
}

void StateMachineComponent::SetOwnerAnimation(AnimationType animationType)
{
	auto hOwner = GetOwner();
	auto pComAni = WORLD.GetComponent<AnimationComponent>(owner);
	if (pComAni) {
		pComAni->mSkinnedModelAnimationController->SetAnimation(animationType);
	}
}

bool StateMachineComponent::CheckAnimationEnd()
{
	auto hOwner = GetOwner();
	auto pComAni = WORLD.GetComponent<AnimationComponent>(hOwner);
	if (pComAni) {
		return pComAni->mSkinnedModelAnimationController->IsAnimationEnd();
	}
}

AnimationType PlayerIdleState::GetAnimationType(StateMachineComponent* pStateMachine) {
	if (shield) {
		auto hOwner = pStateMachine->GetOwner();
		const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
		if (comEquipmentOwner
			&& comEquipmentOwner->equipments[(UINT)EquipmentTypes::Shield]) {
			if (comEquipmentOwner->equipments[(UINT)EquipmentTypes::TwoHandSword]) {
				return AnimationType::TwoHandTemp;
			}
			else {
				return AnimationType::OneHandBaseShieldIdle;
			}
		}
	}

	return AnimationType::Idle;
}

void PlayerIdleState::Enter(StateMachineComponent* pStateMachine, StateProperty stateProperty)
{
	shield = stateProperty.shield;
	shieldInputIn = stateProperty.shield;
	
	AnimationType aniType = GetAnimationType(pStateMachine);
	pStateMachine->SetOwnerAnimation(aniType);
}

void PlayerIdleState::Leave(StateMachineComponent* pStateMachine)
{
	shield = false;
	shieldInputIn = false;
}

void PlayerIdleState::Update(StateMachineComponent* pStateMachine)
{
	if (shield != shieldInputIn) {
		shield = !shield;
		pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
	}
	shieldInputIn = false;
}

void PlayerIdleState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (playerInput.type == InputType::None) {
		shieldInputIn = playerInput.shield;
	}
	else {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerIdleState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent event)
{
	if (event.eventType == GameEventType::Hit) {
		pStateMachine->ChangeState(StateType::Hit, StateProperty());
	}
}

void PlayerWalkState::Enter(StateMachineComponent* pStateMachine, StateProperty stateProperty)
{
	walkInputIn = true;
	shieldInputIn = stateProperty.shield;
	shield = stateProperty.shield;
	inputDirection = stateProperty.dir;
	direction = stateProperty.dir;

	AnimationType walkAnimation = GetAnimationType(pStateMachine);
	pStateMachine->SetOwnerAnimation(walkAnimation);
}

void PlayerWalkState::Leave(StateMachineComponent* pStateMachine)
{
	walkInputIn = false;
	shieldInputIn = false;
	shield = false;
	inputDirection = { 0, 0, 1 };
	direction = { 0, 0, 1 };
}

void PlayerWalkState::Update(StateMachineComponent* pStateMachine)
{
	if (walkInputIn) {
		if (shield != shieldInputIn || direction.x != inputDirection.x || direction.z != inputDirection.z) {
			shield = shieldInputIn;
			direction = inputDirection;
			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
		}

		shieldInputIn = false;
		walkInputIn = false;
	}
	else {
		StateProperty sp;
		sp.shield = shieldInputIn;
		pStateMachine->ChangeState(StateType::Idle, sp);
	}
}

void PlayerWalkState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (playerInput.type == InputType::Walk) {
		walkInputIn = true;
		shieldInputIn = playerInput.shield;
		inputDirection = playerInput.dir;
	}
	else {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerWalkState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent event)
{
	if (event.eventType == GameEventType::Hit) {
		pStateMachine->ChangeState(StateType::Hit, StateProperty());
	}
}

AnimationType PlayerWalkState::GetAnimationType(StateMachineComponent* pStateMachine)
{
	if (shield) {
		auto hOwner = pStateMachine->GetOwner();
		const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
		if (comEquipmentOwner
			&& comEquipmentOwner->equipments[(UINT)EquipmentTypes::Shield]) {
			if (comEquipmentOwner->equipments[(UINT)EquipmentTypes::TwoHandSword]) {
				return AnimationType::TwoHandTemp;
			}
			else {
				AnimationType animationType = AnimationType::OneHandBaseShieldWalkForward;
				if (direction.x == 1 && direction.z == 1) {
					animationType = AnimationType::OneHandBaseShieldWalkForwardRight45;
				}
				else if (direction.x == -1 && direction.z == 1) {
					animationType = AnimationType::OneHandBaseShieldWalkForwardLeft45;
				}
				else if (direction.x == 1 && direction.z == -1) {
					animationType = AnimationType::OneHandBaseShieldWalkBackwardRight45;
				}
				else if (direction.x == -1 && direction.z == -1) {
					animationType = AnimationType::OneHandBaseShieldWalkBackwardLeft45;
				}
				else if (direction.x == 1) {
					animationType = AnimationType::OneHandBaseShieldWalkRight;
				}
				else if (direction.x == -1) {
					animationType = AnimationType::OneHandBaseShieldWalkLeft;
				}
				else if (direction.z == 1) {
					animationType = AnimationType::OneHandBaseShieldWalkForward;
				}
				else if (direction.z == -1) {
					animationType = AnimationType::OneHandBaseShieldWalkBackward;
				}
				return animationType;
			}
		}
	}

	AnimationType animationType = AnimationType::WalkForward;
	if (direction.x == 1 && direction.z == 1) {
		animationType = AnimationType::WalkForwardRight45;
	}
	else if (direction.x == -1 && direction.z == 1) {
		animationType = AnimationType::WalkForwardLeft45;
	}
	else if (direction.x == 1 && direction.z == -1) {
		animationType = AnimationType::WalkBackwardRight45;
	}
	else if (direction.x == -1 && direction.z == -1) {
		animationType = AnimationType::WalkBackwardLeft45;
	}
	else if (direction.x == 1) {
		animationType = AnimationType::WalkRight;
	}
	else if (direction.x == -1) {
		animationType = AnimationType::WalkLeft;
	}
	else if (direction.z == 1) {
		animationType = AnimationType::WalkForward;
	}
	else if (direction.z == -1) {
		animationType = AnimationType::WalkBackward;
	}
	return animationType;
}

void PlayerRollState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	direction = property.dir;

	pStateMachine->SetOwnerAnimation(GetAnimationType());
}

void PlayerRollState::Leave(StateMachineComponent* pStateMachine)
{
	direction = { 0, 0, 1 };
}

void PlayerRollState::Update(StateMachineComponent* pStateMachine)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeState(StateType::Idle, StateProperty());
	}
}

void PlayerRollState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerRollState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent event)
{
	// TO DO : Implement to ignore damages in specific period
}

AnimationType PlayerRollState::GetAnimationType()
{
	AnimationType animationType = AnimationType::RollForward;
	if (direction.x == 1 && direction.z == 1) {
		animationType = AnimationType::RollForwardRight45;
	}
	else if (direction.x == -1 && direction.z == 1) {
		animationType = AnimationType::RollForwardLeft45;
	}
	else if (direction.x == 1 && direction.z == -1) {
		animationType = AnimationType::RollBackwardRight45;
	}
	else if (direction.x == -1 && direction.z == -1) {
		animationType = AnimationType::RollBackwardLeft45;
	}
	else if (direction.x == 1) {
		animationType = AnimationType::RollRight;
	}
	else if (direction.x == -1) {
		animationType = AnimationType::RollLeft;
	}
	else if (direction.z == 1) {
		animationType = AnimationType::RollForward;
	}
	else if (direction.z == -1) {
		animationType = AnimationType::RollBackward;
	}
	return animationType;
}
