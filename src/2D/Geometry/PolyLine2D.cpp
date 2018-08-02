#include "PolyLine2D.h"

Scalar PolyLine2D::length() const
{
    Scalar l = 0.;

    for(size_t i = 1; i < pts_.size(); ++i)
        l += (pts_[i] - pts_[i - 1]).mag();

    return l;
}
