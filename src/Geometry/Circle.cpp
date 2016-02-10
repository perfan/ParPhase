#include <math.h>

#include "Circle.h"

Circle::Circle(Point2D center, Scalar radius)
    :
      center_(center),
      radius_(radius)
{

}

Scalar Circle::area() const
{
    return M_PI*radius_*radius_;
}

bool Circle::isInside(const Point2D &testPoint) const
{
    return (testPoint - center_).magSqr() < radius_*radius_;
}

bool Circle::isOnEdge(const Point2D &testPoint) const
{
    return fabs((testPoint - center_).mag() - radius_) < 1e-12;
}

Point2D Circle::nearestIntersect(const Point2D &testPoint) const
{
    Vector2D rVec = testPoint - center_;
    return center_ + rVec.unitVec()*radius_;
}

void Circle::operator +=(const Vector2D& translationVec)
{
    center_ += translationVec;
}

void Circle::operator -=(const Vector2D& translationVec)
{
    center_ -= translationVec;
}
