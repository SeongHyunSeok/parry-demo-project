#pragma once

#include "component.h"
#include "event.h"
#include "id_manager.h"

#include <concepts>
#include <optional>
#include <set>
#include <type_traits>
#include <typeindex>
#include <unordered_map>


class GameWorld;

class GameObject : public WithId<GameObject> {
protected:
	Handle<GameObject> handle;
	std::unordered_map<std::type_index, Handle<Component>> components;
	std::optional<Handle<GameObject>> parent;
	std::set<Handle<GameObject>> children;

public:
	GameObject() : WithId{} {}

	void SetHandle(Handle<GameObject> handle) { this->handle = handle; }
	Handle<GameObject> GetHandle() const { return handle; }

	template <typename T>
		requires (std::derived_from<std::remove_cvref_t<T>, Component> && !std::same_as< std::remove_cvref_t<T>, Component>)
	bool AttachComponent(Handle<Component> component) {
		Handle<Component>* fakeHandle =
			reinterpret_cast<Handle<Component> *>(&component);
		auto [_it, result] = components.try_emplace(
			std::type_index{ typeid(std::remove_cvref_t<T>) }, *fakeHandle);
		return result;
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	std::optional<Handle<Component>> DetachComponent() {
		auto it = components.find(std::type_index{ typeid(std::remove_cvref_t<T>) });
		if (it == components.end()) {
			return {};
		}
		auto [_, handle] = *it;
		components.erase(it);
		Handle<T>* concreteHandle = reinterpret_cast<Handle<T> *>(&handle);
		return *concreteHandle;
	}

	template <typename T>
		requires (std::derived_from<std::remove_cvref_t<T>, Component> && !std::same_as< std::remove_cvref_t<T>, Component>)
	inline std::optional<Handle<Component>> GetComponent() const {
		auto it = components.find(std::type_index{ typeid(std::remove_cvref_t<T>) });
		if (it == components.end()) {
			return {};
		}
		return it->second;
	}

	bool AddChild(GameWorld& world, Handle<GameObject> child);
	const std::set<Handle<GameObject>>& GetChildren() const { return children; }
	bool RemoveChild(GameWorld& world, Handle<GameObject> child);

	void HandleEvent(Event& event);
};