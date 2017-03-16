#include "Source.h"
#include "Tensor2D.h"

namespace source
{

    ScalarFiniteVolumeField div(const VectorFiniteVolumeField &field)
    {
        ScalarFiniteVolumeField divF(field.grid, "divF");

        for (const Cell &cell: field.grid.cellZone("fluid"))
        {
            for (const InteriorLink &nb: cell.neighbours())
                divF(cell) += dot(field(nb.face()), nb.outwardNorm());

            for (const BoundaryLink &bd: cell.boundaries())
                divF(cell) += dot(field(bd.face()), bd.outwardNorm());
        }

        return divF;
    }

    VectorFiniteVolumeField laplacian(const ScalarFiniteVolumeField& gamma, const VectorFiniteVolumeField& field)
    {
        VectorFiniteVolumeField lapF(field.grid, "lapF");

        for(const Cell &cell: field.grid.cellZone("fluid"))
        {
            for(const InteriorLink& nb: cell.neighbours())
            {
                Scalar coeff = gamma(nb.face())*dot(nb.rCellVec(), nb.outwardNorm())/dot(nb.rCellVec(), nb.rCellVec());
                lapF(cell) += coeff*field(nb.face());
            }

            for(const BoundaryLink& bd: cell.boundaries())
            {
                Scalar coeff = gamma(bd.face())*dot(bd.rFaceVec(), bd.outwardNorm())/dot(bd.rFaceVec(), bd.rFaceVec());
                lapF(cell) += coeff*field(bd.face());
            }
        }

        return lapF;
    }

    VectorFiniteVolumeField div(const ScalarFiniteVolumeField& rho, const VectorFiniteVolumeField& u, const VectorFiniteVolumeField& field)
    {
        VectorFiniteVolumeField divF(field.grid, "divF");

        for (const Cell &cell: field.grid.cellZone("fluid"))
        {
            for (const InteriorLink &nb: cell.neighbours())
            {
                const Face& face = nb.face();
                divF(cell) += rho(face)*dot(outer(u(face), field(face)), nb.outwardNorm());
            }

            for (const BoundaryLink &bd: cell.boundaries())
            {
                const Face& face = bd.face();
                divF(cell) += rho(face)*dot(outer(u(face), field(face)), bd.outwardNorm());
            }
        }

        return divF;
    }
}
