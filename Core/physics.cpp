#define PHYSICS_MAIN

#include "physics.h"
#include "event.h"
#include "world.h"
#include <format>
#include <iostream>
#include <WindowsX.h>
#include "Components/Mesh.h"


Handle<GameObject> debugObj1;
Handle<GameObject> debugObj2;

namespace physics {
	using namespace JPH;
	using namespace JPH::literals;
	using namespace std;

	// An example contact listener
	class MyContactListener : public ContactListener {
	public:
		// See: ContactListener
		virtual ValidateResult
			OnContactValidate(const Body& inBody1, const Body& inBody2,
				RVec3Arg inBaseOffset,
				const CollideShapeResult& inCollisionResult) override {
			//cout << "Contact validate callback" << endl;

			// Allows you to ignore a contact before it is created (using layers to not
			// make objects collide is cheaper!)
			return ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		virtual void OnContactAdded(const Body& inBody1, const Body& inBody2,
			const ContactManifold& inManifold,
			ContactSettings& ioSettings) override {
			auto& meta1 = *reinterpret_cast<BodyMetadata*>(inBody1.GetUserData());
			auto& meta2 = *reinterpret_cast<BodyMetadata*>(inBody2.GetUserData());
			auto pObject1 = WORLD.GetGameObject(meta1.owner);
			auto pObject2 = WORLD.GetGameObject(meta2.owner);
			assert(pObject1);
			assert(pObject2);
			Event ev1{ CollidedWith{meta2.owner} };
			Event ev2{ CollidedWith{meta1.owner} };
			pObject1->HandleEvent(ev1);
			pObject2->HandleEvent(ev2);
		}

		virtual void OnContactPersisted(const Body& inBody1, const Body& inBody2,
			const ContactManifold& inManifold,
			ContactSettings& ioSettings) override {
			cout << "A contact was persisted" << endl;
		}

		virtual void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override {
			auto idBody1 = inSubShapePair.GetBody1ID();
			auto idBody2 = inSubShapePair.GetBody2ID();
			auto& rInterface = PHYSICS_SYSTEM.GetBodyInterfaceNoLock();

			auto& meta1 =
				*reinterpret_cast<BodyMetadata*>(rInterface.GetUserData(idBody1));
			auto& meta2 =
				*reinterpret_cast<BodyMetadata*>(rInterface.GetUserData(idBody2));
			auto pObject1 = WORLD.GetGameObject(meta1.owner);
			auto pObject2 = WORLD.GetGameObject(meta2.owner);
			assert(pObject1);
			assert(pObject2);
			Event ev1{ DetachedWith{meta2.owner} };
			Event ev2{ DetachedWith{meta1.owner} };
			pObject1->HandleEvent(ev1);
			pObject2->HandleEvent(ev2);
		}
	};

	// An example activation listener
	class MyBodyActivationListener : public BodyActivationListener {
	public:
		virtual void OnBodyActivated(const BodyID& inBodyID,
			uint64 inBodyUserData) override {
			auto& rInterface = PHYSICS_SYSTEM.GetBodyInterfaceNoLock();
			auto& meta =
				*reinterpret_cast<BodyMetadata*>(rInterface.GetUserData(inBodyID));
			cout << "A body got activated " << std::format("index: {}", meta.owner.idx) << endl;
		}

		virtual void OnBodyDeactivated(const BodyID& inBodyID,
			uint64 inBodyUserData) override {
			cout << "A body went to sleep" << endl;
		}
	};

	// We need a temp allocator for temporary allocations during the physics
	// update. We're pre-allocating 10 MB to avoid having to do allocations during
	// the physics update. B.t.w. 10 MB is way too much for this example but it is
	// a typical value you can use. If you don't want to pre-allocate you can also
	// use TempAllocatorMalloc to fall back to malloc / free.
	TempAllocatorImpl* temp_allocator = nullptr;

	// We need a job system that will execute physics jobs on multiple threads.
	// Typically you would implement the JobSystem interface yourself and let Jolt
	// Physics run on top of your own job scheduler. JobSystemThreadPool is an
	// example implementation.
	JobSystemThreadPool* job_system = nullptr;

	// Create mapping table from object layer to broadphase layer
	// Note: As this is an interface, PhysicsSystem will take a reference to this
	// so this instance needs to stay alive! Also have a look at
	// BroadPhaseLayerInterfaceTable or BroadPhaseLayerInterfaceMask for a simpler
	// interface.
	BPLayerInterfaceImpl broad_phase_layer_interface;

	// Create class that filters object vs broadphase layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this
	// so this instance needs to stay alive! Also have a look at
	// ObjectVsBroadPhaseLayerFilterTable or ObjectVsBroadPhaseLayerFilterMask for
	// a simpler interface.
	ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

	// Create class that filters object vs object layers
	// Note: As this is an interface, PhysicsSystem will take a reference to this
	// so this instance needs to stay alive! Also have a look at
	// ObjectLayerPairFilterTable or ObjectLayerPairFilterMask for a simpler
	// interface.
	ObjectLayerPairFilterImpl object_vs_object_layer_filter;

	MyContactListener contact_listener;
	MyBodyActivationListener body_activation_listener;

	void InitPhysics() {
		RegisterDefaultAllocator();
		temp_allocator = new TempAllocatorImpl{ 10 * 1024 * 1024 };
		job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1
		);
		Factory::sInstance = new Factory();
		// Register all physics types with the factory and install their collision
		// handlers with the CollisionDispatch class. If you have your own custom
		// shape types you probably need to register their handlers with the
		// CollisionDispatch before calling this function. If you implement your own
		// default material (PhysicsMaterial::sDefault) make sure to initialize it
		// before this function or else this function will create one for you.
		RegisterTypes();

		// This is the max amount of rigid bodies that you can add to the physics
		// system. If you try to add more you'll get an error. Note: This value is low
		// because this is a simple test. For a real project use something in the
		// order of 65536.
		const uint cMaxBodies = 1024;

		// This determines how many mutexes to allocate to protect rigid bodies from
		// concurrent access. Set it to 0 for the default settings.
		const uint cNumBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the
		// broad phase will detect overlapping body pairs based on their bounding
		// boxes and will insert them into a queue for the narrowphase). If you make
		// this buffer too small the queue will fill up and the broad phase jobs will
		// start to do narrow phase work. This is slightly less efficient. Note: This
		// value is low because this is a simple test. For a real project use
		// something in the order of 65536.
		const uint cMaxBodyPairs = 1024;

		// This is the maximum size of the contact constraint buffer. If more contacts
		// (collisions between bodies) are detected than this number then these
		// contacts will be ignored and bodies will start interpenetrating / fall
		// through the world. Note: This value is low because this is a simple test.
		// For a real project use something in the order of 10240.
		const uint cMaxContactConstraints = 1024;

		// Now we can create the actual physics system.
		PHYSICS_SYSTEM.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs,
			cMaxContactConstraints, broad_phase_layer_interface,
			object_vs_broadphase_layer_filter,
			object_vs_object_layer_filter);

		// A body activation listener gets notified when bodies activate and go to
		// sleep Note that this is called from a job so whatever you do here needs to
		// be thread safe. Registering one is entirely optional.
		PHYSICS_SYSTEM.SetBodyActivationListener(&body_activation_listener);

		// A contact listener gets notified when bodies (are about to) collide, and
		// when they separate again. Note that this is called from a job so whatever
		// you do here needs to be thread safe. Registering one is entirely optional.
		PHYSICS_SYSTEM.SetContactListener(&contact_listener);
	}

	EventPropagation StaticBody::HandleEvent(Event& event) {
		cout << "Message from static body: ";
		std::visit(overloaded{ [](CollidedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "collided with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](DetachedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "detached with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](auto& ev) { cout << "unknown event" << endl; } },
			event);
		return EventPropagation::STOP;
	}

	EventPropagation KinematicBody::HandleEvent(Event& event) {
		cout << "Message from kinematic body: ";
		std::visit(overloaded{ [](CollidedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "collided with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](DetachedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "detached with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](auto& ev) { cout << "unknown event" << endl; } },
			event);
		return EventPropagation::STOP;
	}

	EventPropagation DynamicBody::HandleEvent(Event& event) {
		cout << "Message from dynamic body: ";
		std::visit(overloaded{ [](CollidedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "collided with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](DetachedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "detached with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](auto& ev) { cout << "unknown event" << endl; } },
			event);
		return EventPropagation::STOP;
	}

	EventPropagation SensorBody::HandleEvent(Event& event) {
		cout << "Message from sensor body: ";
		std::visit(overloaded{ [](CollidedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "collided with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](DetachedWith& ev) {
								auto gen = ev.target.real_id.generation;
								auto id = ev.target.real_id.id;
								cout << "detached with: "
									 << std::format("gen {}, id {}", gen, id) << endl;
							  },
							  [](auto& ev) { cout << "unknown event" << endl; } },
			event);
		return EventPropagation::STOP;
	}


	void UpdatePhysicsSystem(float deltaTime) {
		static float accDeltaTime = 0.0f;
		accDeltaTime += deltaTime;

		auto& characters = WORLD.GetComponents<CharacterBody>();
		for (auto& character : characters) {
			character.Update(deltaTime);
		}

		if (accDeltaTime >= cDeltaTime) {
			size_t requiredSteps = (int)(round(accDeltaTime / cDeltaTime));
			float simulationDeltaTime = accDeltaTime;
			accDeltaTime = 0.f;

			PHYSICS_SYSTEM.Update(simulationDeltaTime, requiredSteps, temp_allocator,
				job_system);
		}

		for (auto& character : characters) {
			auto hOwner = character.GetOwner();
			auto pMesh = WORLD.GetComponent<MeshComponent>(hOwner);
			if (pMesh) {
				auto pos = character.GetController().GetPosition();
				pMesh->Translation.x = pos.GetX();
				pMesh->Translation.y = pos.GetY();
				pMesh->NumFramesDirty = gNumFrameResources;
				pMesh->Translation.z = pos.GetZ();
			}
		}
	}

	Mat44 FromDirectXMat(const XMMATRIX& mat)
	{
		auto columnMat = XMMatrixTranspose(mat);
		return Mat44::sLoadFloat4x4Aligned(reinterpret_cast<Float4*>(&columnMat));
	}

	void CharacterBody::Update(float dt) {
		bool player_controls_horizontal_velocity = controller->IsSupported();
		if (player_controls_horizontal_velocity)
		{
			// Smooth the player input
			desiredVelocity = 0.25f * targetVelocity + 0.75f * desiredVelocity;

			// True if the player intended to move
			allowSliding = !targetVelocity.IsNearZero();
		}
		else
		{
			// While in air we allow sliding
			allowSliding = true;
		}

		controller->UpdateGroundVelocity();
		Vec3 current_vertical_velocity = controller->GetLinearVelocity().Dot(controller->GetUp()) * controller->GetUp();
		Vec3 ground_velocity = controller->GetGroundVelocity();
		Vec3 new_velocity;
		bool moving_towards_ground = (current_vertical_velocity.GetY() - ground_velocity.GetY()) < 0.1f;
		if (
			controller->GetGroundState() == CharacterVirtual::EGroundState::OnGround	// If on ground
			&&
			moving_towards_ground)
		{
			// Assume velocity of ground when on ground
			new_velocity = ground_velocity;

			// Jump
			if (inJump && moving_towards_ground)
				new_velocity += jumpSpeed * controller->GetUp();
		}
		else
			new_velocity = current_vertical_velocity;
		// Gravity
		new_velocity += PHYSICS_SYSTEM.GetGravity() * dt;

		if (player_controls_horizontal_velocity)
		{
			// Player input
			new_velocity += desiredVelocity;
		}
		else
		{
			// Preserve horizontal velocity
			Vec3 current_horizontal_velocity = controller->GetLinearVelocity() - current_vertical_velocity;
			new_velocity += current_horizontal_velocity;
		}

		// Update character velocity
		controller->SetLinearVelocity(new_velocity);

		CharacterVirtual::ExtendedUpdateSettings update_settings;

		controller->ExtendedUpdate(dt,
			-controller->GetUp() * PHYSICS_SYSTEM.GetGravity().Length(),
			update_settings,
			PHYSICS_SYSTEM.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
			PHYSICS_SYSTEM.GetDefaultLayerFilter(Layers::MOVING),
			{}, {},
			*temp_allocator);
	}
} // namespace physics