#pragma once

#include <cstdint>

template <typename T> struct Id {
	uint64_t generation : 8;
	uint64_t id : 56;

	Id() : Id{ 0, 0 } {}
	Id(uint8_t generation, uint64_t id) : generation(generation), id{ id } {}
	Id(const Id&) = default;
	Id(Id&& other) : generation(other.generation), id{ other.id } {
		other.generation = 0;
		other.id = 0;
	}
	Id& operator=(const Id& other) = default;
	Id& operator=(Id&& other) {
		this->generation = other.generation;
		this->id = other.id;
		other.generation = 0;
		other.id = 0;
		return *this;
	}

	operator bool() const { return bool(this->generation | this->id); }
	bool operator<(const Id<T>& other) const {
		if (generation < other.generation) return true;
		if (generation == other.generation && id < other.id) return true;
		return false;
	}
	bool operator>(const Id<T>& other) const {
		if (generation > other.generation) return true;
		if (generation == other.generation && id > other.id) return true;
		return false;
	}
	bool operator==(const Id<T>& other) const {
		return (generation == other.generation) && (id == other.id);
	}
};
