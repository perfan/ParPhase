#include "FractionalStepMultiphase.h"
#include "Cicsam.h"
#include "CrankNicolson.h"
#include "GradientEvaluation.h"
#include "FaceInterpolation.h"
#include "SourceEvaluation.h"
#include "GhostCellImmersedBoundary.h"
#include "TimeDerivative.h"
#include "Source.h"

FractionalStepMultiphase::FractionalStepMultiphase(const Input &input, const Communicator& comm, FiniteVolumeGrid2D& grid)
    :
      FractionalStep(input, comm, grid),
      gamma(addScalarField(input, "gamma")),
      gradGamma(addVectorField("gradGamma")),
      ft(addVectorField("ft")),
      sg(addVectorField("sg")),
      gradRho(addVectorField("gradRho")),
      gammaEqn_(input, comm, gamma, "gammaEqn"),
      surfaceTensionForce_(input, *this)
{
    cicsamBlending_ = input.caseInput().get<Scalar>("Solver.cicsamBlending", 1.);
    rho1_ = input.caseInput().get<Scalar>("Properties.rho1");
    rho2_ = input.caseInput().get<Scalar>("Properties.rho2");
    mu1_ = input.caseInput().get<Scalar>("Properties.mu1");
    mu2_ = input.caseInput().get<Scalar>("Properties.mu2");

    g_ = Vector2D(input.caseInput().get<std::string>("Properties.g"));

    //volumeIntegrators_ = VolumeIntegrator::initVolumeIntegrators(input, *this);
    Scalar sigma = surfaceTensionForce_.sigma();
    capillaryTimeStep_ = std::numeric_limits<Scalar>::infinity();

    for(const Face& face: grid_.interiorFaces())
    {
        Scalar delta = (face.rCell().centroid() - face.lCell().centroid()).mag();
        capillaryTimeStep_ = std::min(capillaryTimeStep_, sqrt(((rho1_ + rho2_)*delta*delta*delta)/(4*M_PI*sigma)));
    }

    capillaryTimeStep_ = comm.min(capillaryTimeStep_);
    comm.printf("CICSAM blending constant (k): %.2f\n", cicsamBlending_);
    comm.printf("Maximum capillary-wave constrained time-step: %.2e\n", capillaryTimeStep_);
}

void FractionalStepMultiphase::initialize()
{
    //- Ensure the computation starts with a valid gamma field
    fv::computeGradient(fv::FACE_TO_CELL, gamma, gradGamma, true);
    ft = surfaceTensionForce_.compute();

    //- Be careful about this, mu and rho must have valid time histories!!!
    computeRho();
    computeMu();
    computeRho();
    computeMu();

    //- Ensure computations start with a valid pressure field
    computeFaceVelocities(1.);
    solvePEqn(1.);

    //- Ensure the computation starts with a valid velocity field
    correctVelocity(1.);
}

Scalar FractionalStepMultiphase::solve(Scalar timeStep)
{
    solveGammaEqn(timeStep); //- Solve a sharp gamma equation
    solveUEqn(timeStep); //- Solve a momentum prediction

    comm_.printf("Max u* = %lf\n", maxVelocity());
    comm_.printf("Max u*_f = %lf\n", maxFaceVelocity());

    solvePEqn(timeStep); //- Solve the pressure equation, using sharp value of rho
    correctVelocity(timeStep);

    comm_.printf("Max u^(n+1) = %lf\n", maxVelocity());
    comm_.printf("Max u^(n+1)_f = %lf\n", maxFaceVelocity());
    comm_.printf("Max Co = %lf\n", maxCourantNumber(timeStep));
    comm_.printf("Max absolute velocity divergence error = %.4e\n", maxDivergenceError());

    return 0.;
}

Scalar FractionalStepMultiphase::computeMaxTimeStep(Scalar maxCo, Scalar prevTimeStep) const
{
    return std::min( //- Both args have already been globalling minimized
                     FractionalStep::computeMaxTimeStep(maxCo, prevTimeStep),
                     capillaryTimeStep_
                     );
}

//- Protected methods
Scalar FractionalStepMultiphase::solveUEqn(Scalar timeStep)
{   
    u.savePreviousTimeStep(timeStep, 1);
    uEqn_ = (fv::ddt(rho, u, timeStep) + fv::div(rho*u, u) + ib::gc(ibObjs(), u)
             == cn::laplacian(mu, u, 1.) - fv::source(gradP - sg.prev(0)));

    checkMassFluxConsistency(timeStep);

    Scalar error = uEqn_.solve();
    grid_.sendMessages(comm_, u);
    computeFaceVelocities(timeStep);

    return error;
}

Scalar FractionalStepMultiphase::solvePEqn(Scalar timeStep)
{
    fv::interpolateFaces(fv::INVERSE_VOLUME, rho);
    pEqn_ = (fv::laplacian(timeStep/rho, p) + ib::gc(ibObjs(), p) ==
             source::div(u));

    Scalar error = pEqn_.solve();
    grid_.sendMessages(comm_, p);

    //- Compute pressure gradient
    gradP.savePreviousTimeStep(timeStep, 1);

    //- Weighted gradients greatly reduce the effect of large pressure differences
    fv::computeInverseWeightedGradient(rho, p, gradP);
    grid_.sendMessages(comm_, gradP);

    return error;
}

Scalar FractionalStepMultiphase::solveGammaEqn(Scalar timeStep)
{
    gamma.savePreviousTimeStep(timeStep, 1);
    cicsam::interpolateFaces(u, gradGamma, gamma, timeStep, cicsamBlending_);

    gammaEqn_ = (fv::ddt(gamma, timeStep) + cicsam::div(u, gamma) + ib::gc(ibObjs(), gamma) == 0.);

    Scalar error = gammaEqn_.solve();

    for(const Cell& cell: grid_.cells())
        gamma(cell) = std::max(std::min(gamma(cell), 1.), 0.);

    grid_.sendMessages(comm_, gamma);

    gamma.setBoundaryFaces();
    fv::computeGradient(fv::FACE_TO_CELL, gamma, gradGamma, true);

    ft.savePreviousTimeStep(timeStep, 1);
    ft = surfaceTensionForce_.compute();

    grid_.sendMessages(comm_, gradGamma); // Must send gradGamma to other processes for CICSAM to work properly (donor cells may be on other processes)

    computeRho();
    computeMu();

    return error;
}

void FractionalStepMultiphase::computeFaceVelocities(Scalar timeStep)
{
    const ScalarFiniteVolumeField& rho0 = rho.prev(0);

    for (const Face &face: u.grid.interiorFaces())
    {
        const Cell &lCell = face.lCell();
        const Cell &rCell = face.rCell();
        //const Scalar g = rCell.volume() / (rCell.volume() + lCell.volume());

        Scalar l1 = dot(face.centroid() - lCell.centroid(), (rCell.centroid() - lCell.centroid()).unitVec());
        Scalar l2 = dot(rCell.centroid() - face.centroid(), (rCell.centroid() - lCell.centroid()).unitVec());
        Scalar g = mu(lCell)/l1/(mu(lCell)/l1 + mu(rCell)/l2);

        u(face) = g * (u(lCell) + timeStep/rho0(lCell)*gradP(lCell))
                + (1. - g) * (u(rCell) + timeStep/rho0(rCell)*gradP(rCell));
    }

    for (const Face &face: u.grid.boundaryFaces())
    {
        const Cell &cell = face.lCell();

        switch (u.boundaryType(face))
        {
        case VectorFiniteVolumeField::FIXED:
            break;

        case VectorFiniteVolumeField::NORMAL_GRADIENT:
            u(face) = u(cell) + timeStep/rho0(cell)*gradP(cell);
            break;

        case VectorFiniteVolumeField::SYMMETRY:
        {
            const Vector2D nWall = face.outwardNorm(cell.centroid());
            u(face) = u(cell) - dot(u(cell), nWall) * nWall / nWall.magSqr();
        }
            break;

            //default:
            //throw Exception("FractionalStep", "computeFaceVelocities", "unrecognized boundary condition type.");
        };
    }
}

void FractionalStepMultiphase::correctVelocity(Scalar timeStep)
{
    const VectorFiniteVolumeField& gradP0 = gradP.prev(0);
    const ScalarFiniteVolumeField& rho0 = rho.prev(0);

    for (const Cell &cell: grid_.cellZone("fluid"))
        u(cell) -= timeStep * (gradP(cell)/rho(cell) - gradP0(cell)/rho0(cell));

    grid_.sendMessages(comm_, u);

    for (const Face &face: grid_.interiorFaces())
        u(face) -= timeStep / rho(face) * gradP(face);

    for (const Face &face: grid_.boundaryFaces())
    {
        switch (u.boundaryType(face))
        {
        case VectorFiniteVolumeField::FIXED:
            break;

        case VectorFiniteVolumeField::SYMMETRY:
        {
            const Vector2D nWall = face.outwardNorm(face.lCell().centroid());
            u(face) = u(face.lCell()) - dot(u(face.lCell()), nWall) * nWall / nWall.magSqr();
        }
            break;

        case VectorFiniteVolumeField::NORMAL_GRADIENT:
            u(face) -= timeStep / rho(face) * gradP(face);
            break;
        };
    }
}

void FractionalStepMultiphase::computeRho()
{
    const ScalarFiniteVolumeField &w = gamma;
    rho.savePreviousTimeStep(0, 1);

    //- Update density
    for(const Cell &cell: grid_.cells())
    {
        Scalar g = std::max(0., std::min(1., w(cell)));
        rho(cell) = (1. - g)*rho1_ + g*rho2_;
    }

    grid_.sendMessages(comm_, rho);

    //fv::computeGradient(fv::GREEN_GAUSS_CELL_CENTERED, rho, gradRho, false);
    fv::computeInverseWeightedGradient(rho, rho, gradRho);

    //- Update the gravitational source term
    sg.savePreviousTimeStep(0., 1);
    for(const Cell& cell: sg.grid.cellZone("fluid"))
        sg(cell) = dot(g_, -cell.centroid())*gradRho(cell);

    for(const Face& face: sg.grid.faces())
        sg(face) = dot(g_, -face.centroid())*gradRho(face);

    for(const Face& face: grid_.faces())
    {
        Scalar g = std::max(0., std::min(1., w(face)));
        rho(face) = (1. - g)*rho1_ + g*rho2_;
    }
}

void FractionalStepMultiphase::computeMu()
{
    const ScalarFiniteVolumeField &w = gamma;
    mu.savePreviousTimeStep(0, 1);

    //- Update viscosity
    for(const Cell &cell: grid_.cells())
    {
        Scalar g = std::max(0., std::min(1., w(cell)));
        mu(cell) = (1. - g)*mu1_ + g*mu2_;
    }

    grid_.sendMessages(comm_, mu);

    for(const Face& face: grid_.faces())
    {
        Scalar g = std::max(0., std::min(1., w(face)));
        mu(face) = (1. - g)*mu1_ + g*mu2_;
    }
}

void FractionalStepMultiphase::checkMassFluxConsistency(Scalar timeStep)
{
    Scalar max = 0.;

    for(const Face& face: grid_.faces())
    {
        Scalar gammaMom = (rho(face) - rho1_)/(rho2_ - rho1_);
        Scalar gammaVof = gamma(face);

        Scalar error = fabs((gammaMom - gammaVof));

        if(error > max)
            max = error;
    }

    comm_.printf("Max mass flux error = %.2lf.\n", max);
}
