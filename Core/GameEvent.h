#pragma once

enum class GameEventType : unsigned int
{
	None = 0,
	Hit
};

struct GameEvent
{
	GameEventType eventType = GameEventType::None;
};
