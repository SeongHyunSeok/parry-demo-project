#include "game_object.h"
#include "event.h"
#include "world.h"

bool GameObject::AddChild(GameWorld &world, Handle<GameObject> hChild) {
  auto childOpt = world.GetObject(hChild);
  if (!childOpt) {
    return false;
  }

  children.insert(hChild);
  childOpt->parent = this->handle;
  return true;
}

bool GameObject::RemoveChild(GameWorld &world, Handle<GameObject> hChild) {
  auto it = children.find(hChild);
  if (it == children.end()) {
    return false;
  }
  children.erase(it);

  auto childOpt = world.GetObject(hChild);
  if (!childOpt) {
    return false;
  }

  childOpt->parent = {};
  return true;
}

void GameObject::HandleEvent(Event &event) {
  EventPropagation propagation{EventPropagation::PROPAGATE};
  for (auto &hComponent : components) {
    auto pComponent = WORLD.GetAnyComponent(hComponent.second);
    if (!pComponent) {
      continue;
    }
    if (pComponent->HandleEvent(event) == EventPropagation::STOP) {
      propagation = EventPropagation::STOP;
    }
  }

  if (propagation == EventPropagation::PROPAGATE) {
    for (auto &hChild : children) {
      auto child = WORLD.GetObject(hChild);
      if (!child) {
        continue;
      }
      child->HandleEvent(event);
    }
  }
}