#pragma once

#include <string>
#include <unordered_map>
#include "../Model/Animation.h"

namespace HumanoidAssets
{
	const std::string AssetName = "Humanoid";

	const std::string BaseModelFileName = "Models\\HumanoidBaseModel.bin";

	const std::string AnimationFileNames[] = {
		"Models\\HumanoidOneHandAnimation.bin"
	};

	const std::unordered_map<AnimationType, std::string> MoveTypeToName =
	{
		{AnimationType::Idle,"Skeleton|Stand_Idle_A_1"},
		// Walk
		{AnimationType::WalkForward,"Skeleton|Walk_A_F"},
		{AnimationType::WalkForwardRight45,"Skeleton|Walk_A_F_R45"},
		{AnimationType::WalkForwardLeft45,"Skeleton|Walk_A_F_L45"},
		{AnimationType::WalkBackwardRight45,"Skeleton|Walk_A_B_R45"},
		{AnimationType::WalkBackwardLeft45,"Skeleton|Walk_A_B_L45"},
		{AnimationType::WalkRight,"Skeleton|Walk_A_F_R90_A"},
		{AnimationType::WalkLeft,"Skeleton|Walk_A_F_L90_A"},
		{AnimationType::WalkBackward,"Skeleton|Walk_A_B"},
		// Roll
		{AnimationType::RollForward,"Skeleton|Low_Roll_F" },
		{AnimationType::RollForwardRight45,"Skeleton|Low_Roll_F_R45"},
		{AnimationType::RollForwardLeft45,"Skeleton|Low_Roll_F_L45"},
		{AnimationType::RollBackwardRight45,"Skeleton|Low_Roll_B_R45"},
		{AnimationType::RollBackwardLeft45,"Skeleton|Low_Roll_B_L45"},
		{AnimationType::RollRight,"Skeleton|Low_Roll_F_R90"},
		{AnimationType::RollLeft,"Skeleton|Low_Roll_F_L90"},
		{AnimationType::RollBackward,"Skeleton|Low_Roll_B"},
		// Hit
		{ AnimationType::HitBackType1,"Skeleton|Hit_B_3" },
		{ AnimationType::HitBackType2,"Skeleton|Hit_B_1" },
		{ AnimationType::HitBackType3,"Skeleton|Hit_B_2" },
		{ AnimationType::HitFrontType1,"Skeleton|Hit_F_2" },
		{ AnimationType::HitFrontType2,"Skeleton|Hit_F_1" },
		{ AnimationType::HitFrontType3,"Skeleton|Hit_F_3" },
		{ AnimationType::HitFrontDown,"Skeleton|Hit_F_Down" },
		{ AnimationType::HitFrontUp,"Skeleton|Hit_F_Up" },
		// OneHandBaseAttack
		{ AnimationType::OneHandBaseAttackTypeC_1,"Skeleton|OneHand_Base_Attack_C_1" },
		{ AnimationType::OneHandBaseAttackTypeC_2,"Skeleton|OneHand_Base_Attack_C_2" },
		{ AnimationType::OneHandBaseAttackTypeC_3,"Skeleton|OneHand_Base_Attack_C_3" },
		{ AnimationType::OneHandBaseAttackTypeC_4,"Skeleton|OneHand_Base_Attack_C_4" },
		{ AnimationType::OneHandBaseAttackTypeC_5,"Skeleton|OneHand_Base_Attack_C_5" },
		{ AnimationType::OneHandBaseAttackTypeC_6,"Skeleton|OneHand_Base_Attack_C_6" },
		{ AnimationType::OneHandBaseAttackTypeC_7,"Skeleton|OneHand_Base_Attack_C_7" },
		{ AnimationType::OneHandBaseAttackTypeC_8,"Skeleton|OneHand_Base_Attack_C_8" },
		// Parried
		{ AnimationType::Parried,"Skeleton|Action_A_1" },
		// OneHandBaseFatal
		{ AnimationType::OneHandBaseFatalAttack,"Skeleton|OneHand_Base_Fatal_Attack_2" },
		{ AnimationType::OneHandBaseFatalHit,"Skeleton|OneHand_Base_Fatal_Hit_2" },
		// OneHandBaseShield
		{ AnimationType::OneHandBaseShieldStart,"Skeleton|OneHand_Base_Shield_Block_Start_1" },
		{ AnimationType::OneHandBaseShieldIdle,"Skeleton|OneHand_Base_Shield_Block_Idle_1" },
		{ AnimationType::OneHandBaseShieldEnd,"Skeleton|OneHand_Base_Shield_Block_End_1" },
		{ AnimationType::OneHandBaseShieldHitType1,"Skeleton|OneHand_Base_Shield_Block_Hit_1" },
		{ AnimationType::OneHandBaseShieldHitType2,"Skeleton|OneHand_Base_Shield_Block_Hit_2" },
		{ AnimationType::OneHandBaseShieldParryType1,"Skeleton|OneHand_Base_Shield_Block_Parry_2" },
		{ AnimationType::OneHandBaseShieldParryType2,"Skeleton|OneHand_Base_Shield_Block_Parry_3" },
		// OneHandBaseShieldWalk
		{AnimationType::OneHandBaseShieldWalkForward,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_F"},
		{AnimationType::OneHandBaseShieldWalkForwardRight45,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_F_R45"},
		{AnimationType::OneHandBaseShieldWalkForwardLeft45,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_F_L45"},
		{AnimationType::OneHandBaseShieldWalkBackwardRight45,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_B_R45"},
		{AnimationType::OneHandBaseShieldWalkBackwardLeft45,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_B_L45"},
		{AnimationType::OneHandBaseShieldWalkRight,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_F_R90_A"},
		{AnimationType::OneHandBaseShieldWalkLeft,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_F_L90_A"},
		{AnimationType::OneHandBaseShieldWalkBackward,"Skeleton|OneHand_Base_Shield_Walk_Slow_A_B"}
	};

	const std::unordered_map<AnimationType, AnimationEffectiveTimeInfo> AnimationEffectiveTimeInfos = {
		{ AnimationType::OneHandBaseAttackTypeC_1,AnimationEffectiveTimeInfo(0.333333f, 0.400000f)},
		{ AnimationType::OneHandBaseAttackTypeC_2,AnimationEffectiveTimeInfo(0.400000f, 0.533333f) },
		{ AnimationType::OneHandBaseAttackTypeC_3,AnimationEffectiveTimeInfo(0.233333f, 0.366667f) },
		{ AnimationType::OneHandBaseAttackTypeC_4,AnimationEffectiveTimeInfo(0.433333f, 0.533333f) },
		{ AnimationType::OneHandBaseAttackTypeC_5,AnimationEffectiveTimeInfo(0.466667f, 0.566667f) },
		{ AnimationType::OneHandBaseAttackTypeC_6,AnimationEffectiveTimeInfo(0.766667f, 0.866667f) },
		{ AnimationType::OneHandBaseAttackTypeC_7,AnimationEffectiveTimeInfo(0.366667f, 0.500000f) },
		{ AnimationType::OneHandBaseAttackTypeC_8,AnimationEffectiveTimeInfo(0.500000f, 0.633333f) },
		{ AnimationType::OneHandBaseShieldParryType1,AnimationEffectiveTimeInfo(0.333333f, 0.433333f) },
		{ AnimationType::RollBackward,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollBackwardLeft45,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollBackwardRight45,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollForward,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollForwardLeft45,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollForwardRight45,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollLeft,AnimationEffectiveTimeInfo(0.0f, 0.433333f) },
		{ AnimationType::RollRight,AnimationEffectiveTimeInfo(0.0f, 0.433333f) }
	};
}