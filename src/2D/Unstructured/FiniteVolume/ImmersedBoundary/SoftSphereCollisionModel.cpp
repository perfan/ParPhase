#include <algorithm>

#include "SoftSphereCollisionModel.h"
#include "FiniteVolumeGrid2D/FiniteVolumeGrid2D.h"
#include "FiniteVolumeGrid2D/Face/FaceGroup.h"

SoftSphereCollisionModel::SoftSphereCollisionModel(Scalar k_particle, Scalar eta_particle, Scalar range_particle, Scalar k_wall, Scalar eta_wall, Scalar range_wall)
{
    k_particle_ = k_particle;
    eta_particle_ = eta_particle;
    range_particle_ = range_particle;
    k_wall_ = k_wall;
    eta_wall_ = eta_wall;
    range_wall_ = range_wall;
}

Vector2D SoftSphereCollisionModel::force(const ImmersedBoundaryObject &ibObjP, const ImmersedBoundaryObject &ibObjQ) const
{
    if (ibObjP.shape().type() == Shape2D::CIRCLE && ibObjQ.shape().type() == Shape2D::CIRCLE)
    {
        const Circle &c1 = static_cast<const Circle &>(ibObjP.shape());
        const Circle &c2 = static_cast<const Circle &>(ibObjQ.shape());

        const Vector2D &xp = c1.centroid();
        const Vector2D &xq = c2.centroid();

        const Vector2D &vp = ibObjP.velocity(xp);
        const Vector2D &vq = ibObjP.velocity(xq);

        Scalar r1 = c1.radius();
        Scalar r2 = c2.radius();

        Scalar d = (xp - xq).mag();
        Scalar v_pq = (vp - vq).mag(); 

        Vector2D norm_d = (xp - xq) / d;
        Vector2D f_pq = k_particle_ * std::pow(r1 + r2 + range_particle_ - d, 3/2) * norm_d + eta_particle_ * (vp - vq);

        return d > r1 + r2 + range_particle_ ? Vector2D(0., 0.) : f_pq;
    }
    else
        throw Exception("CollisionModel", "force", "unsupported shape type.");
}

Vector2D SoftSphereCollisionModel::force(const ImmersedBoundaryObject &ibObj, const FiniteVolumeGrid2D &grid) const
{
    Vector2D fc = Vector2D(0., 0.);
    Vector2D fc_zero = Vector2D(0.0, 0.0);

    switch(ibObj.shape().type())
    {
    case Shape2D::CIRCLE:
    {
        const Circle &c = static_cast<const Circle &>(ibObj.shape());
        const Vector2D &xp = c.centroid();
        const Vector2D &vp = ibObj.velocity(xp);
        Scalar r = c.radius();

        for (const FaceGroup &p: grid.patches())
            for (const Face &f: p.itemsCoveredBy(Circle(xp, r + range_wall_)))
            {
                if(!grid.localCells().isInSet(f.lCell()))
                    continue;

                const Vector2D &xq = f.centroid();
                Scalar d = (xp - xq).mag();
                Vector2D norm_d = (xp - xq) / d;
                fc += k_particle_ * std::pow(r + range_particle_ - d, 3/2) * norm_d + eta_particle_ * (vp);
            }
        break;
    }
    default:
        throw Exception("CollisionModel", "force", "unsupported shape type.");
    }

    // return fc;
    return fc_zero;
}
