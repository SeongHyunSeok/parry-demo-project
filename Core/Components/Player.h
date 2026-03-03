#pragma once

#include "../component.h"
#include "../world.h"
#include "Animation.h"
#include "StateMachine.h"

class PlayerComponent : public Component
{
public:
    PlayerComponent(Handle<GameObject> owner) : Component{ owner } {};
    PlayerComponent(const PlayerComponent& rhs) = delete;
    PlayerComponent(PlayerComponent&&) = default;

    void Input(const PlayerInupt& playerInput) {
        auto pComStateMachine = WORLD.GetComponent<StateMachineComponent>(owner);
        if (pComStateMachine) {
            pComStateMachine->HandleInput(playerInput);
        }
    }

    void Update(float dt) {
        auto pComStateMachine = WORLD.GetComponent<StateMachineComponent>(owner);
        if (pComStateMachine) {
            pComStateMachine->Update();
        }
    }
};
