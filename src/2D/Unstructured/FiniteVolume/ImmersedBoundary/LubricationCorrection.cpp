#include <algorithm>
#include <math.h>

#include "LubricationCorrection.h"
#include "FiniteVolumeGrid2D/FiniteVolumeGrid2D.h"
#include "FiniteVolumeGrid2D/Face/FaceGroup.h"

LubricationCorrection::LubricationCorrection(Scalar mu, Scalar range_particle, Scalar range_wall)
{
    mu_ = mu;
    range_particle_ = range_particle;
    range_wall_ = range_wall;
}

Vector2D LubricationCorrection::force(const ImmersedBoundaryObject &ibObjP, const ImmersedBoundaryObject &ibObjQ) const
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
        Scalar r_eff = (r1 * r2)/(r1 + r2);
        Scalar d = (xp - xq).mag();
        Scalar v_pq = (vp - vq).mag(); 
        Vector2D norm_d = (xp - xq) / d;
        
        Scalar delta = r1 + r2 + range_particle_ - d;
        Scalar epsilon = delta / r_eff;
        Scalar epsilon_0 = range_particle_ / r_eff;
        Scalar lambda = 1/(2 * epsilon) - (9/20) * log (epsilon) - (3/56) * epsilon * log (epsilon);
        Scalar lambda_0 = 1/(2 * epsilon_0) - (9/20) * log (epsilon_0) - (3/56) * epsilon_0 * log (epsilon_0);

        Vector2D f_lubrication = 6 * M_PI * mu_ * r_eff * (vp - vq) * (lambda - lambda_0);

        return d > r1 + r2 + range_particle_ ? Vector2D(0., 0.) : f_lubrication;
    }
    else
        throw Exception("CollisionModel", "force", "unsupported shape type.");
}

Vector2D LubricationCorrection::force(const ImmersedBoundaryObject &ibObj, const FiniteVolumeGrid2D &grid) const
{
    Vector2D f_lubrication = Vector2D(0., 0.);
    Vector2D f_zero = Vector2D(0.0, 0.0);

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
                Scalar delta = r + range_particle_ - d;
                Scalar epsilon = delta / r;
                Scalar epsilon_0 = range_particle_ / r;
                Scalar lambda = 1/(epsilon) - (1/5) * log (epsilon) - (1/21) * epsilon * log (epsilon);
                Scalar lambda_0 = 1/(epsilon_0) - (1/5) * log (epsilon_0) - (1/21) * epsilon_0 * log (epsilon_0);

                f_lubrication = 6 * M_PI * mu_ * r * (vp) * (lambda - lambda_0);
                f_lubrication += 6 * M_PI * mu_ * r * (vp) * (lambda - lambda_0);
            }
        break;
    }
    default:
        throw Exception("CollisionModel", "force", "unsupported shape type.");
    }

    // return f_lubrication;
    return f_zero;
}
