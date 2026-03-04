#pragma once

#include "d3dUtil.h"

enum class GameEventType : unsigned int
{
	None = 0,
	Hit,
	ActiveFatalAttack,
	Parried,
};

struct GameEvent
{
	GameEventType eventType = GameEventType::None;
	float strength = 0.0f;
	DirectX::XMINT3 dir = { 0, 0, 0 };
};
