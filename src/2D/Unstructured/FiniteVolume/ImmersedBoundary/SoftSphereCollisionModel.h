#ifndef PHASE_SOFT_SPHERE_COLLISION_MODEL_H
#define PHASE_OFT_SPHERE_COLLISION_MODEL_H

#include "ImmersedBoundaryObject.h"

class SoftSphereCollisionModel
{
public:

    SoftSphereCollisionModel(Scalar k_particle, Scalar eta_particle, Scalar range_particle, Scalar k_wall, Scalar eta_wall, Scalar range_wall);

    virtual Vector2D force(const ImmersedBoundaryObject& ibObjP, const ImmersedBoundaryObject& ibObjQ) const;

    virtual Vector2D force(const ImmersedBoundaryObject& ibObj, const FiniteVolumeGrid2D& grid) const;

    Scalar k() const
    { return k_particle_; }

    Scalar eta() const
    { return eta_particle_; }

    Scalar range() const
    { return range_particle_; }

private:

    Scalar k_particle_, eta_particle_, range_particle_, k_wall_, eta_wall_, range_wall_;
};


#endif
