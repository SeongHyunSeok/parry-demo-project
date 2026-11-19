#pragma once

#include "d3dUtil.h"

enum class GameEventType : unsigned int
{
	None = 0,
	Hit,
	ActiveFatalAttack,
	Parried,
};

enum class EnemyEventType : unsigned int
{
	None = 0,
	Active,
	Stop,
	Parried,
};

struct GameEvent
{
	GameEventType eventType = GameEventType::None;
	EnemyEventType enemyEventType = EnemyEventType::None;
	float strength = 0.0f;
	DirectX::XMINT3 dir = { 0, 0, 0 };
};

