#pragma once

#include "id.h"
#include <cstdint>
#include <deque>

template <typename T> class IdManager {
private:
  uint64_t last_id = 1;
  std::deque<Id<T>> free_ids;
  IdManager() = default;

public:
  IdManager(const IdManager &) = delete;
  IdManager &operator=(const IdManager &) = delete;
  IdManager(IdManager &&) = delete;
  IdManager &operator=(IdManager &&) = delete;

  static IdManager &GetInstance() {
    static IdManager instance;
    return instance;
  }

  Id<T> CreateId() {
    if (free_ids.empty()) {
      auto id = last_id;
      ++last_id;
      return Id<T>{0, id};
    }
    auto id = std::move(this->free_ids.front());
    free_ids.pop_front();
    id.generation += 1;
    return id;
  }

  void RecycleId(Id<T> &&id) { this->free_ids.emplace_back(std::move(id)); }
};

template <typename T> struct WithId {
  Id<T> id;

  WithId() : id{IdManager<T>::GetInstance().CreateId()} {}
  WithId(const WithId &) = delete;
  WithId &operator=(const WithId &) = delete;
  WithId(WithId &&other) : id{std::move(other.id)} {}
  WithId &operator=(WithId &&other) {
    id = std::move(other.id);
    return *this;
  }
  ~WithId() {
    if (id)
      IdManager<T>::GetInstance().RecycleId(std::move(id));
  }
};