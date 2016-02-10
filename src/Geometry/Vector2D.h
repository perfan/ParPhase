#ifndef VECTOR_2D_H
#define VECTOR_2D_H

#include <ostream>

#include "Types.h"

class Vector2D
{
public:

    Vector2D(Scalar x = 0., Scalar y = 0.);

    Scalar magSqr() const;
    Scalar mag() const;

    Vector2D unitVec() const;
    Vector2D normalVec() const;

    // Operators
    Scalar& operator()(int component);
    Scalar operator ()(int component) const;
    Vector2D& operator +=(const Vector2D& other);
    Vector2D& operator -=(const Vector2D& other);
    Vector2D& operator *=(Scalar other);
    Vector2D& operator /=(Scalar other);

    Scalar x, y;

private:
};

std::ostream& operator<<(std::ostream& os, const Vector2D& vec);
Vector2D operator+(Vector2D lhs, const Vector2D& rhs);
Vector2D operator-(Vector2D lhs, const Vector2D& rhs);
Vector2D operator*(Vector2D lhs, Scalar rhs);
Vector2D operator*(Scalar lhs, Vector2D rhs);
Vector2D operator/(Vector2D lhs, Scalar rhs);

Scalar dot(const Vector2D& u, const Vector2D& v);
Scalar cross(const Vector2D& u, const Vector2D& v);

#endif
