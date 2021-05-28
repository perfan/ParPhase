#ifndef PHASE_COLLISION_MODEL_H
#define PHASE_COLLISION_MODEL_H

#include "ImmersedBoundaryObject.h"

class CollisionModel
{
public:

    CollisionModel(Scalar eps_particle,Scalar range_particle, Scalar eps_wall, Scalar range_wall);

    virtual Vector2D force(const ImmersedBoundaryObject& ibObjP, const ImmersedBoundaryObject& ibObjQ) const;

    virtual Vector2D force(const ImmersedBoundaryObject& ibObj, const FiniteVolumeGrid2D& grid) const;

    Scalar eps() const
    { return eps_particle_; }

    Scalar range() const
    { return range_particle_; }

private:

    Scalar eps_particle_, range_particle_, eps_wall_, range_wall_;
};


#endif
