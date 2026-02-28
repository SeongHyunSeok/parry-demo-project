#pragma once

#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <cstdint>

#include "Jolt/Math/Vec3.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Body/MotionType.h"
#include "Jolt/Physics/Collision/ObjectLayer.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Jolt/Physics/Character/CharacterVirtual.h"
#include "Jolt/Physics/EActivation.h"
#include "component.h"
#include "event.h"

namespace physics {
using namespace JPH;
using namespace JPH::literals;
using namespace std;

namespace Layers {
static constexpr ObjectLayer NON_MOVING = 0;
static constexpr ObjectLayer MOVING = 1;
static constexpr ObjectLayer NUM_LAYERS = 2;
}; // namespace Layers

	class MyCharacterContactListener : public CharacterContactListener {

	};

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
  virtual bool ShouldCollide(ObjectLayer inObject1,
                             ObjectLayer inObject2) const override {
    switch (inObject1) {
    case Layers::NON_MOVING:
      return inObject2 ==
             Layers::MOVING; // Non moving only collides with moving
    case Layers::MOVING:
      return true; // Moving collides with everything
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

// Each broadphase layer results in a separate bounding volume tree in the broad
// phase. You at least want to have a layer for non-moving and moving objects to
// avoid having to update a tree full of static objects every frame. You can
// have a 1-on-1 mapping between object layers and broadphase layers (like in
// this case) but if you have many object layers you'll be creating many broad
// phase trees, which is not efficient. If you want to fine tune your broadphase
// layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on
// the TTY.
namespace BroadPhaseLayers {
static constexpr BroadPhaseLayer NON_MOVING(0);
static constexpr BroadPhaseLayer MOVING(1);
static constexpr uint NUM_LAYERS(2);
}; // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
  BPLayerInterfaceImpl() {
    // Create a mapping table from object to broad phase layer
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
  }

  virtual uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::NUM_LAYERS;
  }

  virtual BroadPhaseLayer
  GetBroadPhaseLayer(ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
  {
      switch ((BroadPhaseLayer::Type)inLayer)
      {
      case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
      case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
      default:													JPH_ASSERT(false); return "INVALID";
      }
  }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
  BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
  virtual bool ShouldCollide(ObjectLayer inLayer1,
                             BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
    case Layers::NON_MOVING:
      return inLayer2 == BroadPhaseLayers::MOVING;
    case Layers::MOVING:
      return true;
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

#ifdef PHYSICS_MAIN
	PhysicsSystem PHYSICS_SYSTEM;
#else
	extern PhysicsSystem PHYSICS_SYSTEM;
#endif

void InitPhysics();

// We simulate the physics world in discrete time steps. 60 Hz is a good rate to
// update the physics system.
constexpr float cDeltaTime = 1.0f / 60.0f;

struct BodyMetadata {
  Handle<GameObject> owner;
};

class RigidBody : public Component {
protected:
  Body *body = nullptr;
  RigidBody(Handle<GameObject> owner) : Component{owner} {}
  RigidBody(const RigidBody &) = delete;
  RigidBody &operator=(const RigidBody &) = delete;
  RigidBody(RigidBody &&other) : Component{other.owner}, body{other.body} {
    other.owner = Handle<GameObject>{};
    other.body = nullptr;
  }
  RigidBody &operator=(RigidBody &&other) {
    this->~RigidBody();
    owner = other.owner;
    body = other.body;
    other.owner = Handle<GameObject>{};
    other.body = nullptr;
    return *this;
  }
  ~RigidBody() {
    if (body) {
      auto &lInterface = PHYSICS_SYSTEM.GetBodyInterfaceNoLock();
      auto userdata = body->GetUserData();
      if (userdata != 0) {
        delete reinterpret_cast<BodyMetadata *>(userdata);
      }
      lInterface.RemoveBody(body->GetID());
    }
  }

public:
  inline Body &GetBody() { return *body; }
  inline const Body &GetBody() const { return *body; }

  inline Vec3 GetPosition() const { return body->GetPosition(); }
  inline Vec3 GetVelocity() const { return body->GetLinearVelocity(); }
  inline Vec3 GetAngularVelocity() const { return body->GetAngularVelocity(); }
  inline void SetVelocity(Vec3 newVelocity) {
    body->SetLinearVelocity(newVelocity);
  }
  inline void SetAngularVelocity(Vec3 newVelocity) {
    body->SetAngularVelocity(newVelocity);
  }
};

class StaticBody : public RigidBody {
public:
  StaticBody(Handle<GameObject> owner, BodyInterface &bodyInterface,
             ShapeRefC shape, Vec3 initPos, Quat initRotation)
      : RigidBody{owner} {
    BodyCreationSettings bodySettings{shape, initPos, initRotation,
                                      EMotionType::Static, Layers::NON_MOVING};
    auto body = bodyInterface.CreateBody(bodySettings);
    body->SetUserData(reinterpret_cast<uint64_t>(new BodyMetadata{owner}));
    this->body = body;
    bodyInterface.AddBody(body->GetID(), EActivation::DontActivate);
  }

  virtual EventPropagation HandleEvent(Event &event) override;
};

class KinematicBody : public RigidBody {
public:
  KinematicBody(Handle<GameObject> owner, BodyInterface &bodyInterface,
                ShapeRefC shape, Vec3 initPos, Quat initRotation)
      : RigidBody{owner} {
    BodyCreationSettings bodySettings{shape, initPos, initRotation,
                                      EMotionType::Kinematic, Layers::MOVING};
    auto body = bodyInterface.CreateBody(bodySettings);
    body->SetUserData(reinterpret_cast<uint64_t>(new BodyMetadata{owner}));
    this->body = body;
    bodyInterface.AddBody(body->GetID(), EActivation::Activate);
  }
  virtual EventPropagation HandleEvent(Event &event) override;
};

class DynamicBody : public RigidBody {
public:
    DynamicBody(Handle<GameObject> owner, BodyInterface& bodyInterface,
        ShapeRefC shape, Vec3 initPos, Quat initRotation)
        : RigidBody{ owner } {
        BodyCreationSettings bodySettings{ shape, initPos, initRotation,
                                          EMotionType::Dynamic, Layers::MOVING };
        auto body = bodyInterface.CreateBody(bodySettings);
        body->SetUserData(reinterpret_cast<uint64_t>(new BodyMetadata{ owner }));
        this->body = body;
        bodyInterface.AddBody(body->GetID(), EActivation::Activate);
    }
    virtual EventPropagation HandleEvent(Event& event) override;
};

class SensorBody : public RigidBody {
public:
  SensorBody(Handle<GameObject> owner, BodyInterface &bodyInterface,
             ShapeRefC shape, Vec3 initPos, Quat initRotation,
             EMotionType motionType)
      : RigidBody{owner} {
    ObjectLayer layer = Layers::MOVING;
    if (motionType == EMotionType::Static) {
      layer = Layers::NON_MOVING;
    }
    BodyCreationSettings bodySettings{shape, initPos, initRotation, motionType,
                                      layer};
    bodySettings.mIsSensor = true;
    auto body = bodyInterface.CreateBody(bodySettings);
    body->SetUserData(reinterpret_cast<uint64_t>(new BodyMetadata{owner}));
    this->body = body;
    bodyInterface.AddBody(body->GetID(), EActivation::DontActivate);
  }
  virtual EventPropagation HandleEvent(Event &event) override;
	};

	class CharacterBody : public Component {
	private:
		std::unique_ptr<CharacterVirtual> controller = nullptr;

	public:
        Vec3 targetVelocity = Vec3::sZero();
        Vec3 desiredVelocity = Vec3::sZero();
        bool allowSliding = false;
        bool inJump = false;
        const float speed = 10.f;
        const float jumpSpeed = 6.f;

		CharacterBody(Handle<GameObject> owner,
			const CharacterVirtualSettings& settings,
			RVec3Arg position,
			QuatArg rotation,
			PhysicsSystem& physicsSystem,
			std::unique_ptr<CharacterContactListener> listener) : Component{ owner } {
			controller.reset(new CharacterVirtual{ &settings, position, rotation, (uint64)this, &physicsSystem });
			controller->SetListener(listener.release());
		}
		CharacterBody(const CharacterBody&) = delete;
		CharacterBody& operator=(const CharacterBody&) = delete;
		CharacterBody(CharacterBody&&) = default;
		CharacterBody& operator=(CharacterBody&&) = default;

        CharacterVirtual& GetController() { return *controller; }
        const CharacterVirtual& GetController() const { return *controller; }
		void Update(float dt);
};

void UpdatePhysicsSystem(float deltaTime);

} // namespace physics