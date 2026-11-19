#pragma once

#include "utils.h"
#include <variant>

class GameObject;

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

struct CollidedWith {
  Handle<GameObject> target;
};
struct DetachedWith {
  Handle<GameObject> target;
};

using Event = std::variant<CollidedWith, DetachedWith>;

enum class EventPropagation { PROPAGATE, STOP };
