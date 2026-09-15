#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

enum class EnemyPattern : unsigned int
{
	None,
	EnemyAttack1,
	EnemyAttack2,
	EnemyAttack3,
	EnemyAttack1Offbeat,
	EnemyAttack2Offbeat,
	EnemyAttack3Offbeat,
	Combo,
	StrafeLeft,
	StrafeRight,
	BackStep,
	Wait
};

inline bool IsOneHandOnlyPattern(EnemyPattern pattern)
{
	return pattern >= EnemyPattern::EnemyAttack1 && pattern <= EnemyPattern::EnemyAttack3Offbeat;
}

inline const char* GetEnemyPatternLabel(EnemyPattern pattern)
{
	switch (pattern)
	{
	case EnemyPattern::EnemyAttack1: return "Attack 1";
	case EnemyPattern::EnemyAttack2: return "Attack 2";
	case EnemyPattern::EnemyAttack3: return "Attack 3";
	case EnemyPattern::EnemyAttack1Offbeat: return "Attack 1 Offbeat";
	case EnemyPattern::EnemyAttack2Offbeat: return "Attack 2 Offbeat";
	case EnemyPattern::EnemyAttack3Offbeat: return "Attack 3 Offbeat";
	case EnemyPattern::Combo: return "Combo";
	case EnemyPattern::StrafeLeft: return "Strafe Left";
	case EnemyPattern::StrafeRight: return "Strafe Right";
	case EnemyPattern::BackStep: return "Back Step";
	case EnemyPattern::Wait: return "Wait";
	default: return "None";
	}
}

struct EnemyPatternEntry
{
	int id;
	EnemyPattern pattern;
	bool operator==(const EnemyPatternEntry&) const = default;
};

struct EnemyPatternSequenceSettings
{
	std::vector<EnemyPatternEntry> entries;
	bool useFixedOrder = false;
	bool repeat = true;
	std::uint64_t revision = 0;
};

inline int CountEnemyPatternEntries(
	const EnemyPatternSequenceSettings& settings, EnemyPattern pattern)
{
	int count = 0;
	for (const auto& entry : settings.entries)
	{
		if (entry.pattern == pattern)
			++count;
	}
	return count;
}

inline int ApplyEnemyPatternEntryWeight(
	const EnemyPatternSequenceSettings& settings, EnemyPattern pattern, int baseWeight)
{
	// Missing patterns become unavailable; duplicate rows multiply the original AI weight.
	return baseWeight * CountEnemyPatternEntries(settings, pattern);
}

inline bool HasUsableEnemyPatternEntries(
	const EnemyPatternSequenceSettings& settings, bool oneHandEnemy)
{
	return std::any_of(settings.entries.begin(), settings.entries.end(),
		[oneHandEnemy](const EnemyPatternEntry& entry)
		{
			return entry.pattern > EnemyPattern::None
				&& entry.pattern <= EnemyPattern::Wait
				&& (oneHandEnemy || !IsOneHandOnlyPattern(entry.pattern));
		});
}

// Settings may be shared; playback position belongs to each enemy.
class EnemyPatternSequenceCursor
{
public:
	bool Sync(const EnemyPatternSequenceSettings& settings)
	{
		if (revision == settings.revision)
			return false;
		revision = settings.revision;
		nextIndex = 0;
		return true;
	}

	EnemyPattern Peek(const EnemyPatternSequenceSettings& settings) const
	{
		if (settings.entries.empty())
			return EnemyPattern::None;
		if (nextIndex >= settings.entries.size())
			return settings.repeat ? settings.entries.front().pattern : EnemyPattern::None;
		return settings.entries[nextIndex].pattern;
	}

	void Advance(const EnemyPatternSequenceSettings& settings)
	{
		if (Peek(settings) == EnemyPattern::None)
			return;
		nextIndex = nextIndex >= settings.entries.size() ? 1 : nextIndex + 1;
	}

private:
	std::size_t nextIndex = 0;
	std::uint64_t revision = 0;
};

// Destination is a gap in the original list: 0 is before the first item,
// entries.size() is after the last. IDs remain stable across moves.
inline bool MoveEnemyPatternEntry(std::vector<EnemyPatternEntry>& entries, int id, std::size_t destination)
{
	if (destination > entries.size())
		return false;
	const auto source = std::find_if(entries.begin(), entries.end(),
		[id](const EnemyPatternEntry& entry) { return entry.id == id; });
	if (source == entries.end())
		return false;
	const auto sourceIndex = static_cast<std::size_t>(source - entries.begin());
	if (destination == sourceIndex || destination == sourceIndex + 1)
		return false;
	const auto entry = *source;
	entries.erase(source);
	if (sourceIndex < destination)
		--destination;
	entries.insert(entries.begin() + destination, entry);
	return true;
}
