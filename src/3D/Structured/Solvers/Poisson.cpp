#include "Poisson.h"
#include "Structured/FiniteVolume/Discretization/Laplacian.h"

Poisson::Poisson(const Input &input, const std::shared_ptr<const StructuredGrid3D> &grid)
    :
      Solver(input, grid),
      _phi(*addField<Scalar>("phi", input)),
      _phiEqn("phiEqn", input, _phi)
{

}

Scalar Poisson::solve(Scalar timeStep)
{
    _phiEqn = (fv::lap(_phi) == 0.);
    Scalar error = _phiEqn.solve();

    return error;
}
