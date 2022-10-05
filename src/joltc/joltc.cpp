// Copyright � Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "joltc.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <iostream>
#include <cstdarg>
#include <thread>

// All Jolt symbols are in the JPH namespace
using namespace JPH;

// Callback for traces, connect this to your own trace function if you have one
static void TraceImpl(const char* inFMT, ...)
{
    // Format the message
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);

    // Print to the TTY
    std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint inLine)
{
    // Print to the TTY
    std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;

    // Breakpoint
    return true;
};

#endif // JPH_ENABLE_ASSERTS

static JPH::Vec3 ToVec3(const JPH_Vec3* vec)
{
    return JPH::Vec3(vec->x, vec->y, vec->z);
}

static void FromVec3(const JPH::Vec3& vec, JPH_Vec3* result)
{
    result->x = vec.GetX();
    result->y = vec.GetY();
    result->z = vec.GetZ();
}

static JPH::Quat ToQuat(const JPH_Quat* quat)
{
    return JPH::Quat(quat->x, quat->y, quat->z, quat->w);
}

bool JPH_Init(void)
{
    JPH::RegisterDefaultAllocator();

    // TODO
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

        // Create a factory
        JPH::Factory::sInstance = new JPH::Factory();

    // Register all Jolt physics types
    JPH::RegisterTypes();

    return true;
}

void JPH_Shutdown(void)
{
    // Destroy the factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

JPH_TempAllocator* JPH_TempAllocator_Create(uint32_t size)
{
    auto impl = new JPH::TempAllocatorImpl(size);
    return reinterpret_cast<JPH_TempAllocator*>(impl);
}

void JPH_TempAllocator_Destroy(JPH_TempAllocator* allocator)
{
    if (allocator)
    {
        delete reinterpret_cast<JPH::TempAllocator*>(allocator);
    }
}

JPH_JobSystemThreadPool* JPH_JobSystemThreadPool_Create(uint32_t maxJobs, uint32_t maxBarriers, int inNumThreads)
{
    auto job_system = new JPH::JobSystemThreadPool(maxJobs, maxBarriers, inNumThreads);
    return reinterpret_cast<JPH_JobSystemThreadPool*>(job_system);
}

JPH_CAPI void JPH_JobSystemThreadPool_Destroy(JPH_JobSystemThreadPool* system)
{
    if (system)
    {
        delete reinterpret_cast<JPH::JobSystemThreadPool*>(system);
    }
}

/* JPH_BroadPhaseLayerInterface */
static JPH_BroadPhaseLayerInterface_Procs g_BroadPhaseLayerInterface_Procs;

class ManagedBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
    uint GetNumBroadPhaseLayers() const override
    {
        return g_BroadPhaseLayerInterface_Procs.GetNumBroadPhaseLayers(
            reinterpret_cast<const JPH_BroadPhaseLayerInterface*>(this)
        );
    }

    BroadPhaseLayer	GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        return (BroadPhaseLayer)g_BroadPhaseLayerInterface_Procs.GetBroadPhaseLayer(
            reinterpret_cast<const JPH_BroadPhaseLayerInterface*>(this),
            static_cast<JPH_ObjectLayer>(inLayer)
        );
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        if (g_BroadPhaseLayerInterface_Procs.GetBroadPhaseLayerName == nullptr)
            return nullptr;

        return g_BroadPhaseLayerInterface_Procs.GetBroadPhaseLayerName(
            reinterpret_cast<const JPH_BroadPhaseLayerInterface*>(this),
            static_cast<JPH_BroadPhaseLayer>(inLayer)
        );
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED
};

void JPH_BroadPhaseLayerInterface_SetProcs(JPH_BroadPhaseLayerInterface_Procs procs)
{
    g_BroadPhaseLayerInterface_Procs = procs;
}

JPH_BroadPhaseLayerInterface* JPH_BroadPhaseLayerInterface_Create()
{
    auto system = new ManagedBroadPhaseLayerInterface();
    return reinterpret_cast<JPH_BroadPhaseLayerInterface*>(system);
}

void JPH_BroadPhaseLayerInterface_Destroy(JPH_BroadPhaseLayerInterface* layer)
{
    if (layer)
    {
        delete reinterpret_cast<ManagedBroadPhaseLayerInterface*>(layer);
    }
}

/* ShapeSettings */
void JPH_ShapeSettings_Destroy(JPH_ShapeSettings* settings)
{
    if (settings)
    {
        delete reinterpret_cast<JPH::ShapeSettings*>(settings);
    }
}

JPH_BoxShapeSettings* JPH_BoxShapeSettings_Create(const JPH_Vec3* halfExtent, float convexRadius)
{
    auto settings = new JPH::BoxShapeSettings(ToVec3(halfExtent), convexRadius);
    return reinterpret_cast<JPH_BoxShapeSettings*>(settings);
}

JPH_SphereShapeSettings* JPH_SphereShapeSettings_Create(float radius)
{
    auto settings = new JPH::SphereShapeSettings(radius);
    return reinterpret_cast<JPH_SphereShapeSettings*>(settings);
}

/* JPH_BodyCreationSettings */
JPH_BodyCreationSettings* JPH_BodyCreationSettings_Create()
{
    auto bodyCreationSettings = new JPH::BodyCreationSettings();
    return reinterpret_cast<JPH_BodyCreationSettings*>(bodyCreationSettings);
}

JPH_BodyCreationSettings* JPH_BodyCreationSettings_Create2(
    JPH_ShapeSettings* shapeSettings,
    const JPH_Vec3* position,
    const JPH_Quat* rotation,
    JPH_MotionType motionType,
    JPH_ObjectLayer objectLayer)
{
    JPH::ShapeSettings* joltShapeSettings = reinterpret_cast<JPH::ShapeSettings*>(shapeSettings);
    auto bodyCreationSettings = new JPH::BodyCreationSettings(
        joltShapeSettings,
        ToVec3(position),
        ToQuat(rotation),
        (JPH::EMotionType)motionType,
        objectLayer
    );
    return reinterpret_cast<JPH_BodyCreationSettings*>(bodyCreationSettings);
}

void JPH_BodyCreationSettings_Destroy(JPH_BodyCreationSettings* settings)
{
    if (settings)
    {
        delete reinterpret_cast<JPH::BodyCreationSettings*>(settings);
    }
}

/* JPH_PhysicsSystem */
JPH_PhysicsSystem* JPH_PhysicsSystem_Create(void)
{
    auto system = new JPH::PhysicsSystem();
    return reinterpret_cast<JPH_PhysicsSystem*>(system);
}

void JPH_PhysicsSystem_Destroy(JPH_PhysicsSystem* system)
{
    if (system)
    {
        delete reinterpret_cast<JPH::PhysicsSystem*>(system);
    }
}

void JPH_PhysicsSystem_Init(JPH_PhysicsSystem* system,
    uint32_t maxBodies, uint32_t numBodyMutexes, uint32_t maxBodyPairs, uint32_t maxContactConstraints,
    JPH_BroadPhaseLayer* layer,
    JPH_ObjectVsBroadPhaseLayerFilter objectVsBroadPhaseLayerFilter,
    JPH_ObjectLayerPairFilter objectLayerPairFilter)
{
    JPH_ASSERT(system);

    reinterpret_cast<JPH::PhysicsSystem*>(system)->Init(
        maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
        *reinterpret_cast<const JPH::BroadPhaseLayerInterface*>(layer),
        reinterpret_cast<JPH::ObjectVsBroadPhaseLayerFilter>(objectVsBroadPhaseLayerFilter),
        reinterpret_cast<JPH::ObjectLayerPairFilter>(objectLayerPairFilter)
    );
}

void JPH_PhysicsSystem_OptimizeBroadPhase(JPH_PhysicsSystem* system)
{
    JPH_ASSERT(system);

    reinterpret_cast<JPH::PhysicsSystem*>(system)->OptimizeBroadPhase();
}

void JPH_PhysicsSystem_Update(JPH_PhysicsSystem* system, float deltaTime, int collisionSteps, int integrationSubSteps,
    JPH_TempAllocator* tempAlocator,
    JPH_JobSystemThreadPool* jobSystem)
{
    JPH_ASSERT(system);

    auto joltSystem = reinterpret_cast<JPH::PhysicsSystem*>(system);
    auto joltTempAlocator = reinterpret_cast<JPH::TempAllocator*>(tempAlocator);
    auto joltJobSystem = reinterpret_cast<JPH::JobSystemThreadPool*>(jobSystem);
    joltSystem->Update(deltaTime, collisionSteps, integrationSubSteps, joltTempAlocator, joltJobSystem);
}

JPH_BodyInterface* JPH_PhysicsSystem_GetBodyInterface(JPH_PhysicsSystem* system)
{
    JPH_ASSERT(system);

    auto joltSystem = reinterpret_cast<JPH::PhysicsSystem*>(system);
    return reinterpret_cast<JPH_BodyInterface*>(&joltSystem->GetBodyInterface());
}

void JPH_PhysicsSystem_SetContactListener(JPH_PhysicsSystem* system, JPH_ContactListener* listener)
{
    JPH_ASSERT(system);

    auto joltSystem = reinterpret_cast<JPH::PhysicsSystem*>(system);
    auto joltListener = reinterpret_cast<JPH::ContactListener*>(listener);
    joltSystem->SetContactListener(joltListener);
}

JPH_Body* JPH_BodyInterface_CreateBody(JPH_BodyInterface* interface, JPH_BodyCreationSettings* settings)
{
    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);

    auto body = joltBodyInterface->CreateBody(
        *reinterpret_cast<const JPH::BodyCreationSettings*>(settings)
    );

    return reinterpret_cast<JPH_Body*>(body);
}

JPH_BodyID JPH_BodyInterface_CreateAndAddBody(JPH_BodyInterface* interface, JPH_BodyCreationSettings* settings, JPH_ActivationMode activation)
{
    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    JPH::BodyID bodyID = joltBodyInterface->CreateAndAddBody(
        *reinterpret_cast<const JPH::BodyCreationSettings*>(settings),
        (JPH::EActivation)activation
    );

    return bodyID.GetIndexAndSequenceNumber();
}

void JPH_BodyInterface_DestroyBody(JPH_BodyInterface* interface, JPH_BodyID bodyID)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    joltBodyInterface->DestroyBody(JPH::BodyID(bodyID));
}

void JPH_BodyInterface_AddBody(JPH_BodyInterface* interface, JPH_BodyID bodyID, JPH_ActivationMode activation)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    joltBodyInterface->AddBody(JPH::BodyID(bodyID), (JPH::EActivation)activation);
}

void JPH_BodyInterface_RemoveBody(JPH_BodyInterface* interface, JPH_BodyID bodyID)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    joltBodyInterface->RemoveBody(JPH::BodyID(bodyID));
}

bool JPH_BodyInterface_IsActive(JPH_BodyInterface* interface, JPH_BodyID bodyID)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    return joltBodyInterface->IsActive(JPH::BodyID(bodyID));
}

bool JPH_BodyInterface_IsAdded(JPH_BodyInterface* interface, JPH_BodyID bodyID)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    return joltBodyInterface->IsAdded(JPH::BodyID(bodyID));
}

void JPH_BodyInterface_SetLinearVelocity(JPH_BodyInterface* interface, JPH_BodyID bodyID, const JPH_Vec3* velocity)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    joltBodyInterface->SetLinearVelocity(JPH::BodyID(bodyID), ToVec3(velocity));
}

void JPH_BodyInterface_GetLinearVelocity(JPH_BodyInterface* interface, JPH_BodyID bodyID, JPH_Vec3* velocity)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    auto joltVector = joltBodyInterface->GetLinearVelocity(JPH::BodyID(bodyID));
    FromVec3(joltVector, velocity);
}

void JPH_BodyInterface_GetCenterOfMassPosition(JPH_BodyInterface* interface, JPH_BodyID bodyID, JPH_Vec3* position)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    auto joltVector = joltBodyInterface->GetCenterOfMassPosition(JPH::BodyID(bodyID));
    FromVec3(joltVector, position);
}

JPH_MotionType JPH_BodyInterface_GetMotionType(JPH_BodyInterface* interface, JPH_BodyID bodyID)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    return static_cast<JPH_MotionType>(joltBodyInterface->GetMotionType(JPH::BodyID(bodyID)));
}

void JPH_BodyInterface_SetMotionType(JPH_BodyInterface* interface, JPH_BodyID bodyID, JPH_MotionType motionType, JPH_ActivationMode activationMode)
{
    JPH_ASSERT(interface);

    auto joltBodyInterface = reinterpret_cast<JPH::BodyInterface*>(interface);
    joltBodyInterface->SetMotionType(
        JPH::BodyID(bodyID),
        static_cast<JPH::EMotionType>(motionType),
        static_cast<JPH::EActivation>(activationMode)
    );
}

/* Body */
JPH_BodyID JPH_Body_GetID(JPH_Body* body)
{
    auto joltBody = reinterpret_cast<JPH::Body*>(body);
    return joltBody->GetID().GetIndexAndSequenceNumber();
}

bool JPH_Body_IsActive(JPH_Body* body)
{
    return reinterpret_cast<JPH::Body*>(body)->IsActive();
}

bool JPH_Body_IsStatic(JPH_Body* body)
{
    return reinterpret_cast<JPH::Body*>(body)->IsStatic();
}

bool JPH_Body_IsKinematic(JPH_Body* body)
{
    return reinterpret_cast<JPH::Body*>(body)->IsKinematic();
}

bool JPH_Body_IsDynamic(JPH_Body* body)
{
    return reinterpret_cast<JPH::Body*>(body)->IsDynamic();
}

bool JPH_Body_IsSensor(JPH_Body* body)
{
    return reinterpret_cast<JPH::Body*>(body)->IsSensor();
}

JPH_MotionType JPH_Body_GetMotionType(JPH_Body* body)
{
    return static_cast<JPH_MotionType>(reinterpret_cast<JPH::Body*>(body)->GetMotionType());
}

void JPH_Body_SetMotionType(JPH_Body* body, JPH_MotionType motionType)
{
    reinterpret_cast<JPH::Body*>(body)->SetMotionType(static_cast<JPH::EMotionType>(motionType));
}

/* Contact Listener */
static JPH_ContactListener_Procs g_ContactListener_Procs;

class ManagedContactListener final : public JPH::ContactListener
{
public:
    ValidateResult OnContactValidate(const Body& inBody1, const Body& inBody2, const CollideShapeResult& inCollisionResult) override
    {
        JPH_ValidateResult result = g_ContactListener_Procs.OnContactValidate(
            reinterpret_cast<JPH_ContactListener*>(this),
            reinterpret_cast<const JPH_Body*>(&inBody1),
            reinterpret_cast<const JPH_Body*>(&inBody2),
            nullptr
        );

        return (JPH::ValidateResult)result;
    }

    void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
    {
        g_ContactListener_Procs.OnContactAdded(
            reinterpret_cast<JPH_ContactListener*>(this),
            reinterpret_cast<const JPH_Body*>(&inBody1),
            reinterpret_cast<const JPH_Body*>(&inBody2)
        );
    }

    void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override
    {
        g_ContactListener_Procs.OnContactPersisted(
            reinterpret_cast<JPH_ContactListener*>(this),
            reinterpret_cast<const JPH_Body*>(&inBody1),
            reinterpret_cast<const JPH_Body*>(&inBody2)
        );
    }

    void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override
    {
        g_ContactListener_Procs.OnContactRemoved(
            reinterpret_cast<JPH_ContactListener*>(this),
            reinterpret_cast<const JPH_SubShapeIDPair*>(&inSubShapePair)
        );
    }
};

void JPH_ContactListener_SetProcs(JPH_ContactListener_Procs procs)
{
    g_ContactListener_Procs = procs;
}

JPH_ContactListener* JPH_ContactListener_Create()
{
    auto impl = new ManagedContactListener();
    return reinterpret_cast<JPH_ContactListener*>(impl);
}

void JPH_ContactListener_Destroy(JPH_ContactListener* listener)
{
    if (listener)
    {
        delete reinterpret_cast<ManagedContactListener*>(listener);
    }
}
