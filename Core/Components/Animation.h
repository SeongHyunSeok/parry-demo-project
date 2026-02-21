#pragma once

#include "../d3dUtil.h"
#include "../component.h"
#include "../../Model/Animation.h"

struct AnimationComponent : public Component {
    AnimationComponent(Handle<GameObject> owner) : Component{ owner } {};
    AnimationComponent(const AnimationComponent& rhs) = delete;
    AnimationComponent(AnimationComponent&&) = default;

    // Only applicable to skinned render-items.
    UINT SkinnedCBIndex = -1;

    // nullptr if this render-item is not animated by skinned mesh.
    std::unique_ptr<AnimationController> mSkinnedModelAnimationController;
};

struct NonAnimatedComponent : public Component {
    NonAnimatedComponent(Handle<GameObject> owner) : Component{ owner } {};
    NonAnimatedComponent(const NonAnimatedComponent& rhs) = delete;
    NonAnimatedComponent(NonAnimatedComponent&&) = default;
};
