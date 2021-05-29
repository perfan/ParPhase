#include <algorithm>

#include "CollisionModel.h"
#include "FiniteVolumeGrid2D/FiniteVolumeGrid2D.h"
#include "FiniteVolumeGrid2D/Face/FaceGroup.h"

CollisionModel::CollisionModel(Scalar eps_particle, Scalar range_particle, Scalar eps_wall, Scalar range_wall)
{
    eps_particle_ = eps_particle;
    range_particle_ = range_particle;
    eps_wall_ = eps_wall;
    range_wall_ = range_wall;
}

Vector2D CollisionModel::force(const ImmersedBoundaryObject &ibObjP, const ImmersedBoundaryObject &ibObjQ) const
{
    if (ibObjP.shape().type() == Shape2D::CIRCLE && ibObjQ.shape().type() == Shape2D::CIRCLE)
    {
        const Circle &c1 = static_cast<const Circle &>(ibObjP.shape());
        const Circle &c2 = static_cast<const Circle &>(ibObjQ.shape());

        const Vector2D &xp = c1.centroid();
        const Vector2D &xq = c2.centroid();

        Scalar r1 = c1.radius();
        Scalar r2 = c2.radius();

        Scalar d = (xp - xq).mag();

        return d > r1 + r2 + range_particle_ ? Vector2D(0., 0.) : (xp - xq) / eps_particle_ * std::pow(r1 + r2 + range_particle_ - d, 2);
    }
    else
        throw Exception("CollisionModel", "force", "unsupported shape type.");
}

Vector2D CollisionModel::force(const ImmersedBoundaryObject &ibObj, const FiniteVolumeGrid2D &grid) const
{
    Vector2D fc = Vector2D(0., 0.);
    Vector2D fc_zero = Vector2D(0.0, 0.0);

    switch(ibObj.shape().type())
    {
    case Shape2D::CIRCLE:
    {
        const Circle &c = static_cast<const Circle &>(ibObj.shape());
        const Vector2D &xp = c.centroid();
        Scalar r = c.radius();

        for (const FaceGroup &p: grid.patches())
            for (const Face &f: p.itemsCoveredBy(Circle(xp, r + range_wall_)))
            {
                if(!grid.localCells().isInSet(f.lCell()))
                    continue;

                const Vector2D &xq = f.centroid();
                Scalar d = (xp - xq).mag();

                fc += (xp - xq) / eps_wall_ * pow(r + range_wall_ - d, 2);
            }
        break;
    }
    default:
        throw Exception("CollisionModel", "force", "unsupported shape type.");
    }

    // return fc;
    return fc_zero;
}
