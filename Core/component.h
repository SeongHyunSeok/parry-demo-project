#pragma once

#include "event.h"
#include "id_manager.h"
#include "utils.h"
#include <cstdint>

class GameObject;

class Component : public WithId<Component> {
protected:
  Handle<GameObject> owner;

  Component(Handle<GameObject> owner) : owner{owner}, WithId{} {}

public:
  virtual EventPropagation HandleEvent(Event &event) {
    return EventPropagation::PROPAGATE;
  };
  inline Handle<GameObject> GetOwner() const { return owner; }
};

class WithHP : public Component {
private:
  uint32_t hp;

public:
  WithHP(Handle<GameObject> owner, uint32_t hp) : Component{owner}, hp{hp} {}
  WithHP(Handle<GameObject> owner) : WithHP{owner, 0} {}
};