#pragma once

#include "Animation.h"
#include "../world.h"

enum class MoveType : UINT {
    Walk = 0,
    Run
};

enum class MoveDirection : UINT {
    Forward,
    ForwordLeft45,
    ForwordRight45,
    Left,
    Right,
    Backword,
    BackwordLeft45,
    BackwordRight45
};

class PlayerComponent : public Component
{
public:
    PlayerComponent(Handle<GameObject> owner) : Component{ owner } {};
    PlayerComponent(const PlayerComponent& rhs) = delete;
    PlayerComponent(PlayerComponent&&) = default;

    void Move(MoveType moveType, MoveDirection dir) {
        if (moveType == MoveType::Walk) {
            switch (dir) {
            case MoveDirection::Forward: {
                isMoving = SetAnimationIfPossible(AnimationType::WalkForward);
                break;
            }
            }
        }
        else if (moveType == MoveType::Run) {

        }
        moveInputIn = isMoving;
    }

    void UpdateInput() {
        if (isMoving && !moveInputIn) {
            SetAnimationIfPossible(AnimationType::Idle);
            isMoving = false;
        }
        moveInputIn = false;
    }
private:
    bool SetAnimationIfPossible(AnimationType type) {
        auto pComAni = WORLD.GetComponent<AnimationComponent>(owner);

        if (pComAni) {
            return pComAni->mSkinnedModelAnimationController->ChangeAnimationByUserInput(type);
        }
        return false;
    };
private:
    bool isMoving = false;
    bool moveInputIn = false;
};
