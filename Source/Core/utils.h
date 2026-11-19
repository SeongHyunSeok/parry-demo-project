#pragma once

#include "id_manager.h"
#include <cassert>
#include <concepts>
#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

template <typename T> class Handle {
public:
	size_t idx;
	Id<T> real_id;

	Handle() : Handle{ 0, Id<T>{} } {}
	Handle(size_t idx, Id<T> real_id) : idx{ idx }, real_id{ real_id } {}
	Handle(const Handle&) = default;
	Handle(Handle&&) = default;

	Handle& operator=(const Handle&) = default;
	Handle& operator=(Handle&&) = default;

	bool operator<(const Handle& other) const {
		if (idx < other.idx) return true;
		if (idx == other.idx && real_id < other.real_id) return true;
		return false;
	};
	bool operator>(const Handle& other) const {
		if (idx > other.idx) return true;
		if (idx == other.idx && real_id > other.real_id) return true;
		return false;
	};
	bool operator==(const Handle& other) const {
		return (idx == other.idx) && (real_id == other.real_id);
	};

	//template<typename U, typename H>
	//	requires std::derived_from<U, T>&& std::same_as<std::remove_cvref_t<H>, Handle<T>>
	//inline Handle<U> Convert(H&& child) {
	//	return Handle{ child.idx, std::forward(reinterpret_cast<Id<T>>(childId.real_id)) }
	//}
};

template <class CommonT>
class AnyVector {
protected:
	AnyVector() = default;
	AnyVector(const AnyVector&) = default;
	AnyVector(AnyVector&&) = default;
	AnyVector& operator=(const AnyVector&) = default;
	AnyVector& operator=(AnyVector&&) = default;

public:
	virtual CommonT* GetCommon(Handle<CommonT> handle) = 0;
	virtual const CommonT* GetCommon(Handle<CommonT> handle) const = 0;
};

template <typename T, typename Base = T>
	requires std::derived_from<T, Base>&& std::derived_from<Base, WithId<Base>>
class DenseTable : public AnyVector<Base> {
private:
	using HandleIdx = size_t;
	using EntityIdx = size_t;
	// HandleIdx -> EntityIdx
	std::vector<EntityIdx> handleToEntity;
	std::deque<HandleIdx> recycledIndices;
	// EntityIdx -> HandleIdx
	std::vector<HandleIdx> indices;
	// EntityIdx -> T
	std::vector<T> entities;

public:
	template <typename... Args>
	std::pair<Handle<Base>, std::reference_wrapper<T>> Emplace(Args... args) {
		auto idx = entities.size();
		auto& entity = entities.emplace_back(std::forward<Args>(args)...);
		HandleIdx handle_idx = 0;
		if (!recycledIndices.empty()) {
			handle_idx = recycledIndices.front();
			recycledIndices.pop_front();
			handleToEntity[handle_idx] = idx;
		}
		else {
			handle_idx = handleToEntity.size();
			handleToEntity.emplace_back(idx);
		}
		indices.emplace_back(handle_idx);
		return std::make_pair(Handle{ handle_idx, entity.id }, std::ref(entity));
	}

	inline T* Get(Handle<Base> handle) {
		if (handleToEntity.size() <= handle.idx) {
			return nullptr;
		}
		auto entityIdx = handleToEntity[handle.idx];
		if (entities.size() <= entityIdx) {
			assert("TwoLayerVec is in invalid state");
			return nullptr;
		}
		auto& entity = entities[entityIdx];
		if (entity.id != handle.real_id) {
			return nullptr;
		}
		return &entity;
	}

	inline const T* Get(Handle<Base> handle) const {
		if (handleToEntity.size() <= handle.idx) {
			return nullptr;
		}
		auto entityIdx = handleToEntity[handle.idx];
		if (entities.size() <= entityIdx) {
			assert("TwoLayerVec is in invalid state");
			return nullptr;
		}
		auto& entity = entities[entityIdx];
		if (entity.id != handle.real_id) {
			return nullptr;
		}
		return &entity;
	}

	inline Base* GetCommon(Handle<Base> handle) override {
		return Get(handle);
	}

	inline const Base* GetCommon(Handle<Base> handle) const override {
		return Get(handle);
	}

	std::optional<T> Erase(Handle<Base> handle) {
		if (handleToEntity.size() <= handle.idx) {
			return {};
		}
		auto entityIdx = handleToEntity[handle.idx];
		if (entities.size() <= entityIdx) {
			assert("TwoLayerVec is in invalid state");
			return {};
		}
		if (entities[entityIdx].id != handle.real_id) {
			return {};
		}

		auto entity = std::move(entities[entityIdx]);
		handleToEntity[handle.idx] = -1;
		recycledIndices.emplace_back(handle.idx);

		if (entities.size() == (entityIdx + 1)) {
			entities.pop_back();
			indices.pop_back();
			return entity;
		}

		//std::swap(entities[entityIdx], entities.back());
		//entities.pop_back();
		//std::swap(indices[entityIdx], indices.back());
		//indices.pop_back();

		entities[entityIdx].~T();
		new (&entities[entityIdx]) T(std::move(entities.back()));
		entities.pop_back();

		std::swap(indices[entityIdx], indices.back());
		indices.pop_back();

		handleToEntity[indices[entityIdx]] = entityIdx;
		return entity;
	}

	inline std::vector<T>& Entities() { return entities; }
	inline const std::vector<T>& Entities() const { return entities; }
};