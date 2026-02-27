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

    void Move(MoveType moveType, XMINT3 dir) {
        if (dir.x == 0 && dir.z == 0) {
            moveInputIn = false;
            return;
        }

        AnimationType animationType = AnimationType::Idle;
        if (moveType == MoveType::Walk) {
            if (dir.x == 1 && dir.z == 1) {
                animationType = AnimationType::WalkForwardRight45;
            }
            else if (dir.x == -1 && dir.z == 1) {
                animationType = AnimationType::WalkForwardLeft45;
            }
            else if (dir.x == 1 && dir.z == -1) {
                animationType = AnimationType::WalkBackwardRight45;
            }
            else if (dir.x == -1 && dir.z == -1) {
                animationType = AnimationType::WalkBackwardLeft45;
            }
            else if (dir.x == 1) {
                animationType = AnimationType::WalkRight;
            }
            else if (dir.x == -1) {
                animationType = AnimationType::WalkLeft;
            }
            else if (dir.z == 1) {
                animationType = AnimationType::WalkForward;
            }
            else if (dir.z == -1) {
                animationType = AnimationType::WalkBackward;
            }
        }
        else if (moveType == MoveType::Run) {

        }
        isMoving = SetAnimationIfPossible(animationType);
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
            pComAni->mSkinnedModelAnimationController->SetAnimation(type);
            return true;
        }
        return false;
    };
private:
    bool isMoving = false;
    bool moveInputIn = false;
};
