#pragma once

#include "../component.h"

class StateMachineComponent : public Component
{
public:
	StateMachineComponent(Handle<GameObject> owner) : Component{ owner } {};
	StateMachineComponent(const StateMachineComponent& rhs) = delete;
	StateMachineComponent(StateMachineComponent&&) = default;


};

class State
{
public:
	virtual void Enter() = 0;
	virtual void Leave() = 0;
	virtual void Update() = 0;
	virtual void OnNext() = 0;
};

class PlayerIdleState : public State
{
public:
	void Enter() {}
	void Update() {}
};

