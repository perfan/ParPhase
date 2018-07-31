#include "Math/TrilinosAmesosSparseMatrixSolver.h"
#include "Geometry/Tensor2D.h"

#include "DirectForcingImmersedBoundary.h"

DirectForcingImmersedBoundary::DirectForcingImmersedBoundary(const Input &input,
                                                             const std::shared_ptr<const FiniteVolumeGrid2D> &grid,
                                                             const std::shared_ptr<CellGroup> &domainCells)
    :
      ImmersedBoundary(input, grid, domainCells)
{
    
}

void DirectForcingImmersedBoundary::updateCells()
{
    for(auto &ibObj: ibObjs_)
        ibObj->clear();

    ibCells_.clear();
    solidCells_.clear();

    CellGroup solidCells, ibCells;

    for(auto &ibObj: ibObjs_)
    {
        solidCells = ibObj->cellsWithin(*domainCells_);
        ibCells = ibObj->outerPerimeterCells(solidCells.begin(), solidCells.end(), false);
        ibObj->setIbCells(ibCells);
        ibObj->setSolidCells(solidCells);

        ibCells_.add(ibCells);
        solidCells_.add(solidCells);
    }

    cellStatus_->fill(FLUID_CELLS);

    for(const Cell& cell: ibCells_)
        (*cellStatus_)(cell) = IB_CELLS;

    for(const Cell& cell: solidCells_)
        (*cellStatus_)(cell) = SOLID_CELLS;

    grid_->sendMessages(*cellStatus_);
}

void DirectForcingImmersedBoundary::computeForcingTerm(const VectorFiniteVolumeField &u, Scalar timeStep, VectorFiniteVolumeField &fib) const
{
    FiniteVolumeEquation<Vector2D> eqn(fib);
    eqn.setSparseSolver(std::make_shared<TrilinosAmesosSparseMatrixSolver>(grid_->comm()));

    auto ibCells = grid_->globalCellGroup(ibCells_);
    auto solidCells = grid_->globalCellGroup(solidCells_);

    for(const Cell& cell: grid_->localCells())
    {
        if(ibCells.isInSet(cell))
        {
            std::vector<const Cell*> stCells;
            std::vector<std::pair<Point2D, Vector2D>> compatPts;

            for(const CellLink &nb: cell.cellLinks())
            {
                if(!solidCells.isInSet(nb.cell()))
                {
                    stCells.push_back(&nb.cell());

                    if(ibCells.isInSet(nb.cell()))
                    {
                        auto ibObj = nearestIbObj(nb.cell().centroid());
                        auto bp = ibObj->nearestIntersect(nb.cell().centroid());
                        auto bu = ibObj->velocity(bp);
                        compatPts.push_back(std::make_pair(bp, bu));
                    }
                }
            }

            auto ibObj = nearestIbObj(cell.centroid());
            auto bp = ibObj->nearestIntersect(cell.centroid());
            auto bu = ibObj->velocity(bp);

            compatPts.push_back(std::make_pair(bp, bu));

            if(stCells.size() + compatPts.size() < 6)
            {
                throw Exception("DirectForcingImmersedBoundary",
                                "computeForcingTerm",
                                "not enough cells to perform velocity interpolation. Cell id = "
                                + std::to_string(cell.globalId()) + ", proc = " + std::to_string(grid_->comm().rank()));
            }

            Matrix A(stCells.size() + compatPts.size(), 6);

            for(int i = 0; i < stCells.size(); ++i)
            {
                Point2D x = stCells[i]->centroid();
                A.setRow(i, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
            }

            for(int i = 0; i < compatPts.size(); ++i)
            {
                Point2D x = compatPts[i].first;
                A.setRow(i + stCells.size(), {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
            }

            Point2D x = cell.centroid();

            Matrix beta = Matrix(1, 6, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.}) * pseudoInverse(A);

            eqn.add(cell, cell, -timeStep);

            for(int i = 0; i < stCells.size(); ++i)
            {
                eqn.add(cell, *stCells[i], beta(0, i) * timeStep);
                eqn.addSource(cell, beta(0, i) * u(*stCells[i]));
            }

            for(int i = 0; i < compatPts.size(); ++i)
                eqn.addSource(cell, beta(0, i + stCells.size()) * compatPts[i].second);

            eqn.addSource(cell, -u(cell));
        }
        else if (solidCells.isInSet(cell))
        {
            eqn.set(cell, cell, -1.);
            eqn.setSource(cell, (ibObj(cell.centroid())->velocity(cell.centroid()) - u(cell)) / timeStep);
        }
        else
        {
            eqn.set(cell, cell, -1.);
            eqn.setSource(cell, 0.);
        }
    }

    eqn.solve();
    grid_->sendMessages(fib);
}

void DirectForcingImmersedBoundary::computeForcingTerm(const ScalarFiniteVolumeField &rho,
                                                       const VectorFiniteVolumeField &u,
                                                       Scalar timeStep,
                                                       VectorFiniteVolumeField &fib) const
{
    FiniteVolumeEquation<Vector2D> eqn(fib);
    eqn.setSparseSolver(std::make_shared<TrilinosAmesosSparseMatrixSolver>(grid_->comm()));

    auto ibCells = grid_->globalCellGroup(ibCells_);
    auto solidCells = grid_->globalCellGroup(solidCells_);

    for(const Cell& cell: grid_->localCells())
    {
        if(ibCells.isInSet(cell))
        {
            std::vector<const Cell*> stCells;
            std::vector<std::pair<Point2D, Vector2D>> compatPts;

            for(const CellLink &nb: cell.cellLinks())
            {
                if(!solidCells.isInSet(nb.cell()))
                {
                    stCells.push_back(&nb.cell());

                    if(ibCells.isInSet(nb.cell()))
                    {
                        auto ibObj = nearestIbObj(nb.cell().centroid());
                        auto bp = ibObj->nearestIntersect(nb.cell().centroid());
                        auto bu = ibObj->velocity(bp);
                        compatPts.push_back(std::make_pair(bp, bu));
                    }
                }
            }

            auto ibObj = nearestIbObj(cell.centroid());
            auto bp = ibObj->nearestIntersect(cell.centroid());
            auto bu = ibObj->velocity(bp);

            compatPts.push_back(std::make_pair(bp, bu));

            if(stCells.size() + compatPts.size() < 6)
            {
                throw Exception("DirectForcingImmersedBoundary",
                                "computeForcingTerm",
                                "not enough cells to perform velocity interpolation. Cell id = "
                                + std::to_string(cell.globalId()) + ", proc = " + std::to_string(grid_->comm().rank()));
            }

            Matrix A(stCells.size() + compatPts.size(), 6);

            for(int i = 0; i < stCells.size(); ++i)
            {
                Point2D x = stCells[i]->centroid();
                A.setRow(i, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
            }

            for(int i = 0; i < compatPts.size(); ++i)
            {
                Point2D x = compatPts[i].first;
                A.setRow(i + stCells.size(), {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
            }

            Point2D x = cell.centroid();

            Matrix beta = Matrix(1, 6, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.}) * pseudoInverse(A);

            eqn.add(cell, cell, -timeStep / rho(cell));

            for(int i = 0; i < stCells.size(); ++i)
            {
                eqn.add(cell, *stCells[i], beta(0, i) * timeStep / rho(*stCells[i]));
                eqn.addSource(cell, beta(0, i) * u(*stCells[i]));
            }

            for(int i = 0; i < compatPts.size(); ++i)
                eqn.addSource(cell, beta(0, i + stCells.size()) * compatPts[i].second);

            eqn.addSource(cell, -u(cell));
        }
        else if (solidCells.isInSet(cell))
        {
            eqn.set(cell, cell, -timeStep / rho(cell));
            eqn.setSource(cell, ibObj(cell.centroid())->velocity(cell.centroid()) - u(cell));
        }
        else
        {
            eqn.set(cell, cell, -1.);
            eqn.setSource(cell, 0.);
        }
    }

    eqn.solve();
    grid_->sendMessages(fib);
}

void DirectForcingImmersedBoundary::computeForce(Scalar rho,
                                                 Scalar mu,
                                                 const VectorFiniteVolumeField &u,
                                                 const ScalarFiniteVolumeField &p,
                                                 const Vector2D &g)
{
    //    Equation eqn;
    
    //    auto nLocalIbCells = grid_->comm().allGather(ibCells_.size());
    //    auto colOwnershipRange = std::make_pair(6 * std::accumulate(nLocalIbCells.begin(), nLocalIbCells.begin() + grid_->comm().rank(), 0),
    //                                            6 * std::accumulate(nLocalIbCells.begin(), nLocalIbCells.begin() + grid_->comm().rank() + 1, 0));
    
    //    auto colMap = std::vector<Index>(grid_->cells().size(), -1);
    
    //    Index ibCellId = 0;
    //    for(const Cell& cell: ibCells_)
    //        colMap[cell.id()] = colOwnershipRange.first + 6 * ibCellId++;
    
    //    grid_->sendMessages(colMap);
    
    //    auto ibCells = grid_->globalCellGroup(ibCells_);
    //    auto solidCells = grid_->globalCellGroup(solidCells_);

    //    std::vector<const Cell*> stCells;
    //    std::vector<std::pair<const Cell*, Point2D>> compatPts;
    
    //    Index row = 0;

    //    for(const Cell &cell: ibCells_)
    //    {
    //        stCells.clear();
    //        compatPts.clear();

    //        for(const CellLink &nb: cell.cellLinks())
    //            if(!solidCells.isInGroup(nb.cell()))
    //            {
    //                stCells.push_back(&nb.cell());

    //                if(ibCells.isInGroup(nb.cell()))
    //                    compatPts.push_back(std::make_pair(&nb.cell(), nearestIntersect(nb.cell().centroid())));
    //            }

    //        eqn.setRank(eqn.rank() + stCells.size() + compatPts.size() + 1);

    //        Index col = colMap[cell.id()];

    //        for(const auto &cellPtr: stCells)
    //        {
    //            Point2D x = cellPtr->centroid();

    //            eqn.setCoeffs(row,
    //            {col, col + 1, col + 2, col + 3, col + 4, col + 5},
    //            {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});

    //            eqn.setRhs(row++, -p(*cellPtr));
    //        }

    //        for(const auto &compatPt: compatPts)
    //        {
    //            const Cell &cell = *compatPt.first;
    //            Point2D x = compatPt.second;

    //            eqn.setCoeffs(row,
    //            {col, col + 1, col + 2, col + 3, col + 4, col + 5},
    //            {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});

    //            Index col2 = colMap[cell.id()];

    //            eqn.setCoeffs(row++,
    //            {col2, col2 + 1, col2 + 2, col2 + 3, col2 + 4, col2 + 5},
    //            {-x.x * x.x, -x.y * x.y, -x.x * x.y, -x.x, -x.y, -1.});
    //        }

    //        Point2D x = cell.centroid();
    //        Vector2D n = nearestEdgeNormal(x).unitVec();

    //        eqn.setCoeffs(row,
    //        {col, col + 1, col + 2, col + 3, col + 4, col + 5},
    //        {2. * x.x * n.x, 2. * x.y * n.y, x.y * n.x + x.x * n.y, n.x, n.y, 0.});

    //        eqn.setRhs(row++, rho * dot(acceleration(x), n));
    //    }

    //    eqn.setSparseSolver(std::make_shared<TrilinosAmesosSparseMatrixSolver>(grid_->comm(), Tpetra::DynamicProfile));
    //    eqn.setRank(eqn.rank(), 6 * ibCells_.size());
    //    eqn.solveLeastSquares();

    //    std::vector<std::tuple<Point2D, Scalar, Tensor2D>> stresses;

    //    for(int i = 0; i < 6 * ibCells_.size(); i += 6)
    //    {
    //        Scalar a = eqn.x(i);
    //        Scalar b = eqn.x(i + 1);
    //        Scalar c = eqn.x(i + 2);
    //        Scalar d = eqn.x(i + 3);
    //        Scalar e = eqn.x(i + 4);
    //        Scalar f = eqn.x(i + 5);

    //        Point2D x = nearestIntersect(ibCells_[i / 6].centroid());

    //        Scalar pb = a * x.x * x.x + b * x.y * x.y + c * x.x * x.y + d * x.x + e * x.y + f + rho * dot(x, g);

    //        std::vector<std::pair<Point2D, Vector2D>> pts;
    //        pts.reserve(8);

    //        const Cell &cell = ibCells_[i / 6];

    //        for(const CellLink &nb: cell.cellLinks())
    //            if(!solidCells.isInGroup(nb.cell()))
    //            {
    //                pts.push_back(std::make_pair(nb.cell().centroid(), u(nb.cell())));

    //                if(ibCells.isInGroup(nb.cell()))
    //                {
    //                    auto bp = nearestIntersect(nb.cell().centroid());
    //                    pts.push_back(std::make_pair(bp, velocity(bp)));
    //                }
    //            }

    //        auto bp = nearestIntersect(cell.centroid());
    //        pts.push_back(std::make_pair(bp, velocity(bp)));

    //        Matrix A(pts.size(), 6), rhs(pts.size(), 2);

    //        for(int i = 0; i < pts.size(); ++i)
    //        {
    //            Point2D x = pts[i].first;
    //            Point2D u = pts[i].second;

    //            A.setRow(i, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
    //            rhs.setRow(i, {u.x, u.y});
    //        }

    //        auto coeffs = solve(A, rhs);
    //        auto derivs = Matrix(2, 6, {
    //                                 2. * bp.x, 0., bp.y, 1., 0., 0.,
    //                                 0., 2. * bp.y, bp.x, 0., 1., 0.
    //                             }) * coeffs;

    //        //- Then tensor is tranposed here
    //        auto tau = Tensor2D(derivs(0, 0), derivs(1, 0), derivs(0, 1), derivs(1, 1));

    //        stresses.push_back(std::make_tuple(x, pb, mu * (tau + tau.transpose())));
    //    }

    //    stresses = grid_->comm().gatherv(grid_->comm().mainProcNo(), stresses);

    //    if(grid_->comm().isMainProc())
    //    {

    //        std::sort(stresses.begin(), stresses.end(), [this](std::tuple<Point2D, Scalar, Tensor2D> &lhs,
    //                  std::tuple<Point2D, Scalar, Tensor2D> &rhs)
    //        {
    //            return (std::get<0>(lhs) - shape_->centroid()).angle()
    //                    < (std::get<0>(rhs) - shape_->centroid()).angle();
    //        });

    //        Vector2D fShear(0., 0.), fPressure(0., 0.);

    //        for(int i = 0; i < stresses.size(); ++i)
    //        {
    //            auto qpa = stresses[i];
    //            auto qpb = stresses[(i + 1) % stresses.size()];

    //            auto ptA = std::get<0>(qpa);
    //            auto ptB = std::get<0>(qpb);
    //            auto pA = std::get<1>(qpa);
    //            auto pB = std::get<1>(qpb);
    //            auto tauA = std::get<2>(qpa);
    //            auto tauB = std::get<2>(qpb);

    //            fPressure += (pA + pB) / 2. * (ptA - ptB).normalVec();
    //            fShear += dot((tauA + tauB) / 2., (ptB - ptA).normalVec());
    //        }

    //        force_ = fPressure + fShear + this->rho * g * shape_->area();

    //        std::cout << "Pressure force = " << fPressure << "\n"
    //                  << "Shear force = " << fShear << "\n"
    //                  << "Weight = " << this->rho * g * shape_->area() << "\n"
    //                  << "Net force = " << force_ << "\n";
    //    }

    //    force_ = grid_->comm().broadcast(grid_->comm().mainProcNo(), force_);
}

//void DirectForcingImmersedBoundary::computeForce(const ScalarFiniteVolumeField &rho,
//                                                       const ScalarFiniteVolumeField &mu,
//                                                       const VectorFiniteVolumeField &u,
//                                                       const ScalarFiniteVolumeField &p,
//                                                       const ScalarFiniteVolumeField &gamma,
//                                                       const SurfaceTensionForce &ft,
//                                                       const Vector2D &g)
//{
//    Equation eqn;

//    auto nLocalIbCells = grid_->comm().allGather(ibCells_.size());
//    auto colOwnershipRange = std::make_pair(6 * std::accumulate(nLocalIbCells.begin(), nLocalIbCells.begin() + grid_->comm().rank(), 0),
//                                            6 * std::accumulate(nLocalIbCells.begin(), nLocalIbCells.begin() + grid_->comm().rank() + 1, 0));

//    auto colMap = std::vector<Index>(grid_->cells().size(), -1);

//    Index ibCellId = 0;
//    for(const Cell& cell: ibCells_)
//        colMap[cell.id()] = colOwnershipRange.first + 6 * ibCellId++;

//    grid_->sendMessages(colMap);

//    auto ibCells = grid_->globalCellGroup(ibCells_);
//    auto solidCells = grid_->globalCellGroup(solidCells_);

//    std::vector<const Cell*> stCells;
//    std::vector<std::pair<const Cell*, Point2D>> compatPts;

//    Index row = 0;

//    for(const Cell &cell: ibCells_)
//    {
//        stCells.clear();
//        compatPts.clear();

//        for(const CellLink &nb: cell.cellLinks())
//            if(!solidCells.isInGroup(nb.cell()))
//            {
//                stCells.push_back(&nb.cell());

//                if(ibCells.isInGroup(nb.cell()))
//                    compatPts.push_back(std::make_pair(&nb.cell(), nearestIntersect(nb.cell().centroid())));
//            }

//        eqn.setRank(eqn.rank() + stCells.size() + compatPts.size() + 1);

//        Index col = colMap[cell.id()];

//        for(const auto &cellPtr: stCells)
//        {
//            Point2D x = cellPtr->centroid();

//            eqn.setCoeffs(row,
//            {col, col + 1, col + 2, col + 3, col + 4, col + 5},
//            {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});

//            eqn.setRhs(row++, -(p(*cellPtr) + rho(*cellPtr) * dot(g, cellPtr->centroid())));
//        }

//        for(const auto &compatPt: compatPts)
//        {
//            const Cell &cell = *compatPt.first;
//            Point2D x = compatPt.second;

//            eqn.setCoeffs(row,
//            {col, col + 1, col + 2, col + 3, col + 4, col + 5},
//            {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});

//            Index col2 = colMap[cell.id()];

//            eqn.setCoeffs(row++,
//            {col2, col2 + 1, col2 + 2, col2 + 3, col2 + 4, col2 + 5},
//            {-x.x * x.x, -x.y * x.y, -x.x * x.y, -x.x, -x.y, -1.});
//        }

//        Point2D x = cell.centroid();
//        Vector2D n = nearestEdgeNormal(x).unitVec();

//        eqn.setCoeffs(row,
//        {col, col + 1, col + 2, col + 3, col + 4, col + 5},
//        {2. * x.x * n.x, 2. * x.y * n.y, x.y * n.x + x.x * n.y, n.x, n.y, 0.});

//        eqn.setRhs(row++, rho(cell) * (dot(acceleration(x) - g, n)));
//    }

//    eqn.setSparseSolver(std::make_shared<TrilinosAmesosSparseMatrixSolver>(grid_->comm(), Tpetra::DynamicProfile));
//    eqn.setRank(eqn.rank(), 6 * ibCells_.size());
//    eqn.solveLeastSquares();

//    std::vector<std::tuple<Point2D, Scalar, Tensor2D>> stresses;

//    for(int i = 0; i < 6 * ibCells_.size(); i += 6)
//    {
//        Scalar a = eqn.x(i);
//        Scalar b = eqn.x(i + 1);
//        Scalar c = eqn.x(i + 2);
//        Scalar d = eqn.x(i + 3);
//        Scalar e = eqn.x(i + 4);
//        Scalar f = eqn.x(i + 5);

//        Point2D x = nearestIntersect(ibCells_[i / 6].centroid());

//        Scalar pb = a * x.x * x.x + b * x.y * x.y + c * x.x * x.y + d * x.x + e * x.y + f;

//        std::vector<std::pair<Point2D, Vector2D>> pts;
//        pts.reserve(8);

//        const Cell &cell = ibCells_[i / 6];

//        for(const CellLink &nb: cell.cellLinks())
//            if(!solidCells.isInGroup(nb.cell()))
//            {
//                pts.push_back(std::make_pair(nb.cell().centroid(), u(nb.cell())));

//                if(ibCells.isInGroup(nb.cell()))
//                {
//                    auto bp = nearestIntersect(nb.cell().centroid());
//                    pts.push_back(std::make_pair(bp, velocity(bp)));
//                }
//            }

//        auto bp = nearestIntersect(cell.centroid());
//        pts.push_back(std::make_pair(bp, velocity(bp)));

//        Matrix A(pts.size(), 6), rhs(pts.size(), 2);

//        for(int i = 0; i < pts.size(); ++i)
//        {
//            Point2D x = pts[i].first;
//            Point2D u = pts[i].second;

//            A.setRow(i, {x.x * x.x, x.y * x.y, x.x * x.y, x.x, x.y, 1.});
//            rhs.setRow(i, {u.x, u.y});
//        }

//        auto coeffs = solve(A, rhs);
//        auto derivs = Matrix(2, 6, {
//                                 2. * bp.x, 0., bp.y, 1., 0., 0.,
//                                 0., 2. * bp.y, bp.x, 0., 1., 0.
//                             }) * coeffs;

//        //- Then tensor is tranposed here
//        auto tau = Tensor2D(derivs(0, 0), derivs(1, 0), derivs(0, 1), derivs(1, 1));

//        stresses.push_back(std::make_tuple(x, pb, mu(cell) * (tau + tau.transpose())));
//    }

//    stresses = grid_->comm().gatherv(grid_->comm().mainProcNo(), stresses);

//    if(grid_->comm().isMainProc())
//    {

//        std::sort(stresses.begin(), stresses.end(), [this](std::tuple<Point2D, Scalar, Tensor2D> &lhs,
//                  std::tuple<Point2D, Scalar, Tensor2D> &rhs)
//        {
//            return (std::get<0>(lhs) - shape_->centroid()).angle()
//                    < (std::get<0>(rhs) - shape_->centroid()).angle();
//        });

//        Vector2D fShear(0., 0.), fPressure(0., 0.);

//        for(int i = 0; i < stresses.size(); ++i)
//        {
//            auto qpa = stresses[i];
//            auto qpb = stresses[(i + 1) % stresses.size()];

//            auto ptA = std::get<0>(qpa);
//            auto ptB = std::get<0>(qpb);
//            auto pA = std::get<1>(qpa);
//            auto pB = std::get<1>(qpb);
//            auto tauA = std::get<2>(qpa);
//            auto tauB = std::get<2>(qpb);

//            fPressure += (pA + pB) / 2. * (ptA - ptB).normalVec();
//            fShear += dot((tauA + tauB) / 2., (ptB - ptA).normalVec());
//        }

//        force_ = fPressure + fShear + this->rho * g * shape_->area();

//        std::cout << "Pressure force = " << fPressure << "\n"
//                  << "Shear force = " << fShear << "\n"
//                  << "Weight = " << this->rho * g * shape_->area() << "\n"
//                  << "Net force = " << force_ << "\n";
//    }

//    force_ = grid_->comm().broadcast(grid_->comm().mainProcNo(), force_);
//}