#include "StateMachine.h"
#include "Equipment.h"
#include "Animation.h"
#include "Player.h"
#include "Mesh.h"

namespace
{
	std::mt19937& GetEnemyRandomGenerator()
	{
		static std::random_device rd;
		static std::mt19937 generator(rd());

		return generator;
	}

	float GetRandomFloat(float minValue, float maxValue)
	{
		std::uniform_real_distribution<float> distribution(minValue, maxValue);

		return distribution(GetEnemyRandomGenerator());
	}
}

StateMachineComponent::StateMachineComponent(
	Handle<GameObject> owner,
	std::unordered_map<StateType, std::unique_ptr<CharacterState>>&& states,
	StateType initialState
) : Component{ owner } {
	this->states = move(states);
	currentState = this->states[initialState].get();
	currentStateType = initialState;
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

void StateMachineComponent::HandleGameEvent(GameEvent& event)
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
	currentStateType = targetStateType;
	currentState->Enter(this, stateProperty);
}

void StateMachineComponent::SetOwnerAnimation(AnimationType animationType, float speed)
{
	auto hOwner = GetOwner();
	auto pComAni = WORLD.GetComponent<AnimationComponent>(owner);
	if (pComAni) {
		pComAni->mSkinnedModelAnimationController->SetAnimation(animationType, speed);
	}
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
	return false;
}

bool StateMachineComponent::IsInValidFrame(float offset)
{
	auto hOwner = GetOwner();
	auto pComAni = WORLD.GetComponent<AnimationComponent>(hOwner);
	if (pComAni) {
		return pComAni->mSkinnedModelAnimationController->IsTimposInEffectiveFrame(offset);
	}
	return false;
}

bool StateMachineComponent::IsAnimationStarting()
{
	auto hOwner = GetOwner();
	const auto comAnimation = WORLD.GetComponent<AnimationComponent>(hOwner);
	return comAnimation && comAnimation->mSkinnedModelAnimationController->IsAnimationStarting();
}

bool StateMachineComponent::CheckDistAndSetToFaceOpponent(Handle<GameObject> opponentHandle, float distThreshold, float* distance)
{
	auto pMyMesh = WORLD.GetComponent<MeshComponent>(GetOwner());
	auto pOpponentMesh = WORLD.GetComponent<MeshComponent>(opponentHandle);

	XMVECTOR myXZPosition = { pMyMesh->Translation.x, pMyMesh->Translation.z };
	XMVECTOR oppnentXZPosition = { pOpponentMesh->Translation.x, pOpponentMesh->Translation.z };

	XMVECTOR distXZVec = oppnentXZPosition - myXZPosition;

	float distXZ = 0.0f;
	XMStoreFloat(&distXZ, XMVector2Length(distXZVec));

	float distY = std::abs(pMyMesh->Translation.y - pOpponentMesh->Translation.y);

	XMFLOAT2 defualtDir = XMFLOAT2(0, 1);
	XMVECTOR defaultDirVec = XMLoadFloat2(&defualtDir);
	XMVECTOR dirVec = XMVector2Normalize(distXZVec);

	float yAngle = XMVector2AngleBetweenNormals(defaultDirVec, dirVec).m128_f32[0];

	if (distXZVec.m128_f32[0] < 0.0f)
	{
		yAngle = -yAngle;
	}

	pMyMesh->Rotation.y = yAngle;

	if (distance != nullptr)
		*distance = distXZ;

	if (distXZ < distThreshold && distY < distThreshold) {
		return true;
	}
	return false;
}

StateType StateMachineComponent::GetCurrentSatateType()
{
	return currentStateType;
}


AnimationType PlayerIdleState::GetAnimationType(StateMachineComponent* pStateMachine) {
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		if (shield)
			return AnimationType::TwoHandBlockIdle;
		return AnimationType::TwoHandIdle;
	}	
	else if (shield && comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::Shield]) {
			return AnimationType::OneHandBaseShieldIdle;
	}

	return AnimationType::Idle;
}

void PlayerIdleState::Enter(StateMachineComponent* pStateMachine, StateProperty stateProperty)
{
	
	shield = stateProperty.shield;
	shieldInputIn = stateProperty.shield;
	
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		pStateMachine->SetOwnerAnimation(AnimationType::TwoHandIdle);
		return;
	}

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

void PlayerIdleState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	if (event.eventType == GameEventType::Hit) {
		pStateMachine->ChangeState(StateType::Hit, createHitStatePropertyFromGameEvent(event, shield));
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

void PlayerWalkState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	if (event.eventType == GameEventType::Hit) {
		pStateMachine->ChangeState(StateType::Hit, createHitStatePropertyFromGameEvent(event, shield));
	}
}

AnimationType PlayerWalkState::GetAnimationType(StateMachineComponent* pStateMachine)
{
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);

	AnimationType animationType;
	if (shield && comEquipmentOwner) {
		if (comEquipmentOwner->equipments[(UINT)EquipmentType::Shield]) {
			animationType = AnimationType::OneHandBaseShieldWalkForward;
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
		else if (comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
			animationType = AnimationType::TwoHandBlockWalkForward;
			if (direction.x == 1 && direction.z == 1) {
				animationType = AnimationType::TwoHandBlockWalkForwardRight45;
			}
			else if (direction.x == -1 && direction.z == 1) {
				animationType = AnimationType::TwoHandBlockWalkForwardLeft45;
			}
			else if (direction.x == 1 && direction.z == -1) {
				animationType = AnimationType::TwoHandBlockWalkBackwardRight45;
			}
			else if (direction.x == -1 && direction.z == -1) {
				animationType = AnimationType::TwoHandBlockWalkBackwardLeft45;
			}
			else if (direction.x == 1) {
				animationType = AnimationType::TwoHandBlockWalkRight;
			}
			else if (direction.x == -1) {
				animationType = AnimationType::TwoHandBlockWalkLeft;
			}
			else if (direction.z == 1) {
				animationType = AnimationType::TwoHandBlockWalkForward;
			}
			else if (direction.z == -1) {
				animationType = AnimationType::TwoHandBlockWalkBackward;
			}
			return animationType;
		}
	}
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		animationType = AnimationType::TwoHandWalkForward;
		if (direction.x == 1 && direction.z == 1) {
			animationType = AnimationType::TwoHandWalkForwardRight45;
		}
		else if (direction.x == -1 && direction.z == 1) {
			animationType = AnimationType::TwoHandWalkForwardLeft45;
		}
		else if (direction.x == 1 && direction.z == -1) {
			animationType = AnimationType::TwoHandWalkBackwardRight45;
		}
		else if (direction.x == -1 && direction.z == -1) {
			animationType = AnimationType::TwoHandWalkBackwardLeft45;
		}
		else if (direction.x == 1) {
			animationType = AnimationType::TwoHandWalkRight;
		}
		else if (direction.x == -1) {
			animationType = AnimationType::TwoHandWalkLeft;
		}
		else if (direction.z == 1) {
			animationType = AnimationType::TwoHandWalkForward;
		}
		else if (direction.z == -1) {
			animationType = AnimationType::TwoHandWalkBackward;
		}
		return animationType;
	}
	animationType = AnimationType::WalkForward;
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
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		twoHand = true;
	}
	else {
		twoHand = false;
	}
	direction = property.dir;

	// TO DO : change speed by status
	pStateMachine->SetOwnerAnimation(GetAnimationType(), 1.5f);
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

void PlayerRollState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	// TO DO : Implement to ignore damages in specific period
}

AnimationType PlayerRollState::GetAnimationType()
{
	
	AnimationType animationType;
	if (twoHand) {
		animationType = AnimationType::TwoHandRollForward;
		if (direction.x == 1 && direction.z == 1) {
			animationType = AnimationType::TwoHandRollForwardRight45;
		}
		else if (direction.x == -1 && direction.z == 1) {
			animationType = AnimationType::TwoHandRollForwardLeft45;
		}
		else if (direction.x == 1 && direction.z == -1) {
			animationType = AnimationType::TwoHandRollBackwardRight45;
		}
		else if (direction.x == -1 && direction.z == -1) {
			animationType = AnimationType::TwoHandRollBackwardLeft45;
		}
		else if (direction.x == 1) {
			animationType = AnimationType::TwoHandRollRight;
		}
		else if (direction.x == -1) {
			animationType = AnimationType::TwoHandRollLeft;
		}
		else if (direction.z == 1) {
			animationType = AnimationType::TwoHandRollForward;
		}
		else if (direction.z == -1) {
			animationType = AnimationType::TwoHandRollBackward;
		}
	}
	else {
		animationType = AnimationType::RollForward;
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
	}
	return animationType;
}

void PlayerAttackState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	currentComboCount = 0;

	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		maxComboCount = MAX_TwoHand_COMBO_COUNT;
	}
	else {
		maxComboCount = MAX_OneHand_COMBO_COUNT;
	}
	pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
}

void PlayerAttackState::Leave(StateMachineComponent* pStateMachine)
{
	currentComboCount = 0;
	doNextCombo = false;
	fatalAttack = false;
}

void PlayerAttackState::Update(StateMachineComponent* pStateMachine)
{
	if (pStateMachine->CheckAnimationEnd()) {
		fatalAttack = false;
		if (doNextCombo) {
			currentComboCount = (currentComboCount + 1) % maxComboCount;
			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));

			doNextCombo = false;
		}
		else {
			pStateMachine->ChangeState(StateType::Idle, StateProperty());
		}
	}
}

void PlayerAttackState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (playerInput.type == InputType::Attack) {
		doNextCombo = true;
	}
	if (pStateMachine->CheckAnimationEnd() && !doNextCombo) {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerAttackState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	if (event.eventType == GameEventType::ActiveFatalAttack) {
		if (pStateMachine->IsAnimationStarting()) {
			currentComboCount = 0;
			doNextCombo = false;
			fatalAttack = true;
			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
		}
	}
	// TO DO : Implement to do not enter to Hit state in specific status
	else if (event.eventType == GameEventType::Hit) {
		event.strength = event.strength;
		pStateMachine->ChangeState(StateType::Hit, createHitStatePropertyFromGameEvent(event, false));
	}
	else if (event.eventType == GameEventType::Parried) {
		pStateMachine->ChangeState(StateType::Parried, StateProperty());
	}
}

AnimationType PlayerAttackState::GetAnimationType(StateMachineComponent* pStateMachine)
{
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner) {

		if (comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) 
		{
			if (fatalAttack) {
				return AnimationType::TwoHandBaseFatalAttack;
			}
			return AnimationType((UINT)AnimationType::TwoHandBaseAttackTypeB_1 + (currentComboCount));
		}
		else if (comEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword]) 
		{
			if (fatalAttack) {
				return AnimationType::OneHandBaseFatalAttack;
			}
			return AnimationType((UINT)AnimationType::OneHandBaseAttackTypeC_1 + (currentComboCount));
		}
	}
	return AnimationType::OneHandBaseAttackTypeC_1;
}

void PlayerParryState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	// TO DO: change Parry speed by a shield type
	pStateMachine->SetOwnerAnimation(AnimationType::OneHandBaseShieldParryType1, 1.2f);
}

void PlayerParryState::Leave(StateMachineComponent* pStateMachine)
{
}

void PlayerParryState::Update(StateMachineComponent* pStateMachine)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeState(StateType::Idle, StateProperty());
	}
}

void PlayerParryState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerParryState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	// TO DO : Implement to ignore Hit in specific period
}


void PlayerHitState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		twoHand = true;
	}
	else {
		twoHand = false;
	}
	direction = property.dir;
	strength = property.strength;
	shield = property.shield;
	if (strength >= FATATL_HIT_STRENGTH) {
		fatalHit = true;
	}
	pStateMachine->SetOwnerAnimation(GetAnimationType());
}

void PlayerHitState::Leave(StateMachineComponent* pStateMachine)
{
	direction = { 0, 0, 1 };
	strength = 0.0f;
	fatalHit = false;
	shield = false;
}

void PlayerHitState::Update(StateMachineComponent* pStateMachine)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeState(StateType::Idle, StateProperty());
	}
}

void PlayerHitState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerHitState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	// TO DO : Implement to ignore damage in specific period
}

AnimationType PlayerHitState::GetAnimationType()
{
	if (twoHand && shield) {
		return AnimationType::TwoHandBlockHit;
	}
	if (fatalHit) {
		return AnimationType::OneHandBaseFatalHit;
	}
	if (direction.y > 0) {
		return AnimationType::HitFrontUp;
	}
	if (direction.y < 0) {
		return AnimationType::HitFrontDown;
	}
	UINT strengthIndex = 0 + (strength >= MIDDLE_HIT_STRENGTH) + (strength >= HIGH_HIT_STRENGTH);
	if (direction.z < 0) {
		return AnimationType((UINT)AnimationType::HitBackType1 + strengthIndex);
	}

	if (shield)
		return AnimationType::OneHandBaseShieldHitType1;
	
	return AnimationType((UINT)AnimationType::HitFrontType1 + strengthIndex);
}

void PlayerParriedState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	pStateMachine->SetOwnerAnimation(AnimationType::Parried, 0.3f);
}

void PlayerParriedState::Leave(StateMachineComponent* pStateMachine)
{
}

void PlayerParriedState::Update(StateMachineComponent* pStateMachine)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeState(StateType::Idle, StateProperty());
	}
}

void PlayerParriedState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
	if (pStateMachine->CheckAnimationEnd()) {
		pStateMachine->ChangeStateByInput(playerInput);
	}
}

void PlayerParriedState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	// TO DO : Implement to ignore Hit in specific period
	if (event.eventType == GameEventType::Hit) {
		event.strength = FATATL_HIT_STRENGTH;
		pStateMachine->ChangeState(StateType::Hit, createHitStatePropertyFromGameEvent(event, false));
	}
}

void EnemyIdleState::Enter(StateMachineComponent* pStateMachine, StateProperty stateProperty)
{
	bActive = stateProperty.active;
	auto hOwner = pStateMachine->GetOwner();
	const auto comEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	if (comEquipmentOwner
		&& comEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword]) {
		pStateMachine->SetOwnerAnimation(AnimationType::TwoHandIdle);
		return;
	}
	pStateMachine->SetOwnerAnimation(AnimationType::Idle);
}

void EnemyIdleState::Leave(StateMachineComponent* pStateMachine)
{
}

void EnemyIdleState::Update(StateMachineComponent* pStateMachine)
{
	auto& player = WORLD.GetComponents<PlayerComponent>().front();

	if (bActive) {
		bool checkResult = pStateMachine->CheckDistAndSetToFaceOpponent(player.GetOwner(), ATTACK_DISTANCE);

		if (checkResult) {
			pStateMachine->ChangeState(StateType::Pattern, StateProperty());
		}
		else {
			pStateMachine->ChangeState(StateType::Walk, StateProperty());
		}
	}
}

void EnemyIdleState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{

}

void EnemyIdleState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	if (event.enemyEventType == EnemyEventType::Active) {
		bActive = true;
	}
	else if(event.enemyEventType == EnemyEventType::Stop) {
		bActive = false;
	}
	if (event.eventType == GameEventType::Hit) {
		pStateMachine->ChangeState(StateType::Hit, createHitStatePropertyFromGameEvent(event, false));
	}
}

void EnemyAttackState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	currentPattern = property.enemyPattern;
	bActive = property.active;
	currentComboCount = 0;
	doNextCombo = false;
	fatalAttack = false;
	auto hOwner = pStateMachine->GetOwner();
	auto pEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	bool twoHand = false;
	bool oneHand = false;

	if (pEquipmentOwner)
	{
		if (pEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword])
		{
			twoHand = true;
		}
		if (pEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword])
		{
			oneHand = true;
		}
	}

	// Combo
	if (currentPattern == EnemyPattern::Combo)
	{
		if (twoHand)
		{
			maxComboCount = MAX_TwoHand_COMBO_COUNT;
		}
		else if (oneHand)
		{
			maxComboCount = MAX_OneHand_COMBO_COUNT;
		}
		else
		{
			maxComboCount = 0;
		}
		currentComboCount = 0;

		if (maxComboCount > 0)
		{
			// 기존 PlayerAttackState의 TwoHand B_1 또는 OneHand C_1 선택
			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
		}
		return;
	}

	// EnemyAttack : OneHand일때만
	if (IsEnemyAttackPattern(currentPattern))
	{
		if (!oneHand || twoHand)
		{
			currentPattern = EnemyPattern::Combo;
			maxComboCount = twoHand? MAX_TwoHand_COMBO_COUNT: MAX_OneHand_COMBO_COUNT;
			currentComboCount = 0;

			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));

			return;
		}

		maxComboCount = 0;

		pStateMachine->SetOwnerAnimation(GetEnemyAttackAnimation(currentPattern));

		return;
	}
}

void EnemyAttackState::Update(StateMachineComponent* pStateMachine)
{
	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	pStateMachine->CheckDistAndSetToFaceOpponent(player.GetOwner(),ATTACK_DISTANCE);

	if (!pStateMachine->CheckAnimationEnd())
	{
		return;
	}

	if (!bActive)
	{
		StateProperty sp;
		sp.active = false;
		pStateMachine->ChangeState(StateType::Idle, sp);
		return;
	}

	if (currentPattern == EnemyPattern::Combo)
	{
		if (maxComboCount > 0 && currentComboCount + 1 < maxComboCount)
		{
			++currentComboCount;
			pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
			return;
		}
		pStateMachine->ChangeState(StateType::Pattern, StateProperty());
		return;
	}

	pStateMachine->ChangeState(StateType::Pattern,StateProperty());
}

void EnemyAttackState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
}

void EnemyAttackState::HandleEvent(StateMachineComponent* pStateMachine, GameEvent& event)
{
	if (event.enemyEventType == EnemyEventType::Stop) {
		bActive = false;
		maxComboCount = 0;
		doNextCombo = false;
	}
	else if (event.eventType == GameEventType::Parried) {
		pStateMachine->ChangeState(StateType::Parried, StateProperty());
	}
}

bool EnemyAttackState::IsEnemyAttackPattern(EnemyPattern pattern) const
{
	switch (pattern)
	{
	case EnemyPattern::EnemyAttack1:
	case EnemyPattern::EnemyAttack2:
	case EnemyPattern::EnemyAttack3:

	case EnemyPattern::EnemyAttack1Offbeat:
	case EnemyPattern::EnemyAttack2Offbeat:
	case EnemyPattern::EnemyAttack3Offbeat:
		return true;

	default:
		return false;
	}
}

AnimationType EnemyAttackState::GetEnemyAttackAnimation(EnemyPattern pattern) const
{
	switch (pattern)
	{
	case EnemyPattern::EnemyAttack1:
		return AnimationType::EnemyAttackType_1;

	case EnemyPattern::EnemyAttack2:
		return AnimationType::EnemyAttackType_2;

	case EnemyPattern::EnemyAttack3:
		return AnimationType::EnemyAttackType_3;

	case EnemyPattern::EnemyAttack1Offbeat:
		return AnimationType::EnemyAttackType_1_Offbeat;

	case EnemyPattern::EnemyAttack2Offbeat:
		return AnimationType::EnemyAttackType_2_Offbeat;

	case EnemyPattern::EnemyAttack3Offbeat:
		return AnimationType::EnemyAttackType_3_Offbeat;

	default:
		return AnimationType::EnemyAttackType_1;
	}
}

void EnemyHitState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
}

void EnemyParriedState::HandleInput(StateMachineComponent* pStateMachine, const PlayerInupt& playerInput)
{
}

void EnemyWalkState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	currentPattern = property.enemyPattern;
	auto now = std::chrono::steady_clock::now();
	moveStartTime = now;
	lastUpdateTime = now;

	switch (currentPattern)
	{
	case EnemyPattern::StrafeLeft:
	case EnemyPattern::StrafeRight:
		moveDuration = GetRandomFloat(MIN_STRAFE_DURATION, MAX_STRAFE_DURATION);
		break;

	case EnemyPattern::BackStep:
		moveDuration = GetRandomFloat(MIN_BACKSTEP_DURATION, MAX_BACKSTEP_DURATION);
		break;

	case EnemyPattern::Wait:
		moveDuration = GetRandomFloat(MIN_WAIT_DURATION, MAX_WAIT_DURATION);

		break;
	default:
		moveDuration = 0.0f;
		break;
	}
	pStateMachine->SetOwnerAnimation(GetAnimationType(pStateMachine));
}

void EnemyWalkState::Leave(StateMachineComponent * pStateMachine)
{
	currentPattern =EnemyPattern::None;

	moveDuration = 0.0f;
}

void EnemyWalkState::Update(StateMachineComponent * pStateMachine)
{
	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	bool checkResult = pStateMachine->CheckDistAndSetToFaceOpponent(player.GetOwner(), ATTACK_DISTANCE);
	if (IsTimedPattern())
	{
		auto currentTime = std::chrono::steady_clock::now();
		float elapsedTime = std::chrono::duration<float>(currentTime - moveStartTime).count();

		if (elapsedTime >= moveDuration)
		{
			pStateMachine->ChangeState(StateType::Pattern, StateProperty());
			return;
		}
		return;
	}
	if (checkResult)
	{
		pStateMachine->ChangeState(StateType::Pattern, StateProperty());
	}
	return;
}

void EnemyWalkState::HandleInput(StateMachineComponent * pStateMachine, const PlayerInupt & playerInput)
{
}

void EnemyWalkState::HandleEvent(StateMachineComponent * pStateMachine, GameEvent & event)
{
	if (event.enemyEventType == EnemyEventType::Stop) {
		StateProperty sp;
		sp.active = false;
		pStateMachine->ChangeState(StateType::Idle, sp);
	}
}

AnimationType EnemyWalkState::GetAnimationType(StateMachineComponent* pStateMachine)
{
	auto hOwner = pStateMachine->GetOwner();
	auto pEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);
	bool twoHand = false;
	bool oneHand = false;

	if (pEquipmentOwner)
	{
		if (pEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword])
		{
			twoHand = true;
		}
		if (pEquipmentOwner->equipments[(UINT)EquipmentType::OneHandSword])
		{
			oneHand = true;
		}
	}
	if (twoHand)
	{
		switch (currentPattern)
		{
		case EnemyPattern::StrafeLeft:
			return AnimationType::TwoHandWalkLeft;

		case EnemyPattern::StrafeRight:
			return AnimationType::TwoHandWalkRight;

		case EnemyPattern::BackStep:
			return AnimationType::TwoHandWalkBackward;

		case EnemyPattern::Wait:
			return AnimationType::TwoHandIdle;

		default:
			return AnimationType::TwoHandWalkForward;
		}
	}
	if (oneHand)
	{
		switch (currentPattern)
		{
		case EnemyPattern::StrafeLeft:
			return AnimationType::WalkLeft;

		case EnemyPattern::StrafeRight:
			return AnimationType::WalkRight;

		case EnemyPattern::BackStep:
			return AnimationType::WalkBackward;

		case EnemyPattern::Wait:
			return AnimationType::Idle;

		default:
			return AnimationType::WalkForward;
		}
	}
	return AnimationType::WalkForward;
}

bool EnemyWalkState::IsTimedPattern() const
{
	switch (currentPattern)
	{
	case EnemyPattern::StrafeLeft:
	case EnemyPattern::StrafeRight:
	case EnemyPattern::BackStep:
	case EnemyPattern::Wait:
		return true;

	default:
		return false;
	}
}

void EnemyPatternState::Enter(StateMachineComponent* pStateMachine, StateProperty property)
{
	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	bool checkResult = pStateMachine->CheckDistAndSetToFaceOpponent(player.GetOwner(), ATTACK_DISTANCE);

	// 공격 가능 범위 밖이면 다시 추적
	if (!checkResult)
	{
		StateProperty sp;
		sp.enemyPattern = EnemyPattern::None;
		pStateMachine->ChangeState(StateType::Walk, sp);

		return;
	}

	EnemyPattern pattern = SelectPattern(pStateMachine);

	ExecutePattern(pStateMachine, pattern);
}

void EnemyPatternState::Leave(StateMachineComponent* pStateMachine)
{}

void EnemyPatternState::Update(StateMachineComponent * pStateMachine)
{}

void EnemyPatternState::HandleInput(StateMachineComponent * pStateMachine, const PlayerInupt & playerInput)
{}

void EnemyPatternState::HandleEvent(StateMachineComponent * pStateMachine, GameEvent & event)
{}

EnemyPattern EnemyPatternState::SelectPattern(StateMachineComponent* pStateMachine)
{
	float distance;
	auto& player = WORLD.GetComponents<PlayerComponent>().front();
	pStateMachine->CheckDistAndSetToFaceOpponent(player.GetOwner(), ATTACK_DISTANCE, &distance);
	
	if (IsWatchingPattern(previousPattern))
	{
		auto attackPatterns = BuildAttackPatterns(pStateMachine, distance);
		EnemyPattern selected = SelectWeightedPattern(attackPatterns);
		if (selected == EnemyPattern::None)
		{
			auto watchingPatterns = BuildWatchingPatterns(distance);
			selected = SelectWeightedPattern(watchingPatterns);
		}

		UpdatePatternHistory(selected);

		return selected;
	}

	if (consecutiveAttackCount >= MAX_CONSECUTIVE_ATTACK_COUNT)
	{
		auto watchingPatterns = BuildWatchingPatterns(distance);
		EnemyPattern selected = SelectWeightedPattern(watchingPatterns);

		UpdatePatternHistory(selected);

		return selected;
	}
	auto patterns = BuildAttackPatterns(pStateMachine, distance);
	DisablePreviousAttack(patterns);
	auto watchingPatterns = BuildWatchingPatterns(distance);

	patterns.insert(patterns.end(),watchingPatterns.begin(), watchingPatterns.end());

	EnemyPattern selected = SelectWeightedPattern(patterns);

	UpdatePatternHistory(selected);

	return selected;
}

EnemyPattern EnemyPatternState::SelectWeightedPattern(const std::vector<PatternWeight>& patterns)
{
	int totalWeight = 0;

	for (const PatternWeight& data : patterns)
	{
		if (data.weight > 0)
		{
			totalWeight += data.weight;
		}
	}

	if (totalWeight <= 0)
	{
		return EnemyPattern::None;
	}

	std::uniform_int_distribution<int> distribution(1, totalWeight);
	int randomValue = distribution(GetEnemyRandomGenerator());
	int accumulatedWeight = 0;

	for (const PatternWeight& data : patterns)
	{
		if (data.weight <= 0)
		{
			continue;
		}

		accumulatedWeight +=
			data.weight;

		if (randomValue <=
			accumulatedWeight)
		{
			return data.pattern;
		}
	}


	return EnemyPattern::None;
}

std::vector<EnemyPatternState::PatternWeight> EnemyPatternState::BuildAttackPatterns(StateMachineComponent* pStateMachine, float distance)
{
	std::vector<PatternWeight> patterns;
	bool canUseEnemyAttack = CanUseEnemyAttack(pStateMachine);

	if (canUseEnemyAttack)
	{
		if (distance <= CLOSE_DISTANCE)
		{
			patterns =
			{
				{ EnemyPattern::EnemyAttack1,        18 },
				{ EnemyPattern::EnemyAttack2,        15 },
				{ EnemyPattern::EnemyAttack3,        12 },

				{ EnemyPattern::EnemyAttack1Offbeat,  8 },
				{ EnemyPattern::EnemyAttack2Offbeat,  7 },
				{ EnemyPattern::EnemyAttack3Offbeat,  5 },

				{ EnemyPattern::Combo,                5 }
			};
		}
		else if (distance <= MID_DISTANCE)
		{
			patterns =
			{
				{ EnemyPattern::EnemyAttack1,        14 },
				{ EnemyPattern::EnemyAttack2,        14 },
				{ EnemyPattern::EnemyAttack3,        13 },

				{ EnemyPattern::EnemyAttack1Offbeat,  9 },
				{ EnemyPattern::EnemyAttack2Offbeat,  8 },
				{ EnemyPattern::EnemyAttack3Offbeat,  7 },

				{ EnemyPattern::Combo,                7 }
			};
		}
		else
		{
			patterns =
			{
				{ EnemyPattern::EnemyAttack1,        10 },
				{ EnemyPattern::EnemyAttack2,        12 },
				{ EnemyPattern::EnemyAttack3,        13 },

				{ EnemyPattern::EnemyAttack1Offbeat,  9 },
				{ EnemyPattern::EnemyAttack2Offbeat,  9 },
				{ EnemyPattern::EnemyAttack3Offbeat,  8 },

				{ EnemyPattern::Combo,                9 }
			};
		}

		DisableUncheckedAttacks(patterns);

		return patterns;
	}

	if (distance <= CLOSE_DISTANCE)
	{
		patterns.push_back({ EnemyPattern::Combo, 50 });
	}
	else if (distance <= MID_DISTANCE)
	{
		patterns.push_back({ EnemyPattern::Combo, 45 });
	}
	else
	{
		patterns.push_back({ EnemyPattern::Combo, 40 });
	}

	return patterns;
}

std::vector<EnemyPatternState::PatternWeight> EnemyPatternState::BuildWatchingPatterns(float distance)
{
	std::vector<PatternWeight> patterns;

	if (distance <= CLOSE_DISTANCE)
	{
		patterns =
		{
			{ EnemyPattern::StrafeLeft,   8 },
			{ EnemyPattern::StrafeRight,  8 },
			// 너무 가까우면 후퇴 확률 증가
			{ EnemyPattern::BackStep,    10 },
			{ EnemyPattern::Wait,         4 }
		};
	}

	if (distance > CLOSE_DISTANCE && distance <= MID_DISTANCE)
	{
		patterns =
		{
			{ EnemyPattern::StrafeLeft,  10 },
			{ EnemyPattern::StrafeRight, 10 },
			{ EnemyPattern::BackStep,     3 },
			{ EnemyPattern::Wait,         7 }
		};
	}
	if (distance > MID_DISTANCE && distance <= ATTACK_DISTANCE)
	{
		patterns =
		{
			{ EnemyPattern::StrafeLeft,  10 },
			{ EnemyPattern::StrafeRight, 10 },
			{ EnemyPattern::BackStep,     0 },
			{ EnemyPattern::Wait,        10 }
		};
	}

	return patterns;
}

void EnemyPatternState::ExecutePattern(StateMachineComponent* pStateMachine, EnemyPattern pattern)
{
	StateProperty sp;
	sp.enemyPattern = pattern;

	if (IsAttackPattern(pattern))
	{
		pStateMachine->ChangeState(StateType::Attack, sp);
		return;
	}

	switch (pattern)
	{
	case EnemyPattern::StrafeLeft:
	case EnemyPattern::StrafeRight:
	case EnemyPattern::BackStep:
	case EnemyPattern::Wait:
		pStateMachine->ChangeState(StateType::Walk, sp);
		break;
	default:
		pStateMachine->ChangeState(StateType::Walk, StateProperty());
		break;
	}
}

void EnemyPatternState::UpdatePatternHistory(EnemyPattern pattern)
{
	previousPattern = pattern;

	// Combo 자체가 긴 공격 패턴이므로
	// 끝난 다음에는 반드시 간보기 행동이 나오게 함
	if (pattern == EnemyPattern::Combo)
	{
		consecutiveAttackCount = MAX_CONSECUTIVE_ATTACK_COUNT;
		return;
	}

	if (IsAttackPattern(pattern))
	{
		++consecutiveAttackCount;
	}
	else
	{
		consecutiveAttackCount = 0;
	}
}

void EnemyPatternState::DisablePreviousAttack(std::vector<PatternWeight>& patterns) const
{
	if (!IsAttackPattern(previousPattern))
	{
		return;
	}

	for (PatternWeight& data : patterns)
	{
		if (data.pattern == previousPattern)
		{
			data.weight = 0;
			break;
		}
	}
}

void EnemyPatternState::DisableUncheckedAttacks(std::vector<PatternWeight>& patterns) const
{
	for (PatternWeight& data : patterns)
	{
		if (IsAttackPattern(data.pattern) && !attackPatternSettings.IsEnabled(data.pattern))
		{
			data.weight = 0;
		}
	}
}

bool EnemyPatternState::CanUseEnemyAttack(StateMachineComponent* pStateMachine)
{
	auto hOwner = pStateMachine->GetOwner();
	auto pEquipmentOwner = WORLD.GetComponent<EquipmentOwnerComponent>(hOwner);

	if (!pEquipmentOwner)
	{
		return false;
	}

	if (pEquipmentOwner->equipments[(UINT)EquipmentType::TwoHandSword])
	{
		return false;
	}

	return true;
}

bool EnemyPatternState::IsAttackPattern(EnemyPattern pattern) const
{
	switch (pattern)
	{
	case EnemyPattern::EnemyAttack1:
	case EnemyPattern::EnemyAttack2:
	case EnemyPattern::EnemyAttack3:

	case EnemyPattern::EnemyAttack1Offbeat:
	case EnemyPattern::EnemyAttack2Offbeat:
	case EnemyPattern::EnemyAttack3Offbeat:

	case EnemyPattern::Combo:

		return true;

	default:
		return false;
	}
}

bool EnemyPatternState::IsWatchingPattern(EnemyPattern pattern) const
{
	switch (pattern)
	{
	case EnemyPattern::StrafeLeft:
	case EnemyPattern::StrafeRight:
	case EnemyPattern::BackStep:
	case EnemyPattern::Wait:
		return true;

	default:
		return false;
	}
}

