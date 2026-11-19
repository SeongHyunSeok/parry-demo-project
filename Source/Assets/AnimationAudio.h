#pragma once

#include "../Model/Animation.h"
#include <unordered_map>

namespace AnimationAudio
{
    // Fatal-hit cues can be delayed independently from ordinary sword swings.
    inline const std::unordered_map<AnimationType, float> FatalHitTimePositions = {
        { AnimationType::OneHandBaseFatalHit, 0.63f },
        { AnimationType::TwoHandBaseFatalHit, 0.63f },
    };
}
