![Image of 2D particle interactions by ParPhase](imgs/NC.png)

# About ParPhase

ParPhase is an open-source unstructured CFD solver based on a coupled IBM-VoF algorithm for fluid-solid, solid-solid, and particulate flow simulations. The solver is an extended version of the [Phase](https://github.com/obrienadam/Phase) framework.

# Highlighted Features

* 2D moving Immersed Boundary Method (IBM) coupled with a Volume of Fluid (VoF) method for multiphase fluid simulations.
* Soft-sphere collision model for particle-particle, and particle-surface interactions. 
* Implementation of lubrication force for when the distance between the colliding particles becomes very small.
* Fully-parallelized using the [Message Passing Interface (MPI)](https://www.open-mpi.org/) library