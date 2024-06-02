#pragma once

#include "Physics/LumosPhysicsEngine/RigidBody3D.h"
#include "Core/DataStructures/TDArray.h"

namespace Lumos
{

    struct LUMOS_EXPORT CollisionPair
    {
        RigidBody3D* pObjectA;
        RigidBody3D* pObjectB;
    };

    class LUMOS_EXPORT Broadphase
    {
    public:
        virtual ~Broadphase()                                                                                                                  = default;
        virtual void FindPotentialCollisionPairs(RigidBody3D* rootObject, TDArray<CollisionPair>& collisionPairs, uint32_t totalRigidBodyCount) = 0;
        virtual void DebugDraw()                                                                                                               = 0;
    };
}
