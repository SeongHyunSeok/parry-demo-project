#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <map>

#include "game_object.h"
#include "utils.h"

class GameWorld {
private:
	DenseTable<GameObject> gameObjects;
	std::map<Handle<Component>, Component*> anyComponents;
	std::unordered_map<std::type_index, std::unique_ptr<AnyVector>> components;

	template <typename T, typename Base = T>
		requires std::derived_from<std::remove_cvref_t<T>, Base> && std::derived_from<std::remove_cvref_t<Base>, WithId< std::remove_cvref_t<Base>>>
	DenseTable<std::remove_cvref_t<T>, Component>& GetComponentTable() {
		using raw_t = std::remove_cvref_t<T>;

		std::type_index index{ typeid(raw_t) };
		if (components.find(index) == components.end()) {
			components.emplace(index,
				std::unique_ptr<AnyVector>(new DenseTable<raw_t, Base>));
		}
		DenseTable<raw_t, Base>& v =
			*reinterpret_cast<DenseTable<raw_t, Base> *>(components[index].get());
		return v;
	}

public:
	inline GameObject* GetGameObject(Handle<GameObject> handle) {
		return gameObjects.Get(handle);
	}
	inline const GameObject* GetGameObject(Handle<GameObject> handle) const {
		return gameObjects.Get(handle);
	}
	inline Handle<GameObject> AddObject() {
		auto [handle, object] = gameObjects.Emplace();
		object.get().SetHandle(handle);
		return handle;
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	std::optional<Handle<Component>>
		AttachComponent(Handle<GameObject> object, T&& component) {
		using raw_t = std::remove_cvref_t<T>;
		auto obj = GetGameObject(object);
		if (!obj) {
			return {};
		}

		auto& v = GetComponentTable<T, Component>();
		auto [handle, ref] = v.Emplace(std::forward<T>(component));
		anyComponents[handle] = &ref.get();
		obj->AttachComponent<T>(handle);
		return handle;
	}
	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	std::optional<T> DetachComponent(Handle<GameObject> object) {
		using raw_t = std::remove_cvref_t<T>;

		auto obj = GetGameObject(object);
		if (!obj) {
			return {};
		}

		auto handle = obj->DetachComponent<raw_t>();
		if (!handle) {
			return {};
		}

		anyComponents.erase(handle);

		auto& v = GetComponentTable<T, Component>();
		return v.Erase(*handle);
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	inline std::remove_cvref_t<T>* GetComponent(Handle<Component> handle) {
		auto& v = GetComponentTable<T, Component>();
		return v.Get(handle);
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	inline std::remove_cvref_t<T>* GetComponent(Handle<GameObject> objHandle) {
		auto pObj = GetGameObject(objHandle);
		if (!pObj) {
			return nullptr;
		}
		auto hComponentOpt = pObj->GetComponent<std::remove_cvref_t<T>>();
		if (!hComponentOpt) {
			return nullptr;
		}
		auto& v = GetComponentTable<T, Component>();
		return v.Get(*hComponentOpt);
	}

	inline Component* GetAnyComponent(Handle<Component> handle) {
		auto it = anyComponents.find(handle);
		if (it == anyComponents.end()) {
			return nullptr;
		}
		return it->second;
	}

	template <typename T>
		requires std::derived_from<std::remove_cvref_t<T>, Component>
	std::vector<std::remove_cvref_t<T>>& GetComponents() {
		auto& v = GetComponentTable<T, Component>();
		return v.Entities();
	}
};

#ifdef WORLD_MAIN
GameWorld WORLD{};
#else
extern GameWorld WORLD;
#endif