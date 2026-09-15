#pragma once

#include "../Model/Animation.h"
#include <unordered_map>

namespace AnimationAudio
{
    // Seconds on each animation timeline. Change these values to move the cue.
    // These defaults match the beginning of each animation's effective hit window.
    inline const std::unordered_map<AnimationType, float> SwordSlashTimePositions = {
        { AnimationType::OneHandBaseAttackTypeC_1, 0.2f },
        { AnimationType::OneHandBaseAttackTypeC_2, 0.366666f},
        { AnimationType::OneHandBaseAttackTypeC_3, 0.1f},
        { AnimationType::OneHandBaseAttackTypeC_4, 0.3f},
        { AnimationType::OneHandBaseAttackTypeC_5, 0.333333f},
        { AnimationType::OneHandBaseAttackTypeC_6, 0.633333f},
        { AnimationType::OneHandBaseAttackTypeC_7, 0.466666f},
        { AnimationType::TwoHandBaseAttackTypeB_1, 0.6f},
        { AnimationType::TwoHandBaseAttackTypeB_2, 0.433333f},
        { AnimationType::TwoHandBaseAttackTypeB_3, 0.633333f},
        { AnimationType::EnemyAttackType_1, 0.333333f},
        { AnimationType::EnemyAttackType_1_Offbeat, 0.766667f },
        { AnimationType::EnemyAttackType_2, 0.3f},
        { AnimationType::EnemyAttackType_2_Offbeat, 0.600000f },
        { AnimationType::EnemyAttackType_3, 0.6f},
        { AnimationType::EnemyAttackType_3_Offbeat, 1.066667f },
    };

    // Fatal-hit cues can be delayed independently from ordinary sword swings.
    inline const std::unordered_map<AnimationType, float> FatalHitTimePositions = {
        { AnimationType::OneHandBaseFatalHit, 0.63f },
        { AnimationType::TwoHandBaseFatalHit, 0.63f },
    };
}
