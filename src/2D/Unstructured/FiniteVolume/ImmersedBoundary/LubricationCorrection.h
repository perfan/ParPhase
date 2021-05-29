#ifndef PHASE_LUBRICATION_CORRECTION_H
#define PHASE_LUBRICATION_CORRECTION_H

#include "ImmersedBoundaryObject.h"

class LubricationCorrection
{
public:

    LubricationCorrection(Scalar mu, Scalar range_particle, Scalar range_wall);

    virtual Vector2D force(const ImmersedBoundaryObject& ibObjP, const ImmersedBoundaryObject& ibObjQ) const;

    virtual Vector2D force(const ImmersedBoundaryObject& ibObj, const FiniteVolumeGrid2D& grid) const;

    Scalar range_particle() const
    { return range_particle_; }

    Scalar range_wall() const
    { return range_wall_; }

private:

    Scalar mu_, range_particle_, range_wall_;
};


#endif
